#ifndef _DISPATCHER_H
#define _DISPATCHER_H


#include "SOCKETINFO.h"
#include "Packet.h"
#include "ThreadSafeQueue.h"


struct TaskQueueInput
{
	SOCKETINFO* sessionInfo;
	SOCKADDR_IN udpInfo;
	Packet* packet;

public:
	TaskQueueInput(SOCKETINFO* sessionInfo = nullptr, const SOCKADDR_IN& udpInfo = SOCKADDR_IN{}) :
		sessionInfo(sessionInfo), udpInfo(udpInfo), packet(new Packet())
	{}
	TaskQueueInput& operator=(const TaskQueueInput& other)
	{
		if (this != &other) {
			sessionInfo = other.sessionInfo; // session 정보는 deep copy하지 말도록.
			udpInfo = other.udpInfo;
			*packet = *other.packet;
		}
		return *this;
	}
	void InputInfo(SOCKETINFO* sockinfo, const SOCKADDR_IN& udpinfo)
	{
		sessionInfo = sockinfo;
		udpInfo = udpinfo;
	}
	bool isInvalid() { return sessionInfo == nullptr || packet == nullptr; }

	~TaskQueueInput()
	{
		SAFE_FREE(packet);
	}
};



enum class QueueInformation
{
	PacketProcess,
	Send
};


// 작업 큐 디스패쳐
class Dispatcher
{
	static Dispatcher* instance;
	Dispatcher(): taskPool(INFINITE),
		taskProcessWaiting(100), taskToSendClient(100)
	{}

	ThreadSafeStack<TaskQueueInput*> taskPool; // 전체 풀

	ThreadSafeQueue<TaskQueueInput*> taskProcessWaiting; // server에서 받아온 패킷, 세션 정보
	ThreadSafeQueue<TaskQueueInput*> taskToSendClient; // SendManager가 사용하는 큐 / 작업 완료 시 해당 큐에 input

public:
	static Dispatcher& getInstance()
	{
		if (instance == nullptr) instance = new Dispatcher;
		return *instance;
	}

	~Dispatcher()
	{
		undoAllQueue();

		while (!taskPool.isEmpty())
		{
			TaskQueueInput* delThis = nullptr;
			if (taskPool.pop(delThis))
				SAFE_FREE(delThis);
		}
	}
	bool initialize();
	void undoAllQueue();

	bool push(TaskQueueInput*& input);
	bool pop(TaskQueueInput*& output);

	bool enqueue(TaskQueueInput*& input, const QueueInformation& where);
	bool dequeue(TaskQueueInput*& output, const QueueInformation& where);
	bool isEmpty(const QueueInformation& where);
};

#endif