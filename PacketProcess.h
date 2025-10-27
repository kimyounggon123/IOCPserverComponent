#ifndef _PACKETPROCESS_H
#define _PACKETPROCESS_H


#include <functional>
#include <unordered_map>
#include "Dispatcher.h"
#include "Logs.h"
#include "DataToConnectWithClient.h"

// ÇØ½Ì¿ë
struct PacketProcessKey {
	PacketType type;
	PacketResult result;

	bool operator==(const PacketProcessKey& other) const {
		return type == other.type && result == other.result;
	}
};
struct PacketProcessKeyHash {
	std::size_t operator()(const PacketProcessKey& k) const 
	{
		return std::hash<int>()(static_cast<int>(k.type)) ^
			(std::hash<int>()(static_cast<int>(k.result)) << 1);
	}
};


using HandlerFunc = std::function<bool(TaskQueueInput*)>;

class PacketProcess
{
	bool testPacketFunc(TaskQueueInput* input);
	bool closedServerLogic(TaskQueueInput* input);
protected:
	static std::unordered_map<PacketProcessKey, HandlerFunc, PacketProcessKeyHash> func_map;
	bool isInitialized;
	Logs& logs;

	std::string hash_function(const char* key, const char* to_hash);
public:
	PacketProcess() : logs(Logs::getInstance()) , isInitialized(false)
	{}

	virtual ~PacketProcess()
	{}

	virtual void initialize();
	virtual bool registerThreadLocal() { return true; }
	HandlerFunc getFunc(TaskQueueInput* input);

	bool getInitialized() noexcept { return isInitialized; }
	
};

#endif
