#pragma once


#include <boost/json/value.hpp>
namespace json = boost::json;

#include "util.hpp"

namespace btd {

struct sessionStats
{
    int64_t bytesRecv;
    int64_t bytesSent;
    int64_t bytesDataRecv;
    int64_t bytesDataSent;
    int64_t rateRecv;
    int64_t rateSent;
    int64_t bytesFailed;
    int64_t bytesWasted;
    int64_t bytesQueued;
    int numChecking;
    int numDownloading;
    int numSeeding;
    int numStopped;
    int numQueued;
    int numError;
    int numPeersConnected;
    int numPeersHalfOpen;
    int limitUpQueue;
    int limitDownQueue;
    int queuedTrackerAnnounces;
    bool hasIncoming;
    bool isPaused;
    time_t uptime;
    uint64_t uptimeMs;

  public:
    json::object
    to_json_object() const
    {
    	json::object obj({
    		{"rates", rateRecv + rateSent}
    	});

	    if (bytesRecv > 0) obj.emplace("bytesRecv", bytesRecv);
	    if (bytesSent > 0) obj.emplace("bytesSent", bytesSent);
	    if (bytesDataRecv > 0) obj.emplace("bytesDataRecv", bytesDataRecv);
	    if (bytesDataSent > 0) obj.emplace("bytesDataSent", bytesDataSent);
	    if (rateRecv > 0) obj.emplace("rateRecv", rateRecv);
	    if (rateSent > 0) obj.emplace("rateSent", rateSent);
	    if (bytesFailed > 0) obj.emplace("bytesFailed", bytesFailed);
	    if (bytesQueued > 0) obj.emplace("bytesQueued", bytesQueued);
	    if (bytesWasted > 0) obj.emplace("bytesWasted", bytesWasted);
	    if (numChecking > 0) obj.emplace("numChecking", numChecking);
	    if (numDownloading > 0) obj.emplace("numDownloading", numDownloading);
	    if (numSeeding > 0) obj.emplace("numSeeding", numSeeding);
	    if (numStopped > 0) obj.emplace("numStopped", numStopped);
	    if (numQueued > 0) obj.emplace("numQueued", numQueued);
	    if (numError > 0) obj.emplace("numError", numError);
	    if (numPeersConnected > 0) obj.emplace("numPeersConnected", numPeersConnected);
	    if (numPeersHalfOpen > 0) obj.emplace("numPeersHalfOpen", numPeersHalfOpen);
	    if (limitUpQueue > 0) obj.emplace("limitUpQueue", limitUpQueue);
	    if (limitDownQueue > 0) obj.emplace("limitDownQueue", limitDownQueue);
	    if (hasIncoming) obj.emplace("hasIncoming", true);
	    if (isPaused) obj.emplace("isPaused", true);
	    int activeCount = numChecking + numDownloading + numSeeding;
	    int puasedCount = numQueued + numStopped;
	    if (activeCount > 0) obj.emplace("activeCount", activeCount);
	    if (puasedCount > 0) obj.emplace("puasedCount", puasedCount);
	    obj.emplace("taskCount", activeCount + puasedCount);
	    if (uptime > 0) obj.emplace("uptime", uptime);
	    if (uptimeMs > 0) obj.emplace("uptimeMs", uptimeMs);

	    return std::move(obj);
    }

};

inline void
tag_invoke( json::value_from_tag, json::value& jv, sessionStats const& ss )
{
	jv = ss.to_json_object();
}

} // namespace btd
