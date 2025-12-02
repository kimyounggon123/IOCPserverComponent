#ifndef _IOCPSERVER_H
#define _IOCPSERVER_H

#include "PacketProcessThreadPool.h"
#include <Ws2tcpip.h>  // for inet_pton or InetPton
#include <mswsock.h>

// get IO result / recv only
class IOCPserver
{

	std::atomic<bool> exit_flag;
	std::atomic<bool> isGateClosed;

	ULONG_PTR serverPtr;

	// IPv4
	SOCKADDR_IN addrV4;

	USHORT portTCP;
	SOCKET sockTCP;// UDP 전용 소켓을 하나 더 만들어라. 

	USHORT portUDP; // -1 : invalid
	SOCKET sockUDP; // 



	HANDLE IOCP;
	size_t countThreads;

	Logs& logs;
	IOCPSessionManager& sessionManager;
	Dispatcher& dispatcher;

	std::vector<HANDLE> workerThreads;

	// TCP
	bool TCPLogic(SOCKETINFO* socketinfo, IO_CONTEXT* io, INT retval, DWORD cbTransferred);
	bool makeClientSocket(); 
	bool welcomeClient(SOCKETINFO* ptr);
	bool makePacketFromIOresult(SOCKETINFO* ptr, DWORD cbTransferred);
	bool recvFromSOCKETINFO(SOCKETINFO* ptr);



	// UDP
	bool UDPLogic(SOCKETINFO* socketinfo, IO_CONTEXT* io, INT retval, DWORD cbTransferred);
	bool MakeSocketInfoToRecvFrom();
	bool RecvUDP(SOCKETINFO* ptr); // WSArecv
	bool WelcomeToUDP(SOCKETINFO* ptr); // socketinfo pool에 넣음
	bool MakePacketUDP(SOCKETINFO* ptr, DWORD cbTransferred); // 패킷 제작

public:
	IOCPserver(USHORT TCPport, USHORT UDPport, PacketProcessThreadPool* packetThreadPool);
	~IOCPserver();

	bool initialize();	// put threads in IOCP 
	bool Start();
	void openServerGate();
	void closeServerGate();
	void Quit();
	
	static unsigned int WINAPI workerThread(LPVOID server_info); 

	SOCKET GetUDPSocket() const { return sockUDP; }
};



class SendManager : public ThreadPool
{
	Dispatcher& dispatcher;
	SOCKET sockUDP; // UDP 전용

	unsigned int workLoop() override;
public:
	SendManager(int poolCapacity, SOCKET sockUDP) // udpSock은 TCP를 쓸 거면 invalid_socket
		: ThreadPool(poolCapacity), sockUDP(sockUDP),
		dispatcher(Dispatcher::getInstance())
	{}

	bool initialize() override;
};


#endif