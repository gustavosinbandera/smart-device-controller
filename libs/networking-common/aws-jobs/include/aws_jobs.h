#pragma once
#include <memory>
#include "mgos.h"
#include "mgos_mqtt.h"
#include <iostream>

#include <map>
#include <string>
using namespace std;

#include <iterator>

#ifdef __cplusplus
extern "C"
{
#endif
    bool mgos_aws_jobs_init(void);
#ifdef __cplusplus
}
#endif

#define JOBS_JOBID_MAX_LENGTH        64U       /* per AWS IoT API Reference */

enum class QoS
{
    QOS0 = 0, ///< QoS0
    QOS1 = 1  ///< QoS1
};

enum class JobExecutionTopicType
{
    JOB_UNRECOGNIZED_TOPIC = 0,
    JOB_GET_PENDING_TOPIC,
    JOB_START_NEXT_TOPIC,
    JOB_DESCRIBE_TOPIC,
    JOB_UPDATE_TOPIC,
    JOB_NOTIFY_TOPIC,
    JOB_NOTIFY_NEXT_TOPIC,
    JOB_WILDCARD_TOPIC
};

enum class JobExecutionTopicReplyType
{
    JOB_UNRECOGNIZED_TOPIC_TYPE = 0,
    JOB_REQUEST_TYPE,
    JOB_ACCEPTED_REPLY_TYPE,
    JOB_REJECTED_REPLY_TYPE,
    JOB_WILDCARD_REPLY_TYPE
};

enum class JobExecutionStatus
{
    JOB_EXECUTION_STATUS_NOT_SET = 0,
    JOB_EXECUTION_QUEUED,
    JOB_EXECUTION_IN_PROGRESS,
    JOB_EXECUTION_FAILED,
    JOB_EXECUTION_SUCCEEDED,
    JOB_EXECUTION_CANCELED,
    JOB_EXECUTION_REJECTED,
    /***
             * Used for any status not in the supported list of statuses
             */
    JOB_EXECUTION_UNKNOWN_STATUS = 99
};

class AWSJobs
{
public:
    // Disabling default and copy constructors.
    AWSJobs() = delete;                            // Delete Default constructor
    AWSJobs(const AWSJobs &) = delete;              // Delete Copy constructor
    AWSJobs(AWSJobs &&) = default;                  // Default Move constructor
    AWSJobs &operator=(const AWSJobs &) & = delete; // Delete Copy assignment operator
    AWSJobs &operator=(AWSJobs &&) & = default;     // Default Move assignment operator

    static std::unique_ptr<AWSJobs> Create(/*std::shared_ptr<mg_connection> p_mqtt_client,*/
                                          QoS qos,
                                          const std::string &thing_name,
                                          const std::string &client_token = std::string());


class RpcHandler {
 public:
  static RpcHandler &getInstance();
  static void init();

 private:
  stat
    std::unique_ptr<std::string> GetJobTopic(JobExecutionTopicType topicType,
                                             JobExecutionTopicReplyType replyType = JobExecutionTopicReplyType::JOB_REQUEST_TYPE,
                                             const std::string &jobId = std::string());
    
    unique_ptr<string> SendJobsQuery(JobExecutionTopicType topicType, const string &jobId);

    void SendJobsUpdate(const string &jobId,
                                JobExecutionStatus status,
                                const std::map<std::string, std::string> &statusDetailsMap = std::map<std::string, std::string>(),
                                int64_t expectedVersion = 0,    // set to 0 to ignore
                                int64_t executionNumber = 0,    // set to 0 to ignore
                                bool includeJobExecutionState = false,
                                bool includeJobDocument = false);

protected:
    AWSJobs(QoS qos,
           const std::string &thing_name,
           const std::string &client_token);

    static bool BaseTopicRequiresJobId(JobExecutionTopicType topicType);
    static const std::string GetOperationForBaseTopic(JobExecutionTopicType topicType);
    static const std::string GetSuffixForTopicType(JobExecutionTopicReplyType replyType);
    static const std::string GetExecutionStatus(JobExecutionStatus status);
    static std::string Escape(const std::string &value);

    std::string SerializeDescribeJobExecutionPayload(int64_t executionNumber = 0, bool includeJobDocument = true);
    static std::string SerializeStatusDetails(const map<std::string, std::string> &statusDetailsMap);

    std::string SerializeJobExecutionUpdatePayload(JobExecutionStatus status,
                                                        const std::map<std::string, std::string> &statusDetailsMap = map<std::string, std::string>(),
                                                        int64_t expectedVersion = 0,
                                                        int64_t executionNumber = 0,
                                                        bool includeJobExecutionState = false,
                                                        bool includeJobDocument = false);
    std::string SerializeStartNextPendingJobExecutionPayload(const map<std::string, std::string> &statusDetailsMap = map<std::string, std::string>());
    std::string SerializeClientTokenPayload();
    std::unique_ptr<std::string> Create(std::string str);
     
    QoS qos_;
    std::string thing_name_;
    std::string client_token_;

private:
};