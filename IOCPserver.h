#ifndef _IOCPSERVER_H
#define _IOCPSERVER_H

#include "PacketProcessThreadPool.h"
#include <Ws2tcpip.h>  // for inet_pton or InetPton
#include <mswsock.h>


#define DB_SERVER_IP "127.0.0.1"


// get IO result / recv only
class IOCPserver
{
	std::atomic<bool> exit_flag;
	std::atomic<bool> isGateClosed;
	USHORT port;

	// IPv4
	SOCKET sockV4;
	SOCKADDR_IN addrV4;

	HANDLE IOCP;
	size_t countThreads;

	Logs& logs;
	IOCPSessionManager& sessionManager;
	Dispatcher& dispatcher;

	std::vector<HANDLE> workerThreads;

	bool makeClientSocket();
	bool welcomeClient(SOCKETINFO* ptr);

	bool makePacketFromIOresult(SOCKETINFO* ptr, DWORD cbTransferred);
	bool recvFromSOCKETINFO(SOCKETINFO* ptr);
public:

	IOCPserver(USHORT DBserverPort, PacketProcessThreadPool* packetThreadPool);
	~IOCPserver();

	bool initialize();	// put threads in IOCP 
	bool Start();
	void openServerGate();
	void closeServerGate();
	void Quit();
	
	static unsigned int WINAPI workerThread(LPVOID server_info);
};



class SendManager : public ThreadPool
{

	Dispatcher& dispatcher;

	unsigned int workLoop() override;
public:
	SendManager(int poolCapacity, PacketProcessThreadPool* pool) 
		: ThreadPool(poolCapacity), dispatcher(Dispatcher::getInstance())
	{}

	bool initialize() override;
};

// DBconnector 같은 경우에도 pool로 작업하는 것이 좋을까?
class DBconnector
{
	std::atomic<bool> exit_flag;

	USHORT serverPort;
	SOCKET sock;
	SOCKADDR_IN addr;

	HANDLE hThread;
	DWORD dwThreadID;

	char recv_buf[BUFFERSIZE + 1];
	char send_buf[BUFFERSIZE + 1];

	HANDLE hEvent;

	IOCPSessionManager& sessionManager;
	Logs& logs;
	Dispatcher& dispatcher;

	INT send_to_DB(); // 실제 패킷 삭제 함수
	INT recv_from_DB(); // 실제 패킷 생성 함수
	virtual bool make_pk_and_push(char* recv_buf, int recv_len, size_t& offset);

public:
	DBconnector(USHORT serverPort, PacketProcessThreadPool* packetThreadPool);
	~DBconnector();

	bool initialize();
	bool Start();
	void Quit();

	static unsigned int WINAPI workerThread(LPVOID lpParam);
};


#endif