#pragma once
#include "mgos.h"
#include "mgos_rpc.h"


class RpcHandler {
 public:
   static RpcHandler &getInstance();
   static void init();

 private:
  static void handleToggleRelay(struct mg_rpc_request_info *ri, void *cb_arg,
                                struct mg_rpc_frame_info *fi, struct mg_str args);
  static void handleReadInputs(struct mg_rpc_request_info *ri, void *cb_arg,
                               struct mg_rpc_frame_info *fi, struct mg_str args);
  static void handleStatus(struct mg_rpc_request_info *ri, void *cb_arg,
                           struct mg_rpc_frame_info *fi, struct mg_str args);
 static void handleGetRelay(struct mg_rpc_request_info *ri, void *cb_arg,
                           struct mg_rpc_frame_info *fi, struct mg_str args);

static void handleSetRelay(struct mg_rpc_request_info *ri, void *cb_arg,
                           struct mg_rpc_frame_info *fi, struct mg_str args);

static void handleReadAnalog(struct mg_rpc_request_info *ri, void *cb_arg,
                             struct mg_rpc_frame_info *fi, struct mg_str args);

static void handleReportAll(struct mg_rpc_request_info *ri, void *cb_arg,
                            struct mg_rpc_frame_info *fi, struct mg_str args);

static void handleSetBuzzer(struct mg_rpc_request_info *ri,
                                 void *cb_arg,
                                 struct mg_rpc_frame_info *fi,
                                 struct mg_str args);
};
