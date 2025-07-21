#include "ping.h"

#include <string>

#include "mgos.h"
#include "mgos_mqtt.h"

//------------------------------------------------------------------------------------
void ping_timer_cb(void *arg) {
    static std::string topic = std::string("iot/") + mgos_sys_config_get_device_id() + "/ping";
    static std::string payload = std::string("{\"fw_version\":\"") + mgos_sys_config_get_device_fw_version() + "\"}";

    if (mgos_mqtt_global_is_connected()) {
        mgos_mqtt_pub(topic.c_str(), payload.c_str(), payload.size(), 1, false);
        LOG(LL_INFO, ("PING! topic:%s, payload:%s", topic.c_str(), payload.c_str()));
    }
}
//------------------------------------------------------------------------------------
bool mgos_ping_init(void) {
    // ping every 6 hours
    mgos_set_timer(6 * 3600 * 1000, MGOS_TIMER_REPEAT, ping_timer_cb, NULL);
    return true;
}
