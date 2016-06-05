/**
 * Copyright (c) 2016 zScale Technology GmbH <legal@zscale.io>
 * Authors:
 *   - Paul Asmuth <paul@zscale.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#include "eventql/eventql.h"
#include <eventql/io/cstable/CSTableWriter.h>
#include <eventql/io/cstable/columns/column_writer_uint.h>
#include <eventql/io/cstable/columns/v1/BooleanColumnWriter.h>
#include <eventql/io/cstable/columns/v1/BitPackedIntColumnWriter.h>
#include <eventql/io/cstable/columns/v1/UInt32ColumnWriter.h>
#include <eventql/io/cstable/columns/v1/UInt64ColumnWriter.h>
#include <eventql/io/cstable/columns/v1/LEB128ColumnWriter.h>
#include <eventql/io/cstable/columns/v1/DoubleColumnWriter.h>
#include <eventql/io/cstable/columns/v1/StringColumnWriter.h>
#include <eventql/util/SHA1.h>
#include <eventql/util/io/fileutil.h>
#include <eventql/util/option.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace cstable {

RefPtr<CSTableWriter> CSTableWriter::createFile(
    const String& filename,
    const TableSchema& schema,
    Option<RefPtr<LockRef>> lockref /* = None<RefPtr<LockRef>>() */) {
  return createFile(
      filename,
      BinaryFormatVersion::v0_2_0,
      schema,
      lockref);
}

RefPtr<CSTableWriter> CSTableWriter::createFile(
    const String& filename,
    BinaryFormatVersion version,
    const TableSchema& schema,
    Option<RefPtr<LockRef>> lockref /* = None<RefPtr<LockRef>>() */) {
  auto file = File::openFile(filename, File::O_WRITE | File::O_CREATE);
  auto file_os = FileOutputStream::fromFileDescriptor(file.fd());
  ScopedPtr<PageManager> page_mgr;

  FileHeader header;
  header.schema = mkRef(new TableSchema(schema));
  header.columns = header.schema->flatColumns();

  size_t header_size ;
  switch (version) {
    case BinaryFormatVersion::v0_1_0:
      break;
    case BinaryFormatVersion::v0_2_0: {
      auto header_size = cstable::v0_2_0::writeHeader(header, file_os.get());
      page_mgr.reset(new PageManager(file.fd(), header_size, {}));
      break;
    }
  }

  return new CSTableWriter(
      version,
      header.schema,
      std::move(page_mgr),
      header.columns,
      file.releaseFD());
}

RefPtr<CSTableWriter> CSTableWriter::reopenFile(
    const String& filename,
    Option<RefPtr<LockRef>> lockref /* = None<RefPtr<LockRef>>() */) {
  RAISE(kNotYetImplementedError);
}

static RefPtr<ColumnWriter> openColumnV1(const ColumnConfig& c) {
  auto rmax = c.rlevel_max;
  auto dmax = c.dlevel_max;

  switch (c.storage_type) {
    case ColumnEncoding::BOOLEAN_BITPACKED:
      return new v1::BooleanColumnWriter(rmax, dmax);
    case ColumnEncoding::UINT32_BITPACKED:
      return new v1::BitPackedIntColumnWriter(rmax, dmax);
    case ColumnEncoding::UINT32_PLAIN:
      return new v1::UInt32ColumnWriter(rmax, dmax, c.logical_type);
    case ColumnEncoding::UINT64_PLAIN:
      return new v1::UInt64ColumnWriter(rmax, dmax, c.logical_type);
    case ColumnEncoding::UINT64_LEB128:
      return new v1::LEB128ColumnWriter(rmax, dmax, c.logical_type);
    case ColumnEncoding::FLOAT_IEEE754:
      return new v1::DoubleColumnWriter(rmax, dmax);
    case ColumnEncoding::STRING_PLAIN:
      return new v1::StringColumnWriter(rmax, dmax);
    default:
      RAISEF(
          kRuntimeError,
          "unsupported column type: $0",
          (uint32_t) c.storage_type);
  }
}

static RefPtr<ColumnWriter> openColumnV2(
    const ColumnConfig& c,
    PageManager* page_mgr) {
  switch (c.logical_type) {
    //case ColumnType::BOOLEAN:
    //  return new BooleanColumnWriter(c, page_mgr, page_idx);
    case ColumnType::UNSIGNED_INT:
      return new UnsignedIntColumnWriter(c, page_mgr);
    //case ColumnType::SIGNED_INT:
    //  return new SignedIntColumnWriter(c, page_mgr, page_idx);
    //case ColumnType::STRING:
    //  return new StringColumnWriter(c, page_mgr, page_idx);
    //case ColumnType::FLOAT:
    //  return new FloatColumnWriter(c, page_mgr, page_idx);
    //case ColumnType::DATETIME:
    //  return new DateTimeColumnWriter(c, page_mgr, page_idx);
  }
}

CSTableWriter::CSTableWriter(
    BinaryFormatVersion version,
    RefPtr<TableSchema> schema,
    ScopedPtr<PageManager> page_mgr,
    Vector<ColumnConfig> columns,
    int fd) :
    version_(version),
    schema_(std::move(schema)),
    page_mgr_(std::move(page_mgr)),
    columns_(columns),
    fd_(fd),
    current_txid_(0),
    num_rows_(0) {

  // open column writers
  for (size_t i = 0; i < columns_.size(); ++i) {
    RefPtr<ColumnWriter> writer;
    switch (version_) {
      case BinaryFormatVersion::v0_1_0:
        writer = openColumnV1(columns_[i]);
        break;
      case BinaryFormatVersion::v0_2_0:
        writer = openColumnV2(columns_[i], page_mgr_.get());
        break;
    }

    column_writers_.emplace_back(writer);
    column_writers_by_name_.emplace(columns_[i].column_name, writer);
  }
}

