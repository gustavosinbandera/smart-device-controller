#include "aws_jobs_ota.h"
#include "aws_jobs.h"
#include "mgos_mqtt.h"
#include "mgos_ota_http_client.h"
#include <string>
using namespace std;

unique_ptr<AWSJobs> pJobs = nullptr;

string gJobIdString;
string gFwVersion;
string gHttpUrl = "";

//------------------------------------------------------------------------------------
// get a value for the given json path and key
string hasMember(const char *msg, int msg_len, const char* json_path, const char* key_name)
{
    void *h = nullptr;
    json_token key, val;
    
    while ((h = json_next_key(msg, msg_len, h, json_path, &key, &val)) != NULL) 
    {
        if(strncmp(key_name, key.ptr, key.len)== 0)
            return string(val.ptr, val.len);
    }

    return string("");
}
//------------------------------------------------------------------------------------
string getUrl(string& jobId, string& JobDoc)
{
    string Url = hasMember(JobDoc.data(), JobDoc.size(), ".", "url");
    
    if(Url.empty())
    {      
        LOG(LL_ERROR,   ( "Job document schema is invalid. Missing \"url\" for \"ota\" action type." ) );    
        map<string, string> statusDetailsMap;
        statusDetailsMap["reason"] = "Job document schema is invalid. Missing 'url' for 'ota' action type.";
        pJobs->SendJobsUpdate(jobId, JobExecutionStatus::JOB_EXECUTION_FAILED, statusDetailsMap);
        return string("");        
    }
    return Url;
}
//------------------------------------------------------------------------------------
string getVersion(string& jobId, string& JobDoc)
{
    string Version = hasMember(JobDoc.data(), JobDoc.size(), ".", "version");
    
    if(Version.empty())
    {      
        LOG(LL_ERROR,   ( "Job document schema is invalid. Missing firmware \"version\"" ) );
        map<string, string> statusDetailsMap;
        statusDetailsMap["reason"] = "Job document schema is invalid. Missing firmware 'version'";
        pJobs->SendJobsUpdate(jobId, JobExecutionStatus::JOB_EXECUTION_FAILED, statusDetailsMap);
        return string("");
    }

    string CurVersion("");
    if(mgos_sys_config_get_device_fw_version())
        CurVersion = mgos_sys_config_get_device_fw_version();

    if(Version == CurVersion)
    {
        LOG(LL_INFO,   ( "The new firwmware version is identical to the current one" ) );
        map<string, string> statusDetailsMap;
        statusDetailsMap["reason"] = "The new firwmware version is identical to the current one";
        pJobs->SendJobsUpdate(jobId, JobExecutionStatus::JOB_EXECUTION_REJECTED, statusDetailsMap);
        return string("");
    }
    
    return Version;
}
//------------------------------------------------------------------------------------
string getAction(string& jobId, string& JobDoc)
{
    string Action = hasMember(JobDoc.data(), JobDoc.size(), ".", "action");

    if(Action.empty())
    {
        LOG(LL_ERROR,   ( "Job document schema is invalid. Missing expected \"action\" key in document." ) );
        map<string, string> statusDetailsMap;
        statusDetailsMap["reason"] = "Job document schema is invalid. Missing expected 'action' key in document.";
        pJobs->SendJobsUpdate(jobId, JobExecutionStatus::JOB_EXECUTION_FAILED, statusDetailsMap);
        return string("");
    }
    
    if(Action != string("ota"))
    {
        LOG(LL_WARN, ( "Received Job document with unknown action %s", Action.c_str()) );
        map<string, string> statusDetailsMap;
        statusDetailsMap["reason"] = "Received Job document with unknown action";
        pJobs->SendJobsUpdate(jobId, JobExecutionStatus::JOB_EXECUTION_FAILED, statusDetailsMap);
        return string("");
    }
    return Action;
}
//------------------------------------------------------------------------------------
string getJobDocument(const char *msg, int msg_len, string& jobId)
{
    string JobDoc = hasMember(msg, msg_len, ".execution", "jobDocument");

    if(JobDoc.empty())
    {
        LOG(LL_INFO,("job document not found or invalid"));
        map<string, string> statusDetailsMap;
        statusDetailsMap["reason"] = "job document not found or invalid";
        pJobs->SendJobsUpdate(jobId, JobExecutionStatus::JOB_EXECUTION_FAILED, statusDetailsMap);
        return string("");
    }
    return JobDoc;
}
//------------------------------------------------------------------------------------
string getJobId(const char *msg, int msg_len)
{
    string JobId = hasMember(msg, msg_len, ".execution", "jobId");

    if(JobId.empty() || JobId.size() >= JOBS_JOBID_MAX_LENGTH)
    {
        LOG(LL_INFO,("No job execution description found or invalid job id"));
        return string("");
    }
    return JobId;
}
//------------------------------------------------------------------------------------
void aws_jobs_cb(struct mg_connection *c, const char *topic, int topic_len, const char *msg, int msg_len, void *userdata)
{
    LOG(LL_INFO,("AWS IN PUBLISH, topic:%.*s, msg:[%.*s]", topic_len, topic, msg_len, msg));
    
    if(mgos_ota_is_in_progress()) {
        return; // do not interrupt the OTA if its in progress
    }
    string JobId = getJobId(msg, msg_len);
    if (JobId.empty())
        return;

    LOG(LL_INFO,("jobId:%s", JobId.c_str()));
    string JobDocument = getJobDocument(msg, msg_len, JobId);

    // do not process the job from previos OTA, 
    // its IN_PROGRESS state and will be updated after OTA is commited
    if(gJobIdString == JobId)
        return;

    if(JobDocument.empty())
        return;

    if(getAction(JobId, JobDocument).empty())
        return;        

    gHttpUrl = getUrl(JobId, JobDocument);
    if(gHttpUrl.empty())
        return;
    
    string Version = getVersion(JobId, JobDocument);
    if(Version.empty())
        return;    
    
    // save JobId in a global variable to update job status
    gJobIdString = JobId;
    // save new fw version to update after ota
    gFwVersion = Version;

    pJobs->SendJobsUpdate(JobId, JobExecutionStatus::JOB_EXECUTION_IN_PROGRESS);
    LOG(LL_INFO,("OTA request received, update is scheduled"));
    OTA_MANAGER->requestOta();
}
//------------------------------------------------------------------------------------
void ota_status_cb(int ev, void *ev_data, void *userdata) {
  const struct mgos_ota_status *status = (const struct mgos_ota_status *) ev_data;
  /*LOG(LL_INFO, ("OTA STATUS is_committed=%d, commit_timeout=%d, partition=%d, msg: "
                "\"%s\", progress_percent=%d, state:%d",
                status->is_committed, status->commit_timeout, status->partition,
                status->msg, status->progress_percent, status->state));*/
  
  if(status->state == MGOS_OTA_STATE_ERROR) 
  {
    LOG(LL_INFO,("http_url:%s, retries left:%d, ota_in_progress:%d, mqtt conn:%d, msg:%s", gHttpUrl.c_str(),
                    OTA_MANAGER->getHttpRetriesLeft(), mgos_ota_is_in_progress(),
                    mgos_mqtt_global_is_connected(), status->msg));
    if(string(status->msg) == "Invalid HTTP response code") {
        map<string, string> statusDetailsMap;
        statusDetailsMap["reason"] = status->msg;
        pJobs->SendJobsUpdate(gJobIdString, JobExecutionStatus::JOB_EXECUTION_FAILED, statusDetailsMap);
        OTA_MANAGER->clearOTA();
        static uint8_t http_error = 1;
        mgos_event_trigger(EVENT_OTA_RESULT_FAILED, (void*)&http_error);
    }
    OTA_MANAGER->handleHttpError();
  } else if (status->state == MGOS_OTA_STATE_PROGRESS) {
      OTA_MANAGER->setProgressPercent(status->progress_percent);
    } else {
    if (status->state == MGOS_OTA_STATE_SUCCESS) {
        // save job id to update job state after reboot
        // save new fw version to update after ota is successfull
        mgos_event_trigger(EVENT_OTA_RESULT_COMPLETE, NULL);
        OTA_MANAGER->WriteSettings(gJobIdString.c_str(), gFwVersion.c_str(), false);
    }
  }
}

