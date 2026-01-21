#ifndef _DISPATCHER_H
#define _DISPATCHER_H


#include "SOCKETINFO.h"
#include "Packet.h"
#include "ThreadSafeQueue.h"

enum class TARGET_TYPE 
{
	Single, // 단일 통신
	Room,	// 전체 브로드캐스팅
	Player	// Whisper 등의 특수 케이스
};

struct Target
{
	TARGET_TYPE type;
	SOCKETINFO* tcp;
	SOCKADDR_IN udp;
	Room* room;

	Target(): type(TARGET_TYPE::Single), tcp(nullptr), udp{}, room(nullptr)
	{}
	Target& operator=(const Target& other)
	{
		if (this != &other) {
			type = other.type;
			tcp = other.tcp;
			udp = other.udp;
			room = other.room;
		}
		return *this;
	}
	void Reset()
	{
		type = TARGET_TYPE::Single;
		tcp = nullptr;
		udp = {};
		room = nullptr;
	}
};

struct Task
{
	// 전송자 정보
	SOCKETINFO* sessionInfo;	// TCP 통신 전용
	SOCKADDR_IN udpInfo;		// UDP 통신 전용

	Target target;
	SESSION_TYPE sessionType;

	Packet* packet; // 내용

public:
	Task(SOCKETINFO* sessionInfo = nullptr, const SOCKADDR_IN& udpInfo = SOCKADDR_IN{}) :
		sessionType(SESSION_TYPE::TCP),
		sessionInfo(sessionInfo), udpInfo(udpInfo), 
		packet(new Packet())
	{}


	void Reset()
	{
		sessionInfo = nullptr;
		udpInfo = {};
		target.Reset();
		packet->CLEAR_PACKET();
	}

	bool isInvalid() { return sessionInfo == nullptr || packet == nullptr; }

	void copyFrom(const Task& other)
	{
		if (this == &other) return;
		sessionInfo = other.sessionInfo;
		udpInfo = other.udpInfo;
		sessionType = other.sessionType;
		target = other.target;
		packet->copyFrom(*other.packet);
	}

	~Task()
	{
		SAFE_FREE(packet);
	}
};


using TaskPTR = std::unique_ptr<Task>;
class TaskPool
{
	ThreadSafeStack<TaskPTR> taskPool; // 전체 풀

public:

	TaskPool(DWORD timems = INFINITE) : taskPool(timems)
	{}
	~TaskPool() = default;   // ← delete 필요 없음


	bool Initialize(int poolCount = 1000);
	bool push(TaskPTR task);
	bool pop(TaskPTR& out);
	bool isEmpty()
	{
		return taskPool.isEmpty();
	}
};


using Pipe = ThreadSafeQueue<TaskPTR>;
class DispatcherUnit
{
	TaskPool* taskPool;
	Pipe pipe; // server에서 받아온 패킷, 세션 정보
public:
	DispatcherUnit():
		taskPool(nullptr),
		pipe(100)
	{}
	~DispatcherUnit()
	{
		UndoAll();
		SAFE_FREE(taskPool);
	}

	bool initialize(int poolCount = 1000, DWORD timemsPipe = 100);
	void UndoAll();

	bool pushPool(TaskPTR&& input);		 // into pool
	bool popPool(TaskPTR& output);	 // from pool

	bool enqueue(TaskPTR&& input);    // into pipe
	bool dequeue(TaskPTR& output);  // from pipe

	bool isEmpty()
	{
		return taskPool->isEmpty();
	}


};

// 작업 큐 디스패쳐

/*
session -> pipe -> packetprocess
처리를 다 하면 
packetprocesspool 결과 본 후에 packet 복사 후 -> pipe -> session
*/


enum class TaskInformation
{
	PacketProcess,
	Send
};

class Dispatcher
{

	bool isInitialized;
	DispatcherUnit* taskWaiting;
	DispatcherUnit* taskSend;

	static Dispatcher* instance;
	Dispatcher():isInitialized(false),
		taskWaiting(nullptr), taskSend(nullptr)
	{}

public:
	static Dispatcher& getInstance()
	{
		if (instance == nullptr) instance = new Dispatcher;
		return *instance;
	}

	~Dispatcher()
	{
		SAFE_FREE(taskWaiting);
		SAFE_FREE(taskSend);
	}

	bool initialize();

	bool push(TaskPTR input, const TaskInformation& where);
	bool pop(TaskPTR& output, const TaskInformation& where);

	bool enqueue(TaskPTR input, const TaskInformation& where);
	bool dequeue(TaskPTR& output, const TaskInformation& where);

	bool ProcessToSession(TaskPTR processResult);

	bool isEmpty(const TaskInformation& where);

	
};


#endif