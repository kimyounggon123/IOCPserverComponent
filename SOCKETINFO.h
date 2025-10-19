#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef _CLIENTINFORMATIONS_H
#define _CLIENTINFORMATIONS_H

#include "Packet.h"
#include <unordered_map>
#include <WinSock2.h>
#include <MSWSock.h>

#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "ws2_32.lib")

#include "ThreadSafeQueue.h"
#include "Logs.h"

// session control class
enum class IO_TYPE { Request, Response };
struct SOCKETINFO;
struct IO_CONTEXT {
	OVERLAPPED overlapped; // 반드시 첫 멤버로 설정해야 함
	IO_TYPE ioType;

	WSABUF wsabuf;
	char IO_buffer[1042];

	SOCKETINFO* owner;

	IO_CONTEXT(IO_TYPE type, SOCKETINFO* owner) : ioType(type), owner(owner),
		IO_buffer{}
	{
		memset(&overlapped, 0, sizeof(OVERLAPPED));
		wsabuf.buf = IO_buffer;
		wsabuf.len = 2 * BUFFERSIZE;
	}

	~IO_CONTEXT()
	{}

	IO_CONTEXT(const IO_CONTEXT&) = delete;
	IO_CONTEXT& operator=(const IO_CONTEXT&) = delete;

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

	HANDLE hEvent;

	SOCKETINFO() :
		id(0), lastActive(GetTickCount64()),
		request(IO_TYPE::Request, this), response(IO_TYPE::Response, this), acceptCompleted(false),
		sock(INVALID_SOCKET), addr{}
	{
		hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		SetEvent(hEvent);
		//sock = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	}
	~SOCKETINFO()
	{
		shutdown(sock, SD_BOTH);
		closesocket(sock);
		sock = INVALID_SOCKET;
		addr = {};
		CloseHandle(hEvent);
	}

	bool response_proc();

	DWORD waitEvent() { return WaitForSingleObject(hEvent, INFINITE); }
	void setEvent() { SetEvent(hEvent); }

	void updateActivity() { lastActive = GetTickCount64(); }
};

struct UDPsession {
	int id;
	SOCKADDR_IN addr;
	ULONGLONG lastActive;


	UDPsession(const SOCKADDR_IN& addr) :
		id(0), addr(addr), lastActive(GetTickCount64())
	{}

	bool response_proc();
	void updateActivity() { lastActive = GetTickCount64(); }
};


// SOCKETINFO containor
template <typename T>
class ClientSessionManager
{
	int next_id;
	int countClient;
	std::unordered_map<int, T>	client_map; // 현재 접속한 클라이언트 목록들
	CRITICAL_SECTION map_cs;

	static ClientSessionManager<T>* instance;
	ClientSessionManager() : next_id(1), countClient(0)
	{
		InitializeCriticalSection(&map_cs);
	}
public:

	// deny copy
	ClientSessionManager(const ClientSessionManager&) = delete;
	ClientSessionManager& operator=(const ClientSessionManager&) = delete;

	static ClientSessionManager<T>& getInstance()
	{
		if (instance == nullptr) instance = new ClientSessionManager<T>;
		return *instance;
	}
	~ClientSessionManager()
	{
		delete_all();
		DeleteCriticalSection(&map_cs);
	}


	bool input_socketinfo(T client_info)
	{
		EnterCriticalSection(&map_cs);

		client_info->id = next_id; // input current integer id at clientinfo
		auto pair = client_map.emplace(client_info->id, client_info);
		if (!pair.second) // 삽입 실패 시
		{
			LeaveCriticalSection(&map_cs);
			return false;
		}
		next_id++; // increase next id
		countClient++;
		LeaveCriticalSection(&map_cs);

		return true;
	}

	bool find_socketinfo(int id, T& found)
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

		auto it = client_map.find(id);
		if (it == client_map.end())
		{
			LeaveCriticalSection(&map_cs);
			return false;
		}

		delete it->second;
		client_map.erase(it);

		countClient--;
		LeaveCriticalSection(&map_cs);

		return true;
	}

	void search_and_destory()
	{
		EnterCriticalSection(&map_cs);

		for (auto it = client_map.begin(); it != client_map.end(); )
		{
			if (it->second->id <= 0)
			{
				delete it->second;
				client_map.erase(it);
			}
		}

		LeaveCriticalSection(&map_cs);
	}

	void delete_all()
	{
		EnterCriticalSection(&map_cs);
		for (auto it = client_map.begin(); it != client_map.end(); )
		{
			delete it->second;
			client_map.erase(it);
		}
		LeaveCriticalSection(&map_cs);
	}


	int getClientCount() const {
		return countClient;
	}
};

using IOCPSessionManager = ClientSessionManager<SOCKETINFO*>;
using UdpSessionManager = ClientSessionManager<UDPsession*>;
#endif