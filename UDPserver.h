#ifndef _UDPSERVER_H
#define _UDPSERVER_H



#include <WinSock2.h>
#include <WS2tcpip.h>
#include "Logs.h"
class UDPserver
{
	WSADATA wsadata; // �Ŀ� iocp�� �����ұ�?
	SOCKET udpSocket;
	SOCKADDR_IN serverAddr;

	USHORT port;
	bool exit_flag;

	Logs& logs;
public:
	UDPserver(USHORT port);
	~UDPserver();


	bool initialize();

	void openServer();
};

#endif