//------------------------------------------------------------------------------------
void mqtt_glob_event_handler_cb(struct mg_connection *nc, int ev,
                                   void *ev_data MG_UD_ARG(void *user_data)) 
{
    if(ev == MG_EV_MQTT_CONNACK)
    {
        LOG(LL_INFO,("MG_EV_MQTT_CONNACK"));

        if (OTA_MANAGER->getOtaRequested()) {
            if (mgos_ota_is_in_progress() == false && OTA_MANAGER->getRetryInProgress()) {
                OTA_MANAGER->OtaHttpStart();
            }
        } else {
            if(!gJobIdString.empty()) {
                // commit the ota update and update the job status
                if (!mgos_ota_is_committed())
                {
                    mgos_ota_commit();
                    // update or shedule update of the job in case theres no mqtt connection
                    pJobs->SendJobsUpdate(gJobIdString, JobExecutionStatus::JOB_EXECUTION_SUCCEEDED);
                    LOG(LL_INFO,("OTA SUCCEEDED"));
                    mgos_event_trigger(EVENT_OTA_COMMITED, NULL);
                    OTA_MANAGER->WriteSettings("", gFwVersion.c_str(), true);
                } else {
                    // the OTA is already commited if there was a rollback and OTA failed
                    map<string, string> statusDetailsMap;
                    statusDetailsMap["reason"] = "OTA wasnt commited after reboot and will be rolled back";
                    pJobs->SendJobsUpdate(gJobIdString, JobExecutionStatus::JOB_EXECUTION_FAILED, statusDetailsMap);
                    LOG(LL_INFO,("OTA FAILED"));
                    OTA_MANAGER->WriteSettings("", nullptr, true);
                }
                gJobIdString.clear();
            }
        }
        // Publish to AWS IoT Jobs on the DescribeJobExecution API to request the next pending job
        unique_ptr<string> res = pJobs->SendJobsQuery(JobExecutionTopicType::JOB_DESCRIBE_TOPIC, "$next");
        LOG(LL_INFO,("OUT publish:%s", res->c_str()));
    }
}
//-----------------------------------------------------------------------------
OtaManager *OtaManager::inst_ = NULL;
OtaManager *OtaManager::getInstance() {
    if (inst_ == NULL) {
        inst_ = new OtaManager();
    }
    return (inst_);
}
//------------------------------------------------------------------------------------
void OtaManager::OtaHttpStart() {
    if (http_retries_left_) {
        http_retries_left_ -= 1;
    }
    LOG(LL_INFO,("URL:%s, retries left:%d", gHttpUrl.c_str(), http_retries_left_));
    LOG(LL_INFO,("------------OTA START------------"));

    mgos_ota_opts opts;
    opts.timeout = mgos_sys_config_get_ota_http_ota_timeout_sec();
    opts.commit_timeout = mgos_sys_config_get_ota_http_ota_timeout_sec();
    opts.ignore_same_version = false;
    mgos_ota_http_start(gHttpUrl.c_str(), &opts);
}
//------------------------------------------------------------------------------------
void OtaManager::WriteSettings(const char* JobId, const char* Version, bool after_reboot)
{
    mgos_sys_config_set_ota_job_id(JobId);
    http_retries_left_ = 0;
    gHttpUrl = "";
    
    if(after_reboot)
    {
        if(Version)
            mgos_sys_config_set_device_fw_version(Version);
        mgos_sys_config_set_ota_fw_version("");
    }else
        mgos_sys_config_set_ota_fw_version(Version);

    char *err = NULL;
    save_cfg(&mgos_sys_config, &err); /* Writes conf9.json */

    if(err)
        LOG(LL_ERROR,   ( "Error saving configuration: %s", err ) );

    free(err);    
}
//------------------------------------------------------------------------------------
void OtaManager::requestOta() {
    requested_ = true;
}
//------------------------------------------------------------------------------------
void OtaManager::clearOTA()
{
    requested_ = false;
    retry_in_progress_ = false;
    http_retries_left_ = false;
}
//------------------------------------------------------------------------------------
void OtaManager::handleHttpError() {
    if (mgos_ota_is_in_progress() == false) {
        if (retry_in_progress_) {
            if (http_retries_left_ == 0) {
                LOG(LL_INFO,("OTA retry FAILED, schedule OTA in 3 hours"));
                retry_in_progress_ = false;
                mgos_event_trigger(EVENT_OTA_RESULT_FAILED, NULL);
            } else {
                LOG(LL_INFO, ( "OtaHttpStart()_____________________+++++++++++++++++"));
                OtaHttpStart();
            }
        }
    }
}
//------------------------------------------------------------------------------------
void OtaManager::StartOta() {
    http_retries_left_ = mgos_sys_config_get_ota_http_max_retries();
    retry_in_progress_ = true;
    OtaHttpStart();
}
//------------------------------------------------------------------------------------
bool mgos_aws_ota_init()
{
    OTA_MANAGER->registerEventBase((ota_event_t)EVENT_OTA_BASE, "OTA_EVENTS");
    // set a callback to handle HTTP OTA success\error events
    mgos_event_add_handler(MGOS_EVENT_OTA_STATUS, ota_status_cb, NULL);
    
    // load ota job id to gJobIdString, the job id was saved in the previos OTA ettempt
    // ota.job_id is set to empty string after OTA is over
    if(mgos_sys_config_get_ota_job_id())    
        gJobIdString = mgos_sys_config_get_ota_job_id();
    
    if(mgos_sys_config_get_ota_fw_version())
        gFwVersion = mgos_sys_config_get_ota_fw_version();
    
    // MQTT events callback to handle MQTT connack event
    mgos_mqtt_add_global_handler(mqtt_glob_event_handler_cb, NULL);

    pJobs = AWSJobs::Create(QoS::QOS1, string(mgos_sys_config_get_aws_thing_name()));
    // subscribe to AWS Jobs notify-next topic
    unique_ptr<string> pNext = pJobs->GetJobTopic(JobExecutionTopicType::JOB_NOTIFY_NEXT_TOPIC, 
                                                  JobExecutionTopicReplyType::JOB_REQUEST_TYPE);
    mgos_mqtt_sub(pNext->c_str(), aws_jobs_cb, NULL);

    // subscribe to AWS Jobs describe topic
    unique_ptr<string> pAccepted = pJobs->GetJobTopic(JobExecutionTopicType::JOB_DESCRIBE_TOPIC, 
                                                      JobExecutionTopicReplyType::JOB_ACCEPTED_REPLY_TYPE,
                                                      string("$next"));
    mgos_mqtt_sub(pAccepted->c_str(), aws_jobs_cb, NULL);
	return true;
}
