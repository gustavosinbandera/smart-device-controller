#include "rpc_handler.h"
#include "io_board.h"
#include "frozen.h"
#include "mgos_adc.h"

RpcHandler &RpcHandler::getInstance() {
  static RpcHandler instance;
  return instance;
}

void RpcHandler::init() {
  mg_rpc_add_handler(mgos_rpc_get_global(), "IoBoard.ToggleRelay", "{index: %d}", handleToggleRelay, nullptr);
  mg_rpc_add_handler(mgos_rpc_get_global(), "IoBoard.ReadInputs", "", handleReadInputs, nullptr);
  mg_rpc_add_handler(mgos_rpc_get_global(), "IoBoard.Status", "", handleStatus, nullptr);
  mg_rpc_add_handler(mgos_rpc_get_global(), "IoBoard.GetRelay", "{index: %d}", handleGetRelay, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "IoBoard.SetRelay", "{index: %d, state: %B}", handleSetRelay, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "IoBoard.ReadAnalog", "", handleReadAnalog, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(), "IoBoard.ReportAll", "", handleReportAll, NULL);
  mg_rpc_add_handler(mgos_rpc_get_global(),"IoBoard.SetBuzzer","{state: %B}",handleSetBuzzer,nullptr);
}

void RpcHandler::handleToggleRelay(struct mg_rpc_request_info *ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi, struct mg_str args) {
  int index;
  if (json_scanf(args.p, args.len, "{index: %d}", &index) == 1) {
    IoBoard::getInstance().toggleRelay(index);
    //IoBoard::getInstance().syncShadowAfterCommand(index);
    mg_rpc_send_responsef(ri, "{status: %Q, index: %d}", "toggled", index);
  } else {
    mg_rpc_send_errorf(ri, 400, "Missing index");
  }
}

void RpcHandler::handleGetRelay(struct mg_rpc_request_info *ri, void *cb_arg,
                                struct mg_rpc_frame_info *fi, struct mg_str args) {
  int index;
  if (json_scanf(args.p, args.len, "{index: %d}", &index) == 1) {
    bool state = IoBoard::getInstance().getRelay(index);
    mg_rpc_send_responsef(ri, "{index: %d, state: %B}", index, state);
  } else {
    mg_rpc_send_errorf(ri, 400, "Missing index");
  }
}




void RpcHandler::handleSetRelay(struct mg_rpc_request_info *ri, void *cb_arg,
                                struct mg_rpc_frame_info *fi, struct mg_str args) {
  int index;
  bool state;
  if (json_scanf(args.p, args.len, "{index: %d, state: %B}", &index, &state) == 2) {
    IoBoard::getInstance().setRelay(index, state);
    mg_rpc_send_responsef(ri, "{status: %Q, index: %d, state: %B}", "set", index, state);
  } else {
    mg_rpc_send_errorf(ri, 400, "Missing parameters");
  }
}

void RpcHandler::handleReadAnalog(struct mg_rpc_request_info *ri, void *cb_arg,
                                  struct mg_rpc_frame_info *fi, struct mg_str args) {
  int val = mgos_adc_read(IoBoard::getInstance().analogPin);
  mg_rpc_send_responsef(ri, "{analog: %d}", val);
}

void RpcHandler::handleReportAll(struct mg_rpc_request_info *ri, void *cb_arg,
                                 struct mg_rpc_frame_info *fi, struct mg_str args) {
  IoBoard::getInstance().reportState();
  mg_rpc_send_responsef(ri, "{status: %Q}", "reported");
}

void RpcHandler::handleReadInputs(struct mg_rpc_request_info *ri, void *cb_arg,
                                  struct mg_rpc_frame_info *fi, struct mg_str args) {
  char msg[128];
  struct json_out out = JSON_OUT_BUF(msg, sizeof(msg));

  json_printf(&out, "{");
  for (int i = 0; i < IOBOARD_NUM_INPUTS; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "input%d", i + 1);
    bool val = mgos_gpio_read(IoBoard::getInstance().inputPins[i]);
    json_printf(&out, "%s%Q:%B", (i > 0 ? "," : ""), key, val);
  }
  json_printf(&out, "}");

  mg_rpc_send_responsef(ri, "%s", msg);
}

void RpcHandler::handleStatus(struct mg_rpc_request_info *ri, void *cb_arg,
                              struct mg_rpc_frame_info *fi, struct mg_str args) {
  char msg[256];
  struct json_out out = JSON_OUT_BUF(msg, sizeof(msg));

  json_printf(&out, "{");

  /* ---------- RELAYS ---------- */
  for (int i = 0; i < IOBOARD_NUM_RELAYS; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "relay%d", i + 1);
    bool val = IoBoard::getInstance().getRelay(i);   // ← usa la variable
    json_printf(&out, "%s%Q:%B", (i > 0 ? "," : ""), key, val);
  }

  /* ---------- INPUTS ---------- */
  for (int i = 0; i < IOBOARD_NUM_INPUTS; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "input%d", i + 1);
    bool val = mgos_gpio_read(IoBoard::getInstance().inputPins[i]);
    json_printf(&out, ",%Q:%B", key, val);
  }

  /* ---------- ANALOG ---------- */
  int analog = mgos_adc_read(IoBoard::getInstance().analogPin);
  json_printf(&out, ",%Q:%d", "analog", analog);

  /* ---------- BUZZER ---------- */
  json_printf(&out, ",%Q:%B", "buzzer",
              IoBoard::getInstance().getBuzzer());

  json_printf(&out, "}");
  mg_rpc_send_responsef(ri, "%s", msg);
}


/* ---------- RPC: IoBoard.SetBuzzer ---------- */
void RpcHandler::handleSetBuzzer(struct mg_rpc_request_info *ri,
                                 void *cb_arg,
                                 struct mg_rpc_frame_info *fi,
                                 struct mg_str args) {
  bool state;
  if (json_scanf(args.p, args.len, "{state: %B}", &state) == 1) {
    IoBoard::getInstance().setBuzzer(state);
    mg_rpc_send_responsef(ri,
                          "{status:%Q, state:%B}",
                          "set", state);
  } else {
    mg_rpc_send_errorf(ri, 400, "Missing state");
  }
}


extern "C" {
int mgos_rpc_handler_init(void) {
  RpcHandler::getInstance().init();
  return true;
}
}
