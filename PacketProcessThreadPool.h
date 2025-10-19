#ifndef _PACKETPROCESSTHREADPOOL_H
#define _PACKETPROCESSTHREADPOOL_H

#include "ThreadPool.h"
#include "PacketProcess.h"
#include "Dispatcher.h"

class PacketProcessThreadPool : public ThreadPool
{

	PacketProcess* packetProcess; // ���� �۾� Ŭ����
	Dispatcher& dispatcher;

	unsigned int workLoop() override;
public:
	PacketProcessThreadPool(int poolCapacity, PacketProcess* process = nullptr);
	bool initialize() override;
};
#endif

