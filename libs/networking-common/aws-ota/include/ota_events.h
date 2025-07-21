#pragma once

#include "mgos.h"

#define EVENT_OTA_BASE MGOS_EVENT_BASE('L', 'T', 'A')

enum ota_event_t {
    EVENT_OTA_RESULT_FAILED = EVENT_OTA_BASE,
    EVENT_OTA_RESULT_COMPLETE,
    EVENT_OTA_COMMITED,
    EVENT_OTA_UKN
};