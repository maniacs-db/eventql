/**
 * Copyright (c) 2016 zScale Technology GmbH <legal@zscale.io>
 * Authors:
 *   - Paul Asmuth <paul@zscale.io>
 *   - Laura Schlimmer <laura@zscale.io>
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
#include <eventql/sql/expressions/table_expression.h>
#include <eventql/AnalyticsAuth.h>
#include <eventql/db/partition_map.h>

#include "eventql/eventql.h"

namespace eventql {

class RemoteExpression : public csql::TableExpression {
public:
  static const constexpr size_t kMaxBufferSize = 100;

  RemoteExpression(
      csql::Transaction* txn,
      const String& db_namespace,
      RefPtr<csql::TableExpressionNode> qtree,
      Vector<ReplicaRef> hosts,
      AnalyticsAuth* auth);

  ~RemoteExpression();

  ScopedPtr<csql::ResultCursor> execute() override;

protected:

  void executeAsync();
  void executeOnHost(const InetAddr& host);

  bool next(csql::SValue* out_row, size_t out_row_len);

  csql::Transaction* txn_;
  String db_namespace_;
  RefPtr<csql::TableExpressionNode> qtree_;
  Vector<ReplicaRef> hosts_;
  AnalyticsAuth* auth_;
  bool eof_;
  bool error_;
  String error_str_;
  std::atomic<bool> cancelled_;
  std::mutex mutex_;
  std::condition_variable cv_;
  List<Vector<csql::SValue>> buf_;
  size_t buf_ctr_;
  std::thread thread_;
};

}
