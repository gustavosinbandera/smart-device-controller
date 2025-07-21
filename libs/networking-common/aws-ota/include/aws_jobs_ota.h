#pragma once

extern "C"
{
#include "mgos.h"
}

#include <string>
#include "register_events.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
bool mgos_aws_ota_init();
#ifdef __cplusplus
}

#define OTA_MANAGER OtaManager::getInstance()
//------------------------------------------------------------------------------------
class OtaManager : public RegisterEvent
{
public:
    static OtaManager* getInstance();

    void requestOta();
    void StartOta();
    bool getOtaRequested(){return requested_;}
    bool getRetryInProgress(){return retry_in_progress_;}
    void handleHttpError();
    
    void setProgressPercent(uint8_t val){progress_percent_ = val;}
    uint8_t getProgressPercent(){return progress_percent_;}
    
    uint8_t getHttpRetriesLeft(){return http_retries_left_;}

    void OtaHttpStart();
    void WriteSettings(const char* JobId, const char* Version, bool after_reboot);
    void clearOTA();
private:
    bool requested_ = false;
    bool retry_in_progress_ = false;
    std::string http_url_ = "";
    static OtaManager* inst_;
    uint8_t progress_percent_ = 0;
    uint8_t http_retries_left_ = 0;
    OtaManager() {} // private constructor
};
#endif /* __cplusplus */
