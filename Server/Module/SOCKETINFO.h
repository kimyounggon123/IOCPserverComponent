#ifndef SOCKETINFO_H
#define SOCKETINFO_H
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>

#include <Ws2tcpip.h>  // for inet_pton or InetPton
#include <MSWSock.h>
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "ws2_32.lib")

#include <Windows.h>


#include "stdafx.h"
#include <unordered_map>
#include "Thread/ThreadSafeQueue.h"
#define IO_BUFFER_LEN 3 * 1024

// session control class
enum class IO_TYPE { Request, Response };

struct SOCKETINFO;
struct IO_CONTEXT {
	OVERLAPPED overlapped; // 반드시 첫 멤버로 설정해야 함
	IO_TYPE ioType;

	WSABUF wsabuf;
	char IO_buffer[IO_BUFFER_LEN];

	SOCKETINFO* owner;


	IO_CONTEXT() : ioType(IO_TYPE::Request), owner(nullptr), IO_buffer{}
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		memset(&IO_buffer, 0, IO_BUFFER_LEN);
		wsabuf.buf = IO_buffer;
		wsabuf.len = IO_BUFFER_LEN;
	}

	IO_CONTEXT(IO_TYPE type, SOCKETINFO* owner) : ioType(type), owner(owner),
		IO_buffer{}
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		memset(&IO_buffer, 0, IO_BUFFER_LEN);
		wsabuf.buf = IO_buffer;
		wsabuf.len = IO_BUFFER_LEN;
	}

	~IO_CONTEXT()
	{
		owner = nullptr;
	}

	void reset_overlapped(char* buf = nullptr, ULONG len = IO_BUFFER_LEN, bool ClearIO_buffer = false)
	{
		if (len > IO_BUFFER_LEN || len < 1) len = IO_BUFFER_LEN;
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		if (ClearIO_buffer) memset(&IO_buffer, 0, len);
		wsabuf.buf = IO_buffer;
		wsabuf.len = len;
	}
};


#define MAX_HEARTHBEATS 6000
#define MAX_REQUEST_COUNT_IN_TIME_TCP 50
#define MAX_REQUEST_COUNT_IN_TIME_UDP 250
#define MAX_DENY_COUNT 10

enum class SESSION_TYPE { TCP, UDP };
struct SOCKETINFO {
	int id; // 일단 임시로 next id 형태로 등록하지만 원래는 DB에서 가져와야 함.

	LONGLONG hearthBeats;
	SOCKET sock;
	SOCKADDR_IN addr;

	SESSION_TYPE sessionType;

	std::atomic<bool> acceptCompleted; 
	std::atomic<int> responseCount; // 남아있는 전송 task 양, 0이면 삭제 가능.

	HANDLE hEvent;

	IO_CONTEXT request;
	IO_CONTEXT response;

	const bool isDummy;  // 브로드캐스트용, 기타 필요한 경우 true
	std::atomic<bool> inUdpUse; // 해당 info가 dummy인데 UDP Process를 진행하고 있다면

	// Attack 방지
	std::atomic<unsigned int> CURR_TCP_REQUEST_COUNT;
	std::atomic<unsigned int> CURR_UDP_REQUEST_COUNT;

	std::atomic<unsigned int> DENY_COUNT;
	std::atomic<bool> isBlocked;

	std::vector<int> roomIDlist; // 현재 속한 room의 주소

	SOCKETINFO(SESSION_TYPE sessionType, bool isDummy = false)
		: id(0),
		hearthBeats(0),
		sessionType(sessionType),
		acceptCompleted(false),
		responseCount(0),
		sock(INVALID_SOCKET),
		addr{},
		isDummy(isDummy),
		inUdpUse(false),
		CURR_TCP_REQUEST_COUNT(0), CURR_UDP_REQUEST_COUNT(0), DENY_COUNT(0),
		isBlocked(false)
	{
		hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hEvent != NULL) SetEvent(hEvent);

		// 모든 멤버 초기화 이후에 생성
		request = IO_CONTEXT(IO_TYPE::Request, this);
		response = IO_CONTEXT(IO_TYPE::Response, this);
	}

	~SOCKETINFO()
	{
		request.owner = nullptr;
		response.owner = nullptr;
		CloseHandle(hEvent);
	}

	void cleanupSession()
	{
		CancelIoEx((HANDLE)sock, NULL);
		shutdown(sock, SD_BOTH);
		closesocket(sock);
	}


	DWORD waitSendEvent() { return WaitForSingleObject(hEvent, 1000); }
	void setSendEvent() { SetEvent(hEvent); }

	void ResetHearthBeats() { hearthBeats = 0; }
	void AddHearthBeats(const uint32_t count) { hearthBeats += count; }

	void addResponseCount() { responseCount.fetch_add(1); }
	void subResponseCount() { responseCount.fetch_sub(1); }

	void AddRequestCountTCP()
	{
		CURR_TCP_REQUEST_COUNT.fetch_add(1);	
	}
	void AddRequestCountUDP()
	{
		CURR_UDP_REQUEST_COUNT.fetch_add(1);	
	}

	bool CheckFullRequestProcessTCP()
	{

		if (CURR_TCP_REQUEST_COUNT.load() >= MAX_REQUEST_COUNT_IN_TIME_TCP)
			DENY_COUNT.fetch_add(1);
		if (DENY_COUNT.load() >= MAX_DENY_COUNT)
			isBlocked.store(true);

		return true;
	}
	bool CheckFullRequestProcessUDP()
	{
		if (CURR_UDP_REQUEST_COUNT.load() >= MAX_REQUEST_COUNT_IN_TIME_UDP)
			DENY_COUNT.fetch_add(1);
		if (DENY_COUNT.load() >= MAX_DENY_COUNT)
			isBlocked.store(true);

		return true;
	}

	void ResetRequestCount()
	{
		CURR_TCP_REQUEST_COUNT.store(0);
		CURR_UDP_REQUEST_COUNT.store(0);
	}

};


