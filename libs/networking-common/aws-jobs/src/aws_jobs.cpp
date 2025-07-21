
#include "aws_jobs.h"

#define BASE_THINGS_TOPIC "$aws/things/"

#define NOTIFY_OPERATION "notify"
#define NOTIFY_NEXT_OPERATION "notify-next"
#define GET_OPERATION "get"
#define START_NEXT_OPERATION "start-next"
#define WILDCARD_OPERATION "+"
#define UPDATE_OPERATION "update"
#define ACCEPTED_REPLY "accepted"
#define REJECTED_REPLY "rejected"
#define WILDCARD_REPLY "#"

AWSJobs::AWSJobs(QoS qos, const std::string &thing_name, const std::string &client_token)
{
    qos_ = qos;
    thing_name_ = thing_name;
    client_token_ = client_token;
};

std::unique_ptr<AWSJobs> AWSJobs::Create(QoS qos, const std::string &thing_name, const std::string &client_token)
{
    return std::unique_ptr<AWSJobs>(new AWSJobs(qos, thing_name, client_token));
}

bool AWSJobs::BaseTopicRequiresJobId(JobExecutionTopicType topicType)
{
    switch (topicType)
    {
    case JobExecutionTopicType::JOB_UPDATE_TOPIC:
    case JobExecutionTopicType::JOB_DESCRIBE_TOPIC:
        return true;
    case JobExecutionTopicType::JOB_NOTIFY_TOPIC:
    case JobExecutionTopicType::JOB_NOTIFY_NEXT_TOPIC:
    case JobExecutionTopicType::JOB_START_NEXT_TOPIC:
    case JobExecutionTopicType::JOB_GET_PENDING_TOPIC:
    case JobExecutionTopicType::JOB_WILDCARD_TOPIC:
    case JobExecutionTopicType::JOB_UNRECOGNIZED_TOPIC:
    default:
        return false;
    }
};

const std::string AWSJobs::GetOperationForBaseTopic(JobExecutionTopicType topicType)
{
    switch (topicType)
    {
    case JobExecutionTopicType::JOB_UPDATE_TOPIC:
        return UPDATE_OPERATION;
    case JobExecutionTopicType::JOB_NOTIFY_TOPIC:
        return NOTIFY_OPERATION;
    case JobExecutionTopicType::JOB_NOTIFY_NEXT_TOPIC:
        return NOTIFY_NEXT_OPERATION;
    case JobExecutionTopicType::JOB_GET_PENDING_TOPIC:
    case JobExecutionTopicType::JOB_DESCRIBE_TOPIC:
        return GET_OPERATION;
    case JobExecutionTopicType::JOB_START_NEXT_TOPIC:
        return START_NEXT_OPERATION;
    case JobExecutionTopicType::JOB_WILDCARD_TOPIC:
        return WILDCARD_OPERATION;
    case JobExecutionTopicType::JOB_UNRECOGNIZED_TOPIC:
    default:
        return "";
    }
};

const std::string AWSJobs::GetSuffixForTopicType(JobExecutionTopicReplyType replyType)
{
    switch (replyType)
    {
    case JobExecutionTopicReplyType::JOB_REQUEST_TYPE:
        return "";
    case JobExecutionTopicReplyType::JOB_ACCEPTED_REPLY_TYPE:
        return "/" ACCEPTED_REPLY;
    case JobExecutionTopicReplyType::JOB_REJECTED_REPLY_TYPE:
        return "/" REJECTED_REPLY;
    case JobExecutionTopicReplyType::JOB_WILDCARD_REPLY_TYPE:
        return "/" WILDCARD_REPLY;
    case JobExecutionTopicReplyType::JOB_UNRECOGNIZED_TOPIC_TYPE:
    default:
        return "";
    }
}

