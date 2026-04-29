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

// 누구에게 전송할 것인가?
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

// 통신 기본 단위
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


	void copyFrom(const Task* other)
	{
		if (other == nullptr || this == other) return;
		sessionInfo = other->sessionInfo;
		udpInfo = other->udpInfo;
		sessionType = other->sessionType;
		target = other->target;
		packet->copyFrom(other->packet);
	}

	void copyFrom(const Task& other)
	{
		copyFrom(&other);
	}


	~Task()
	{
		SAFE_FREE(packet);
	}
};

using TaskPTR = std::unique_ptr<Task>;
using Pipe = ThreadSafeQueue<TaskPTR>;

class DispatcherUnit
{
	ThreadSafeStack<TaskPTR> taskPool; // 최대 작업 풀
	Pipe pipe; // server에서 받아온 패킷, 세션 정보
public:
	DispatcherUnit(DWORD timems = INFINITE):
		taskPool(timems),
		pipe(100)
	{}
	~DispatcherUnit()
	{
		UndoAll();
	}

	bool initialize(int poolCount = 1000, DWORD timemsPipe = 100);
	void UndoAll();

	bool pushPool(TaskPTR&& input);  // 다 쓴 정보 회수
	bool popPool(TaskPTR& output);	 // 정보 가져오기

	bool enqueue(TaskPTR&& input);    // into pipe
	bool dequeue(TaskPTR& output);  // from pipe

	bool isEmpty()
	{
		return taskPool.isEmpty();
	}


};

// 작업 큐 디스패쳐

/*
session -> pipe -> packetprocess
처리를 다 하면 
packetprocesspool 결과 본 후에 packet 복사 후 -> pipe -> session
*/

/*
enum class TaskInformation
{
	PacketProcess,
	Send
};

class Dispatcher
{
	bool isInitialized;

	std::unordered_map<int, DispatcherUnit*> dispatcherMap;

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
*/



struct DispatcherID
{
	static constexpr int32_t Base = 0; 
	static constexpr int32_t ServerToProcess = Base + 0;
	static constexpr int32_t ProcessToServer = Base + 1;
};

class DispatcherHub
{
	std::unordered_map<int32_t, DispatcherUnit*> dispatcherMap;

	void DestroyAll()
	{
		for (auto it = dispatcherMap.begin(); it != dispatcherMap.end(); it++)
			SAFE_FREE(it->second);
	}
	static DispatcherHub* instance;
	DispatcherHub()
	{}

public:
	static DispatcherHub& getInstance()
	{
		if (instance == nullptr) instance = new DispatcherHub;
		return *instance;
	}
	static void DeleteInstance()
	{
		if (instance == nullptr) return;
		SAFE_FREE(instance);
	}

	~DispatcherHub()
	{
		DestroyAll();
	}

	bool AddNewDispatcher(int32_t id, int poolCount = 1000, DWORD timemsPipe = 100)
	{
		if (dispatcherMap.count(id) != 0) return false;

		DispatcherUnit* newOne = new DispatcherUnit();
		if (newOne == nullptr) return false;

		newOne->initialize(poolCount, timemsPipe);
		dispatcherMap.emplace(id, newOne);
		return true;
	}

	bool ReturnTaskPTR(TaskPTR input, const int32_t id);
	bool BorrowTaskPTR(TaskPTR& output, const int32_t id);

	bool EnqueueTaskPTR(TaskPTR input, const int32_t id);
	bool DequeueTaskPTR(TaskPTR& output, const int32_t id);


	bool MoveTaskToOtherPipe(TaskPTR& ToCopy, const int32_t home, const int32_t where)
	{
		if (dispatcherMap.count(home) != 0 || dispatcherMap.count(where) != 0) return false;
		if (ToCopy == nullptr) return false;
		
		TaskPTR ToServer = nullptr;
		if (!BorrowTaskPTR(ToServer, DispatcherID::ProcessToServer)) return false;
		ToServer.get()->copyFrom(ToCopy.get());

		// 2. 기존 사용한 Task 반환
		if (!ReturnTaskPTR(std::move(ToCopy), DispatcherID::ServerToProcess)) return false;

		// 3. 이 후 복사한 데이터 서버 측으로 전송
		if (!EnqueueTaskPTR(std::move(ToServer), DispatcherID::ProcessToServer)) return false;

		return true;
	}

};
#endif