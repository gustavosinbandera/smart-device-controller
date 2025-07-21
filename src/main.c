#include "mgos.h"
#include "mgos_rpc_service_ota.h"
enum mgos_app_init_result mgos_app_init(void) {
LOG(LL_INFO, ("Hello RPC!"));
  //mgos_set_timer(1000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