const std::string AWSJobs::GetExecutionStatus(JobExecutionStatus status)
{
    switch (status)
    {
    case JobExecutionStatus::JOB_EXECUTION_QUEUED:
        return "QUEUED";
    case JobExecutionStatus::JOB_EXECUTION_IN_PROGRESS:
        return "IN_PROGRESS";
    case JobExecutionStatus::JOB_EXECUTION_FAILED:
        return "FAILED";
    case JobExecutionStatus::JOB_EXECUTION_SUCCEEDED:
        return "SUCCEEDED";
    case JobExecutionStatus::JOB_EXECUTION_CANCELED:
        return "CANCELED";
    case JobExecutionStatus::JOB_EXECUTION_REJECTED:
        return "REJECTED";
    case JobExecutionStatus::JOB_EXECUTION_STATUS_NOT_SET:
    case JobExecutionStatus::JOB_EXECUTION_UNKNOWN_STATUS:
    default:
        return "";
    }
}

std::unique_ptr<std::string> AWSJobs::Create(std::string str)
{
    /*  if (!IsValidInput(str)) {
            return nullptr;
        } */
    return std::unique_ptr<std::string>(new std::string(str));
}

std::unique_ptr<std::string> AWSJobs::GetJobTopic(JobExecutionTopicType topicType,
                                                 JobExecutionTopicReplyType replyType,
                                                 const std::string &jobId)
{
    if (thing_name_.empty())
    {
        return nullptr;
    }

    if ((topicType == JobExecutionTopicType::JOB_NOTIFY_TOPIC || topicType ==
                                                                     JobExecutionTopicType::JOB_NOTIFY_NEXT_TOPIC) &&
        replyType != JobExecutionTopicReplyType::JOB_REQUEST_TYPE)
    {
        return nullptr;
    }

    if ((topicType == JobExecutionTopicType::JOB_GET_PENDING_TOPIC || topicType == JobExecutionTopicType::JOB_START_NEXT_TOPIC ||
         topicType == JobExecutionTopicType::JOB_NOTIFY_TOPIC || topicType == JobExecutionTopicType::JOB_NOTIFY_NEXT_TOPIC) &&
        !jobId.empty())
    {
        return nullptr;
    }

    const bool requireJobId = BaseTopicRequiresJobId(topicType);
    if (jobId.empty() && requireJobId)
    {
        return nullptr;
    }

    const std::string operation = GetOperationForBaseTopic(topicType);
    if (operation.empty())
    {
        return nullptr;
    }

    const std::string suffix = GetSuffixForTopicType(replyType);

    if (requireJobId)
    {
        return Create(BASE_THINGS_TOPIC + thing_name_ + "/jobs/" + jobId + '/' + operation + suffix);
    }
    else if (topicType == JobExecutionTopicType::JOB_WILDCARD_TOPIC)
    {
        return Create(BASE_THINGS_TOPIC + thing_name_ + "/jobs/#");
    }
    else
    {
        return Create(BASE_THINGS_TOPIC + thing_name_ + "/jobs/" + operation + suffix);
    }
};

std::string AWSJobs::SerializeJobExecutionUpdatePayload(JobExecutionStatus status,
                                                       const map<std::string, std::string> &statusDetailsMap,
                                                       int64_t expectedVersion, // set to 0 to ignore
                                                       int64_t executionNumber, // set to 0 to ignore
                                                       bool includeJobExecutionState,
                                                       bool includeJobDocument)
{
    const std::string executionStatus = GetExecutionStatus(status);

    if (executionStatus.empty())
    {
        return "";
    }

    std::string result = "{\"status\":\"" + executionStatus + "\"";
    if (!statusDetailsMap.empty())
    {
        result += ",\"statusDetails\":" + SerializeStatusDetails(statusDetailsMap);
    }
    if (expectedVersion > 0)
    {
        result += ",\"expectedVersion\":\"" + std::to_string(expectedVersion) + "\"";
    }
    if (executionNumber > 0)
    {
        result += ",\"executionNumber\":\"" + std::to_string(executionNumber) + "\"";
    }
    if (includeJobExecutionState)
    {
        result += ",\"includeJobExecutionState\":\"true\"";
    }
    if (includeJobDocument)
    {
        result += ",\"includeJobDocument\":\"true\"";
    }
    if (!client_token_.empty())
    {
        result += ",\"clientToken\":\"" + client_token_ + "\"";
    }
    result += '}';

    return result;
};

