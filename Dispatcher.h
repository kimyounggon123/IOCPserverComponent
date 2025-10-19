#ifndef _DISPATCHER_H
#define _DISPATCHER_H


#include "SOCKETINFO.h"
#include "Packet.h"
#include "ThreadSafeQueue.h"


struct TaskQueueInput
{
	SOCKETINFO* sessionInfo;
	Packet* packet;

public:
	TaskQueueInput(SOCKETINFO* sessionInfo = nullptr) : sessionInfo(sessionInfo), packet(new Packet(false))
	{}
	TaskQueueInput& operator=(const TaskQueueInput& other)
	{
		if (this != &other) {
			sessionInfo = other.sessionInfo;
			packet = other.packet;
		}
		return *this;
	}

	bool isInvalid() { return this == nullptr || sessionInfo == nullptr || packet == nullptr; }

	~TaskQueueInput()
	{
		SAFE_FREE(packet);
	}
};


enum class QueueInformation
{
	PacketProcess,
	Database,
	Send
};


// 작업 큐 디스패쳐
class Dispatcher
{
	static Dispatcher* instance;
	Dispatcher(): taskPool(true),
		taskProcessWaiting(true, 100), taskToSendDB(true, 100), taskToSendClient(true, 100)
	{}

	ThreadSafeStack<TaskQueueInput*> taskPool;

	ThreadSafeQueue<TaskQueueInput*> taskProcessWaiting; // server에서 받아온 패킷, 세션 정보
	// result queues
	ThreadSafeQueue<TaskQueueInput*> taskToSendDB; // DB 전송을 위한 작업 큐 / 만약 결과가 DB 작업을 요구한다면 여기서 패킷 검사 후 input
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