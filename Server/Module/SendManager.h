

#include "SOCKETINFO.h"
#include "Dispatcher.h"
#include "Thread/ThreadPool.h"

class SendManager : public ThreadPool
{
	DispatcherHub& dispatcher;
	SOCKET sockUDP; // UDP 전용

	unsigned int workLoop() override;
public:
	SendManager(int poolCapacity, SOCKET sockUDP) // udpSock은 TCP를 쓸 거면 invalid_socket
		: ThreadPool(poolCapacity), sockUDP(sockUDP),
		dispatcher(DispatcherHub::getInstance())
	{
	}

	bool initialize() override;
};