CSTableWriter::~CSTableWriter() {
  if (fd_ > 0) {
    close(fd_);
  }
}

void CSTableWriter::addRow() {
  num_rows_++;
}

void CSTableWriter::addRows(size_t num_rows) {
  num_rows_ += num_rows;
}

void CSTableWriter::commit() {
  switch (version_) {
    case BinaryFormatVersion::v0_1_0:
      return commitV1();
    case BinaryFormatVersion::v0_2_0:
      return commitV2();
  }
}

void CSTableWriter::commitV1() {
  for (size_t i = 0; i < columns_.size(); ++i) {
    auto writer = column_writers_[i].asInstanceOf<v1::ColumnWriter>();
    writer->commit();
    columns_[i].body_size = writer->bodySize();
  }

  util::BinaryMessageWriter header;
  header.appendUInt32(cstable::v0_1_0::kMagicBytesUInt32);
  header.appendUInt16(cstable::v0_1_0::kVersion);
  header.appendUInt64(0); // flags
  header.appendUInt64(num_rows_);
  header.appendUInt32(columns_.size());

  size_t offset = header.size();
  for (const auto& col : columns_) {
    offset += 32 + col.column_name.size();
  }

  for (auto& col : columns_) {
    col.body_offset = offset;
    offset += col.body_size;

    header.appendUInt32((uint32_t) col.storage_type);
    header.appendUInt32(col.column_name.length());
    header.append(col.column_name.data(), col.column_name.length());
    header.appendUInt32(col.rlevel_max);
    header.appendUInt32(col.dlevel_max);
    header.appendUInt64(col.body_offset);
    header.appendUInt64(col.body_size);
  }

  {
    auto ret = pwrite(fd_, header.data(), header.size(), 0);
    if (ret < 0) {
      RAISE_ERRNO(kIOError, "write() failed");
    }
    if (ret != header.size()) {
      RAISE(kIOError, "write() failed");
    }
  }


  for (size_t i = 0; i < columns_.size(); ++i) {
    const auto& col = columns_[i];
    auto writer = column_writers_[i].asInstanceOf<v1::ColumnWriter>();

    Buffer buf(col.body_size);
    writer->write(buf.data(), buf.size());
    RCHECK(buf.size() == col.body_size, "invalid column body size");

    auto ret = pwrite(fd_, buf.data(), buf.size(), col.body_offset);
    if (ret < 0) {
      RAISE_ERRNO(kIOError, "write() failed");
    }
    if (ret != buf.size()) {
      RAISE(kIOError, "write() failed");
    }
  }
}

void CSTableWriter::commitV2() {
  // write index
  Buffer index_buf;
  auto index_os = BufferOutputStream::fromBuffer(&index_buf);
  uint64_t index_offset = page_mgr_->getAllocatedBytes();
  uint64_t index_size = v0_2_0::writeIndex(
      page_mgr_->getPageIndex(),
      index_os.get());

  {
    auto ret = pwrite(fd_, index_buf.data(), index_buf.size(), index_offset);
    if (ret < 0) {
      RAISE_ERRNO(kIOError, "write() failed");
    }
    if (ret != index_buf.size()) {
      RAISE(kIOError, "write() failed");
    }
  }

  // build new meta block
  auto meta_block_position = cstable::v0_2_0::kMetaBlockPosition;
  auto meta_block_size = cstable::v0_2_0::kMetaBlockSize;

  MetaBlock mb;
  mb.transaction_id = current_txid_ + 1;
  mb.num_rows = num_rows_;
  mb.index_offset = index_offset;
  mb.index_size = index_size;

  Buffer buf;
  buf.reserve(meta_block_size);
  auto os = BufferOutputStream::fromBuffer(&buf);

  switch (version_) {
    case BinaryFormatVersion::v0_1_0:
      RAISE(kIllegalArgumentError, "unsupported version: v0.1.0");
    case BinaryFormatVersion::v0_2_0:
      cstable::v0_2_0::writeMetaBlock(mb, os.get());
      break;
  }

  // fsync file before comitting meta block
  fsync(fd_);

  // write to metablock slot
  auto mb_index = mb.transaction_id % 2;
  auto mb_offset = meta_block_position + meta_block_size * mb_index;
  RCHECK(buf.size() == meta_block_size, "invalid meta block size");
  {
    auto ret = pwrite(fd_, buf.data(), buf.size(), mb_offset);
    if (ret < 0) {
      RAISE_ERRNO(kIOError, "write() failed");
    }
    if (ret != meta_block_size) {
      RAISE(kIOError, "write() failed");
    }
  }

  // fsync one more time
  fsync(fd_);
}

RefPtr<ColumnWriter> CSTableWriter::getColumnWriter(
    const String& column_name) const {
  auto col = column_writers_by_name_.find(column_name);
  if (col == column_writers_by_name_.end()) {
    RAISEF(kNotFoundError, "column not found: $0", column_name);
  }

  return col->second;
}

bool CSTableWriter::hasColumn(const String& column_name) const {
  auto col = column_writers_by_name_.find(column_name);
  return col != column_writers_by_name_.end();
}

const TableSchema* CSTableWriter::schema() {
  return schema_.get();
}

const Vector<ColumnConfig>& CSTableWriter::columns() const {
  return columns_;
}

} // namespace cstable


