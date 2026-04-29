#ifndef _COMPONENT_H
#define _COMPONENT_H


#include "PacketProcessThreadPool.h"
#include "PacketProcess.h"
#include "IOCPserver.h"
#include "Dispatcher.h"
#include "Logs.h"
#include "InputManager.h"

// Port list
#define CENTER_PORT 9000
#define SIGN_SERVERPORT 6000
#define BASIC_DB_PORT 7000

#define SIGN_SERVERPORT 6000
#define CHAT_SERVER_PORT 5000
#define GAME_PORT 4000


// 기본 1:1 통신용 
// 브로드캐스팅은 따로 상속 등으로 구현하세요.
class ServerFramework
{
	bool exit_flag;
	WSADATA wsadata;
	
protected:

	PacketProcess* packetProc; // PacketProcess 부분만 상속 받아서 확장시키기
	PacketProcessThreadPool* packetThreadPool;

	IOCPserver* iocp; USHORT portTCP; USHORT portUDP; // port = 0 -> invalid socket
	SendManager* sendManager;
	DispatcherHub& dispatcher;
	IOCPSessionManager& sessionManager;

	Logs& logs;
	InputManager& input;

	void showCommands();
	
	virtual bool Start();
	void WorkDebugger();
	virtual void Quit();

public:

	ServerFramework(PacketProcess* packetProc = nullptr, USHORT portTCP = 0, USHORT portUDP = 0):
		exit_flag(false),
		packetProc(packetProc), packetThreadPool(nullptr), iocp(nullptr), sendManager(nullptr),
		portTCP(portTCP), portUDP(portUDP), dispatcher(DispatcherHub::getInstance()), sessionManager(IOCPSessionManager::getInstance()),
		logs(Logs::getInstance()), input(InputManager::getInstance())
	{
		if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) return;
	}

	virtual ~ServerFramework()
	{
		WSACleanup();
		SAFE_FREE(iocp);
		SAFE_FREE(sendManager);
		SAFE_FREE(packetThreadPool);
		SAFE_FREE(packetProc);
	}

	virtual bool initialize();
	void Run();
};

#endif
