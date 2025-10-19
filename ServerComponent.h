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

class ServerComponent
{
	bool exit_flag;
	WSADATA wsadata;
	
	PacketProcess* packetProc; // PacketProcess 부분만 상속 받아서 확장시키기
	PacketProcessThreadPool* packetThreadPool;

	IOCPserver* iocp; USHORT serverPort;
	DBconnector* dbSender; USHORT dbSenderPort;
	SendManager* sendManager;
	Dispatcher& dispatcher;
	IOCPSessionManager& sessionManager;

	Logs& logs;
	InputManager& input;
	
	bool Start();
	void WorkDebugger();
	void Quit();

	void showCommands();
public:

	ServerComponent(PacketProcess* packetProc = nullptr, USHORT serverPort = 1000, USHORT dbSenderPort = 1001): exit_flag(false),
		packetProc(packetProc), packetThreadPool(nullptr), iocp(nullptr),
		dbSender(nullptr), sendManager(nullptr),
		serverPort(serverPort), dbSenderPort(dbSenderPort), dispatcher(Dispatcher::getInstance()), sessionManager(IOCPSessionManager::getInstance()),
		logs(Logs::getInstance()), input(InputManager::getInstance())
	{
		if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) return;
	}

	~ServerComponent()
	{
		WSACleanup();
		SAFE_FREE(iocp);
		SAFE_FREE(sendManager);
		SAFE_FREE(dbSender);
		SAFE_FREE(packetThreadPool);
		SAFE_FREE(packetProc);
	}

	bool initialize(bool useDBconnector = false);
	void Run();
};

#endif
