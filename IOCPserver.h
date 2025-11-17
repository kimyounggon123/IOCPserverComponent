#ifndef _IOCPSERVER_H
#define _IOCPSERVER_H

#include "PacketProcessThreadPool.h"
#include <Ws2tcpip.h>  // for inet_pton or InetPton
#include <mswsock.h>

// get IO result / recv only
class IOCPserver
{
	ULONG_PTR serverPtr;
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
	SendManager(int poolCapacity) 
		: ThreadPool(poolCapacity), dispatcher(Dispatcher::getInstance())
	{}

	bool initialize() override;
};


#endif