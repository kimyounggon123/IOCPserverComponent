#ifndef _COMPONENT_H
#define _COMPONENT_H

#include "Module/Dispatcher.h"
#include "Module/IOCPserver.h"
#include "Module/SendManager.h"
#include "Module/PacketProcessThreadPool.h"
#include "Module/PacketProcess.h"
#include "Module/DBconnector.h"
#include "Module/Broadcaster.h"
#include "UserControl/InputManager.h"

#include "Logs.h"

// Port list
#define CENTER_PORT 9000
#define SIGN_SERVERPORT 6000
#define BASIC_DB_PORT 7000

#define SIGN_SERVERPORT 6000
#define CHAT_SERVER_PORT 5000
#define GAME_PORT 4000



class ServerFramework
{
	bool exit_flag;
	bool use_debug_mode;
	WSADATA wsadata;
	
protected:

	PacketProcess* packetProc; // PacketProcess 부분만 상속 받아서 확장시키기
	PacketProcessThreadPool* packetThreadPool;


	IOCPserver* iocp; USHORT portTCP; USHORT portUDP; // port = 0 -> invalid socket
	SendManager* sendManager;

	bool use_broadcast;
	Broadcaster* broadcaster;

	std::vector<DBconnector*> DBconnectors;

	DispatcherHub& dispatcher;
	RoomManager& roomManager;

	Logs& logs;
	InputManager& input;

	void showCommands();
	
	virtual bool Start();
	void Work();
	virtual void Quit();

public:

	ServerFramework(PacketProcess*&& packetProc, USHORT portTCP = 0, USHORT portUDP = 0, bool use_debug_mode = true, bool use_broadcast = true);
	virtual ~ServerFramework();

	virtual bool Initialize();
	bool InitializeDBconnector(int count, USHORT startPort = 2000);
	void Run();
};

#endif
