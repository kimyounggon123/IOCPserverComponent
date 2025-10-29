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


	IO_CONTEXT() : ioType(IO_TYPE::Request), owner(nullptr),
		IO_buffer{}
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

struct SOCKETINFO {
	int id; // unique integer id in this server
	ULONGLONG lastActive; // get last connection time

	SOCKET sock;
	SOCKADDR_IN addr;
	IO_CONTEXT request;
	IO_CONTEXT response;

	std::atomic<bool> acceptCompleted;
	std::atomic<int> requestCount;
	HANDLE hEvent;


	SOCKETINFO() :
		id(0), lastActive(GetTickCount64()),
		acceptCompleted(false), requestCount(0),
		sock(INVALID_SOCKET), addr{},
		request(IO_TYPE::Request, this),
		response(IO_TYPE::Response, this)
	{
		hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hEvent != NULL) SetEvent(hEvent);
	}

	~SOCKETINFO()
	{
		request.owner = nullptr;
		response.owner = nullptr;
		shutdown(sock, SD_BOTH);
		closesocket(sock);
		sock = INVALID_SOCKET;
		addr = {};
		CloseHandle(hEvent);
	}

	DWORD waitEvent() { return WaitForSingleObject(hEvent, INFINITE); }
	void setEvent() { SetEvent(hEvent); }

	void updateActivity() { lastActive = GetTickCount64(); }

	void addRequestCount() { requestCount.fetch_add(1); }
	void subRequestCount() { requestCount.fetch_sub(1); }
};

struct UDPsession {
	int id;
	SOCKADDR_IN addr;
	ULONGLONG lastActive;

	std::atomic<int> requestCount;
	UDPsession(const SOCKADDR_IN& addr) :
		id(0), addr(addr), lastActive(GetTickCount64())
	{}
	void updateActivity() { lastActive = GetTickCount64(); }
};


// SOCKETINFO containor
class IOCPSessionManager
{
	std::atomic<int> next_id;
	std::atomic<int> countClient;
	std::unordered_map<int, SOCKETINFO*>	client_map; // 현재 접속한 클라이언트 목록들
	CRITICAL_SECTION map_cs;

	std::vector<SOCKETINFO*>deletedClients;
	CRITICAL_SECTION deleteCS;

	static IOCPSessionManager* instance;
	IOCPSessionManager() : next_id(0), countClient(0)
	{
		InitializeCriticalSection(&map_cs);
		InitializeCriticalSection(&deleteCS);
	}
public:

	// deny copy
	IOCPSessionManager(const IOCPSessionManager&) = delete;
	IOCPSessionManager& operator=(const IOCPSessionManager&) = delete;

	static IOCPSessionManager& getInstance()
	{
		if (instance == nullptr) instance = new IOCPSessionManager;
		return *instance;
	}
	~IOCPSessionManager()
	{
		delete_all();
		destroyInvalidSOCKETINFO();
		DeleteCriticalSection(&map_cs);
		DeleteCriticalSection(&deleteCS);
	}


	bool input_socketinfo(SOCKETINFO* client_info)
	{
		EnterCriticalSection(&map_cs);

		int id = next_id.fetch_add(1); // 안전하게 id 할당
		client_info->id = id;

		auto pair = client_map.emplace(id, client_info);
		if (!pair.second) {
			LeaveCriticalSection(&map_cs);
			return false;
		}

		countClient.fetch_add(1);
		LeaveCriticalSection(&map_cs);
		return true;
	}

	bool find_socketinfo(int id, SOCKETINFO*& found)
	{
		bool result = false;
		EnterCriticalSection(&map_cs);

		auto it = client_map.find(id);
		if (it != client_map.end())
		{
			found = it->second;
			result = true;
		}

		LeaveCriticalSection(&map_cs);
		return result;
	}

	bool socketinfo_isin_here(int id)
	{
		bool result = false;
		EnterCriticalSection(&map_cs);

		auto it = client_map.find(id);

		if (it != client_map.end())	result = true;
		
		LeaveCriticalSection(&map_cs);

		return result;
	}

	bool delete_socketinfo(int id)
	{
		EnterCriticalSection(&map_cs);
		EnterCriticalSection(&deleteCS);

		bool result = false;

		auto it = client_map.find(id);
		if (it != client_map.end())
		{
			countClient.fetch_sub(1);
			deletedClients.push_back(it->second);
			result = true;
		}

		LeaveCriticalSection(&deleteCS);
		LeaveCriticalSection(&map_cs);
		return result;
	}

	void destroyInvalidSOCKETINFO()
	{
		EnterCriticalSection(&map_cs);
		EnterCriticalSection(&deleteCS);

		for (auto it = deletedClients.begin(); it != deletedClients.end(); )
		{
			SOCKETINFO* info = *it;

			if (info->requestCount.load() == 0)
			{
				client_map.erase(info->id);
				delete info;
				it = deletedClients.erase(it);
			}
			else
			{
				++it;
			}
		}
		LeaveCriticalSection(&deleteCS);
		LeaveCriticalSection(&map_cs);
	}

	void delete_all()
	{
		EnterCriticalSection(&map_cs);
		for (auto it = client_map.begin(); it != client_map.end(); )
		{
			delete it->second;
			it = client_map.erase(it);
		}
		LeaveCriticalSection(&map_cs);
	}


	int getClientCount() const { return countClient; }
};

#endif