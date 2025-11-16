#include "ServerFramework.h"

bool ServerFramework::initialize()
{
	try
	{
		SetConsoleOutputCP(CP_UTF8); // 콘솔 코드 페이지를 UTF-8로 변경

		if (packetProc == nullptr) throw "PacketProcess is nullptr!"; // process는 미리 받아오기

		packetThreadPool = new PacketProcessThreadPool(10, packetProc);
		iocp = new IOCPserver(serverPort, packetThreadPool);
		sendManager = new SendManager(10);

		if (iocp && !iocp->initialize()) throw "IOCP";
		if (sendManager && !sendManager->initialize())  throw "SendManager";
		if (packetThreadPool && !packetThreadPool->initialize()) throw "PacketThreadPool";
		if (!dispatcher.initialize()) throw "Dispatcher";
	}
	catch (const char* msg)
	{
		return logs.log_error(msg, "ServerComponent::initialize()");
	}

	return true;
}

// Make threads
bool ServerFramework::Start()
{
	try
	{
		if (iocp && !iocp->Start()) throw "IOCP";
		if (sendManager && !sendManager->Start())  throw "SendManager";
		if (packetThreadPool && !packetThreadPool->Start()) throw "PacketThreadPool";
	}
	catch (const char* msg)
	{
		logs.log_error(msg, "ServerComponent::Start()");
		return false;
	}
	return true;
}

void ServerFramework::WorkDebugger()
{
	_tprintf(_T("KeyInput mode is working.\n"));
	_tprintf(_T("[Commands] You can use [ctrl + a] to write all commands on this window.\n"));
	while (!exit_flag)
	{
		input.readEveryFrame();

		bool ctrlPressed = input.isKeyPressed(VK_CONTROL);

		if (ctrlPressed)
		{
			if (input.isKeyDown('A')) showCommands();

			if (input.isKeyDown('Q')) exit_flag = true;

			if (input.isKeyDown('O')) 
			{ 
				if (iocp) iocp->openServerGate();
			}
			if (input.isKeyDown('P')) 
			{
				if (iocp) iocp->closeServerGate();
			}

			if (input.isKeyDown('H')) logs.showHeapWalk();

			if (input.isKeyDown('M')) logs.showMemoryUsage("Server");

			if (input.isKeyDown('D')) printf("delete list Count %lld\n", sessionManager.getDeleteNum());
		}

		sessionManager.destroyInvalidSOCKETINFO();
		Sleep(50);
	}

	_tprintf(_T("Leave the server.\n"));
}

void ServerFramework::Quit()
{
	if (iocp) iocp->Quit();
	if (sendManager) sendManager->Quit();
	if (packetThreadPool) packetThreadPool->Quit();
	dispatcher.undoAllQueue();
}

void ServerFramework::Run()
{
	Start();
	WorkDebugger();
	Quit();
}


void ServerFramework::showCommands()
{
	_tprintf(_T("\n-----------------------------<Commands list>---------------------------------\n"));
	_tprintf(_T("[ctrl + a]: Show all commands.\n"));
	_tprintf(_T("[ctrl + q]: Quit server.\n"));
	_tprintf(_T("[ctrl + o]: Open server.\n"));
	_tprintf(_T("[ctrl + p]: Close server.\n"));
	_tprintf(_T("[ctrl + m]: Show memory states.\n"));
	_tprintf(_T("[ctrl + h]: Show heap states.\n"));
	_tprintf(_T("-------------------------------------------------------------------------------\n"));
}