#include <ctime>
#include <chrono>
// SOCKETINFO containor
#define MAX_DELTA_EACH_ROOM 16
const auto target_duration = std::chrono::milliseconds(MAX_DELTA_EACH_ROOM); // 기준 delta

class Room
{
	int roomID;
	std::atomic<int> next_id; // 임시용. 원래는 DB에 저장된 id를 입력해야 해서 이 부분이 불필요함.
	std::atomic<int> countClient;
	int maxClientsNum;

protected:
	LONGLONG delta;
	std::chrono::steady_clock::time_point last_tick;

	std::unordered_map<int, SOCKETINFO*> client_map; // 현재 접속한 클라이언트 목록들
	CRITICAL_SECTION map_cs;

	std::vector<SOCKADDR_IN> udpTargets; // UDP 브로드캐스팅용
	CRITICAL_SECTION udpCS;

	std::vector<SOCKETINFO*>deletedClients;
	CRITICAL_SECTION deleteCS;

	virtual void Update()
	{

	};
public:
	Room(int roomID, int maxClientsNum = -1) : roomID(roomID),
		next_id(0), countClient(0), maxClientsNum(maxClientsNum),
		delta(0), last_tick(std::chrono::steady_clock::now())
	{
		InitializeCriticalSection(&map_cs);
		InitializeCriticalSection(&udpCS);
		InitializeCriticalSection(&deleteCS);
	}

	// deny copy
	Room(const Room&) = delete;
	Room& operator=(const Room&) = delete;

	~Room()
	{
		delete_all();
		destroyInvalidSOCKETINFO();
		DeleteCriticalSection(&map_cs);
		DeleteCriticalSection(&udpCS);
		DeleteCriticalSection(&deleteCS);
	}

	// virtual 
	void UpdateWithFrame(bool freeFlag = false);

	void AddDelta(int32_t frame) { delta += frame; }


	// TCP
	bool input_socketinfo(SOCKETINFO* client_info);
	bool find_socketinfo(int id, SOCKETINFO*& found);
	bool socketinfo_isin_here(int id);
	bool delete_socketinfo(int id); // 삭제 flag를 조정함
	void destroyInvalidSOCKETINFO(bool freeFlag = false); // 실제 containor에서 삭제. freeFlag = 완전히 free할 것인가?(실 소유한 클래스에서만 true)

	size_t getDeleteNum() const { return deletedClients.size(); }
	int getClientCount() const { return countClient; }

	void CopySOCKETINFOPointers(std::vector<SOCKETINFO*>& out);


	// UDP
	bool inputUDPsession(const SOCKADDR_IN& addr);
	bool SOCKADDRisinHere(const SOCKADDR_IN& addr);
	bool deleteUDPsession(const SOCKADDR_IN& addr);
	void CopyMemberPointersUDP(std::vector<SOCKADDR_IN>& out);

	void delete_all();

	LONGLONG GetDelta() { return delta; }

};

#include <mutex>
// 모든 socketinfo의 실 소유권을 보유하는 manager 클래스
class IOCPSessionManager : public Room
{
	static IOCPSessionManager* instance;


	LockPool<SOCKETINFO> DummySOCKETINFOpool;
	
	//CRITICAL_SECTION pool_cs;
	//std::mutex pool_mtx;

	bool exit_flag;
	IOCPSessionManager(): DummySOCKETINFOpool(1000),
		Room(0), exit_flag(false) 
	{
		//InitializeCriticalSection(&pool_cs);
	}

public:
	static IOCPSessionManager& getInstance()
	{
		if (instance == nullptr) instance = new IOCPSessionManager;
		return *instance;
	}
	~IOCPSessionManager()
	{
		//DeleteCriticalSection(&pool_cs);
		deleteUDPSOCKET();
	}
	void Update() override;

	// initialize
	bool MakeSOCKETINFOforUDPbroadcast(int count); // udp용 broadcast

	// 삭제
	void deleteUDPSOCKET();

	// 현재 풀 크기
	size_t getUDPSocketPoolSize();

	bool GetSOCKETINFOforUDP(SOCKETINFO*& output); // Udp 전송에 필요한 임시 SOCKETINFO 빌리기
	void ReleaseSOCKETINFOforUDP(SOCKETINFO*&& input);

	void Quit() { exit_flag = true; }
};


class RoomManager
{
	static RoomManager* instance;
	bool isInitialized;

	std::atomic<int> nextID;
	IOCPSessionManager& allClients;

	CRITICAL_SECTION map_cs;
	std::unordered_map<int, Room*> rooms;

	RoomManager() :
		nextID(1), isInitialized(false),
		allClients(IOCPSessionManager::getInstance())
	{
		InitializeCriticalSection(&map_cs);
	}
public:
	~RoomManager()
	{
		allClients.deleteUDPSOCKET();
		DeleteCriticalSection(&map_cs);
	}
	static RoomManager& getInstance()
	{
		if (instance == nullptr) instance = new RoomManager;
		return *instance;
	}

	bool Initialize();

	Room* AddRoom(int max_client = -1);
	bool DeleteRoom(int ID);
	void DeleteAll();
	Room* GetRoom(int ID = 0);
	void Update();

	bool DeleteClientFromHere(const SOCKETINFO* info);

	size_t GetDeleteReservationCount() const { return allClients.getDeleteNum(); }

	IOCPSessionManager& GetSessionsOwner() { return allClients; }
};


#endif