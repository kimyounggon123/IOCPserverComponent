#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef _CLIENTINFORMATIONS_H
#define _CLIENTINFORMATIONS_H

#include <unordered_map>
#include <WinSock2.h>
#include <MSWSock.h>

#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "ws2_32.lib")

#include "ThreadSafeQueue.h"
#include "Logs.h"
#include "Packet.h"

// session control class
enum class IO_TYPE { Request, Response };
struct SOCKETINFO;
struct IO_CONTEXT {
	OVERLAPPED overlapped; // 반드시 첫 멤버로 설정해야 함
	IO_TYPE ioType;

	WSABUF wsabuf;
	char IO_buffer[2 * BUFFERSIZE];

	SOCKETINFO* owner;


	IO_CONTEXT() : ioType(IO_TYPE::Request), owner(nullptr), IO_buffer{}
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		wsabuf.buf = IO_buffer;
		wsabuf.len = 2 * BUFFERSIZE;
	}

	IO_CONTEXT(IO_TYPE type, SOCKETINFO* owner) : ioType(type), owner(owner),
		IO_buffer{}
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		wsabuf.buf = IO_buffer;
		wsabuf.len = 2 * BUFFERSIZE;
	}

	~IO_CONTEXT()
	{
		owner = nullptr;
	}

	void reset_overlapped(char* buf = nullptr, ULONG len = 2 * BUFFERSIZE)
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		wsabuf.buf = IO_buffer;
		wsabuf.len = len;
	}
};

enum class SESSION_TYPE { TCP, UDP };
struct SOCKETINFO {
	int id; // 일단 임시로 next id 형태로 등록하지만 원래는 DB에서 가져와야 함.
	ULONGLONG lastActive;
	SOCKET sock;
	SOCKADDR_IN addr;

	SESSION_TYPE sessionType;

	std::atomic<bool> acceptCompleted;
	std::atomic<int> responseCount;

	HANDLE hEvent;

	IO_CONTEXT request;
	IO_CONTEXT response;

	SOCKETINFO(SESSION_TYPE sessionType)
		: id(0),
		lastActive(GetTickCount64()),
		sessionType(sessionType),
		acceptCompleted(false),
		responseCount(0),
		sock(INVALID_SOCKET),
		addr{}
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

	DWORD waitSendEvent() { return WaitForSingleObject(hEvent, 1000); }
	void setSendEvent() { SetEvent(hEvent); }

	void updateActivity() { lastActive = GetTickCount64(); }

	void addResponseCount() { responseCount.fetch_add(1); }
	void subResponseCount() { responseCount.fetch_sub(1); }

	void cleanupSession()
	{
		CancelIoEx((HANDLE)sock, NULL);
		shutdown(sock, SD_BOTH);
		closesocket(sock);
	}
};



// SOCKETINFO containor
class Room
{
	int roomID;
	std::atomic<int> next_id; // 임시용. 원래는 DB에 저장된 id를 입력해야 해서 이 부분이 불필요함.
	std::atomic<int> countClient;
	int maxClientsNum;

	std::unordered_map<int, SOCKETINFO*> client_map; // 현재 접속한 클라이언트 목록들
	CRITICAL_SECTION map_cs;

	std::vector<SOCKETINFO*>deletedClients;
	CRITICAL_SECTION deleteCS;

public:
	Room(int roomID, int maxClientsNum = -1) : roomID(roomID),
		next_id(0), countClient(0), maxClientsNum(maxClientsNum)
	{
		InitializeCriticalSection(&map_cs);
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
		DeleteCriticalSection(&deleteCS);
	}

	bool input_socketinfo(SOCKETINFO* client_info);

	bool find_socketinfo(int id, SOCKETINFO*& found);

	bool socketinfo_isin_here(int id);
	bool delete_socketinfo(int id);

	void destroyInvalidSOCKETINFO();

	void delete_all();

	size_t getDeleteNum() const { return deletedClients.size(); }
	int getClientCount() const { return countClient; }

	void EnterCriticalOutSide() { EnterCriticalSection(&map_cs); }
	void LeaveCriticalOutSide() { LeaveCriticalSection(&map_cs); }

	void CopyMemberPointers(std::vector<SOCKETINFO*>& out);
};

class IOCPSessionManager : public Room
{
	static IOCPSessionManager* instance;
	IOCPSessionManager(): Room(0)
	{ }
public:
	static IOCPSessionManager& getInstance()
	{
		if (instance == nullptr) instance = new IOCPSessionManager;
		return *instance;
	}
};

#endif