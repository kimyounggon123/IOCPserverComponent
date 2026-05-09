#ifndef _PACKETPROCESSTHREADPOOL_H
#define _PACKETPROCESSTHREADPOOL_H


#include "Dispatcher.h"
#include "Thread/ThreadPool.h"
#include "PacketProcess.h"

class PacketProcessThreadPool : public ThreadPool
{
	PacketProcess* packetProcess; // 실제 작업 클래스
	DispatcherHub& dispatcher;

	unsigned int workLoop() override;
public:
	PacketProcessThreadPool(int poolCapacity, PacketProcess* process = nullptr);
	bool initialize() override;
};
#endif

