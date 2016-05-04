/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#pragma once
#include "eventql/util/VFS.h"
#include "eventql/util/http/httpservice.h"
#include "eventql/util/http/HTTPSSEStream.h"
#include "eventql/util/json/json.h"
#include "eventql/util/web/SecureCookie.h"
#include "eventql/AnalyticsApp.h"
#include "eventql/AnalyticsSession.pb.h"
#include "eventql/sql/runtime/runtime.h"
#include "eventql/AnalyticsAuth.h"
#include "eventql/ConfigDirectory.h"
#include "eventql/core/TSDBService.h"
#include "eventql/api/LogfileAPIServlet.h"
#include "eventql/api/EventsAPIServlet.h"
#include "eventql/api/MapReduceAPIServlet.h"
#include "eventql/RemoteTSDBScanParams.pb.h"

using namespace stx;

namespace zbase {

struct AnalyticsQuery;

class AnalyticsServlet : public stx::http::StreamingHTTPService {
public:

  AnalyticsServlet(
      RefPtr<AnalyticsApp> app,
      const String& cachedir,
      AnalyticsAuth* auth,
      csql::Runtime* sql,
      zbase::TSDBService* tsdb,
      ConfigDirectory* customer_dir,
      PartitionMap* pmap);

  void handleHTTPRequest(
      RefPtr<stx::http::HTTPRequestStream> req_stream,
      RefPtr<stx::http::HTTPResponseStream> res_stream);

protected:

  void handle(
      RefPtr<stx::http::HTTPRequestStream> req_stream,
      RefPtr<stx::http::HTTPResponseStream> res_stream);

  void getAuthInfo(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void getPrivateAPIToken(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void pushEvents(
      const URI& uri,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void insertIntoMetric(
      const URI& uri,
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void listTables(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void fetchTableDefinition(
      const AnalyticsSession& session,
      const String& table_name,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void createTable(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void addTableField(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void removeTableField(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void addTableTag(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void removeTableTag(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void insertIntoTable(
      const Option<AnalyticsSession>& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void uploadTable(
      const URI& uri,
      const AnalyticsSession& session,
      http::HTTPRequestStream* req_stream,
      http::HTTPResponse* res);

  void executeSQL(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res,
      RefPtr<http::HTTPResponseStream> res_stream);

  void executeSQL_ASCII(
      const URI::ParamList& params,
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res,
      RefPtr<http::HTTPResponseStream> res_stream);

  void executeSQL_BINARY(
      const URI::ParamList& params,
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res,
      RefPtr<http::HTTPResponseStream> res_stream);

  void executeSQL_JSON(
      const URI::ParamList& params,
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res,
      RefPtr<http::HTTPResponseStream> res_stream);

  void executeSQL_JSONSSE(
      const URI::ParamList& params,
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res,
      RefPtr<http::HTTPResponseStream> res_stream);

  void executeSQLAggregatePartition(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void executeSQLScanPartition(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res,
      RefPtr<http::HTTPResponseStream> res_stream);

  void executeDrilldownQuery(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res,
      RefPtr<http::HTTPResponseStream> res_stream);

  void pipelineInfo(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void sessionTrackingListEvents(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void sessionTrackingEventInfo(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void sessionTrackingEventAdd(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void sessionTrackingEventRemove(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void sessionTrackingEventAddField(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void sessionTrackingEventRemoveField(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void sessionTrackingListAttributes(
      const AnalyticsSession& session,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void performLogin(
      const URI& uri,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  void performLogout(
      const URI& uri,
      const http::HTTPRequest* req,
      http::HTTPResponse* res);

  inline void expectHTTPPost(const http::HTTPRequest& req) {
    if (req.method() != http::HTTPMessage::M_POST) {
      RAISE(kIllegalArgumentError, "expected HTTP POST request");
    }
  }

  String getCookieDomain(const http::HTTPRequest& req) {
    auto domain = req.getHeader("Host");
    auto ppos = domain.find(":");
    if (ppos != String::npos) {
      domain.erase(domain.begin() + ppos, domain.end());
    }

    auto parts = StringUtil::split(domain, ".");
    if (parts.size() > 2) {
      parts.erase(parts.begin(), parts.end() - 2);
    }

    if (parts.size() == 2) {
      return "." + StringUtil::join(parts, ".");
    } else {
      return "";
    }
  }

  void catchAndReturnErrors(
      http::HTTPResponse* resp,
      Function<void ()> fn) const {
    try {
      fn();
    } catch (const StandardException& e) {
      resp->setStatus(http::kStatusInternalServerError);
      resp->addBody(e.what());
    }
  }

  RefPtr<AnalyticsApp> app_;
  String cachedir_;
  AnalyticsAuth* auth_;
  csql::Runtime* sql_;
  zbase::TSDBService* tsdb_;
  ConfigDirectory* customer_dir_;

  LogfileAPIServlet logfile_api_;
  EventsAPIServlet events_api_;
  MapReduceAPIServlet mapreduce_api_;
  PartitionMap* pmap_;
};

}
