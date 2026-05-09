#ifndef _IOCPSERVER_H
#define _IOCPSERVER_H

#include "SOCKETINFO.h"
#include "PacketProcessThreadPool.h"


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
	RoomManager& roomManager;
	DispatcherHub& dispatcher;

	std::vector<HANDLE> workerThreads;

	// TCP
	bool TCPLogic(SOCKETINFO* socketinfo, IO_CONTEXT* io, INT retval, DWORD cbTransferred);
	bool LeaveServer(SOCKETINFO* ptr);
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
	
	void WaitThreadClosing();
	static unsigned int WINAPI workerThread(LPVOID server_info); 

	SOCKET GetUDPSocket() const { return sockUDP; }
};


#endif