std::string AWSJobs::SerializeStatusDetails(const map<std::string, std::string> &statusDetailsMap)
{
    std::string result = "{";

    map<std::string, std::string>::const_iterator itr = statusDetailsMap.begin();
    while (itr != statusDetailsMap.end())
    {
        result += (itr == statusDetailsMap.begin() ? "\"" : ",\"");
        result += Escape(itr->first) + "\":\"" + Escape(itr->second) + "\"";
        itr++;
    }

    result += '}';
    return result;
}

std::string AWSJobs::Escape(const std::string &value)
{
    std::string result = "";

    for (int i = 0; i < value.length(); i++)
    {
        switch (value[i])
        {
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        default:
            result += value[i];
        }
    }
    return result;
}

std::string AWSJobs::SerializeDescribeJobExecutionPayload(int64_t executionNumber, // set to 0 to ignore
                                                         bool includeJobDocument)
{
    std::string result = "{\"includeJobDocument\":\"";
    result += (includeJobDocument ? "true" : "false");
    result += "\"";
    if (executionNumber > 0)
    {
        result += ",\"executionNumber\":\"" + std::to_string(executionNumber) + "\"";
    }
    if (!client_token_.empty())
    {
        result += "\"clientToken\":\"" + client_token_ + "\"";
    }
    result += '}';

    return result;
}

std::string AWSJobs::SerializeStartNextPendingJobExecutionPayload(const map<std::string, std::string> &statusDetailsMap)
{
    std::string result = "{";
    if (!statusDetailsMap.empty())
    {
        result += "\"statusDetails\":" + SerializeStatusDetails(statusDetailsMap);
    }
    if (!client_token_.empty())
    {
        if (!statusDetailsMap.empty())
        {
            result += ',';
        }
        result += "\"clientToken\":\"" + client_token_ + "\"";
    }
    result += '}';

    return result;
}

std::string AWSJobs::SerializeClientTokenPayload()
{
    if (!client_token_.empty())
    {
        return "{\"clientToken\":\"" + client_token_ + "\"}";
    }

    return "{}";
}

unique_ptr<string> AWSJobs::SendJobsQuery(JobExecutionTopicType topicType, const string &jobId) 
{
    uint16_t packet_id = 0;
    unique_ptr<string> jobTopic = GetJobTopic(topicType,JobExecutionTopicReplyType::JOB_REQUEST_TYPE, jobId);

    if (jobTopic != nullptr)
    {
        string payload = SerializeClientTokenPayload();
        mgos_mqtt_pub(jobTopic->c_str(), payload.c_str(), payload.size(), (int)qos_, 0);
    }
    return jobTopic;
}

void AWSJobs::SendJobsUpdate(const string &jobId,
                                    JobExecutionStatus status,
                                    const map<string, string> &statusDetailsMap,
                                    int64_t expectedVersion,    // set to 0 to ignore
                                    int64_t executionNumber,    // set to 0 to ignore
                                    bool includeJobExecutionState,
                                    bool includeJobDocument) 
{
    uint16_t packet_id = 0;
    unique_ptr<string> jobTopic = GetJobTopic(JobExecutionTopicType::JOB_UPDATE_TOPIC, 
                                              JobExecutionTopicReplyType::JOB_REQUEST_TYPE, jobId);
    LOG(LL_INFO,("jobTopic:%s", jobTopic->c_str()));

    if (jobTopic != nullptr) 
    {
        string payload = SerializeJobExecutionUpdatePayload(status, statusDetailsMap, expectedVersion, executionNumber,
                                                                            includeJobExecutionState, includeJobDocument);
        LOG(LL_INFO,("Update payload:%s", payload.c_str()));        
        mgos_mqtt_pub(jobTopic->c_str(), payload.c_str(), payload.size(), (int)qos_, 0);    
    }
}

bool mgos_aws_jobs_init()
{
	return true;
}