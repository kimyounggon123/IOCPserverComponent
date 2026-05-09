#ifndef _DISPATCHER_H
#define _DISPATCHER_H

#include "SOCKETINFO.h"
#include "Thread/ThreadSafeQueue.h"
#include "Packet/Packet.h"

enum class TARGET_TYPE
{
	Default,
	Player,	// 유저 간 1:1 통신
	Room,	// 전체 브로드캐스팅
};

// 누구에게 전송할 것인가?
struct Target
{
	TARGET_TYPE type; // 수신자의 타입
	SOCKETINFO* tcp; // 수신자 TCP 통신에 사용
	SOCKADDR_IN udp; // 수신자 UDP 통신에 사용
	Room* room; // TARGET_TYPE이 Room일 경우 해당 주체 사용

	Target(TARGET_TYPE type = TARGET_TYPE::Default): type(type), tcp(nullptr), udp{}, room(nullptr)
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
		type = TARGET_TYPE::Default;
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

	Target target; // 받을 주체
	SESSION_TYPE sessionType;

	Packet* packet; // 내용

	bool broadcastFlag;
	bool DBflag;
public:
	Task(SOCKETINFO* sessionInfo = nullptr, const SOCKADDR_IN& udpInfo = SOCKADDR_IN{}) :
		sessionType(SESSION_TYPE::TCP),
		sessionInfo(sessionInfo), udpInfo(udpInfo), 
		packet(new Packet()),
		broadcastFlag(false), DBflag(false)
	{}

	void Reset()
	{
		sessionInfo = nullptr;
		udpInfo = {};
		target.Reset();
		packet->CLEAR_PACKET();
		broadcastFlag = false;
		DBflag = false;
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
		broadcastFlag = other->broadcastFlag;
		DBflag = other->DBflag;
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

class DispatcherUnit
{
	ThreadSafePool<Task> pool;

	ThreadSafeQueue<Task*> pipe; // server에서 받아온 패킷, 세션 정보
public:
	DispatcherUnit(DWORD timems = INFINITE):
		pool(timems),
		pipe(100)
	{}
	~DispatcherUnit()
	{
		UndoAll();
	}

	bool initialize(int poolCount = 1000, DWORD timemsPipe = 100);
	void UndoAll();

	bool pushPool(Task*&& input);  // 다 쓴 정보 회수
	bool popPool(Task*& output);	 // 정보 가져오기

	bool enqueue(Task*&& input);    // into pipe
	bool dequeue(Task*& output);  // from pipe

	bool isEmpty()
	{
		return pool.isEmpty();
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
	static constexpr int32_t Broadcast = Base + 2;
	static constexpr int32_t Database = Base + 3;
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

	bool ReturnTaskPTR(Task*&& input, const int32_t id);
	bool BorrowTaskPTR(Task*& output, const int32_t id);

	bool EnqueueTaskPTR(Task*&& input, const int32_t id);
	bool DequeueTaskPTR(Task*& output, const int32_t id);


	/*
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
	*/

};
#endif