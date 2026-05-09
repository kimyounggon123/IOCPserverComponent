#include "ServerFramework.h"

ServerFramework::ServerFramework(PacketProcess*&& packetProc, USHORT portTCP, USHORT portUDP, bool use_debug_mode, bool use_broadcast) :
	exit_flag(false), use_debug_mode(use_debug_mode),
	packetProc(std::move(packetProc)), packetThreadPool(nullptr),
	iocp(nullptr), portTCP(portTCP), portUDP(portUDP),
	sendManager(nullptr),
	use_broadcast(use_broadcast), broadcaster(nullptr),
	dispatcher(DispatcherHub::getInstance()), roomManager(RoomManager::getInstance()),
	logs(Logs::getInstance()), input(InputManager::getInstance())
{
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) return;
}

ServerFramework::~ServerFramework()
{
	WSACleanup();
	SAFE_FREE(iocp);
	SAFE_FREE(sendManager);
	SAFE_FREE(packetThreadPool);
	SAFE_FREE(packetProc);
	SAFE_FREE(broadcaster);

	for (auto* con : DBconnectors)
	{
		SAFE_FREE(con);
	}
	DBconnectors.clear();
}

bool ServerFramework::Initialize()
{
	try
	{
		SetConsoleOutputCP(CP_UTF8); // 콘솔 코드 페이지를 UTF-8로 변경

		if (packetProc == nullptr)
		{
			// process는 미리 등록
			// 미등록 시 디폴트 process 등록하기

			packetProc = new PacketProcess();
			if (packetProc == nullptr)	throw "PacketProcess is nullptr!"; // memory error
			packetProc->initialize();
		}
	
		packetThreadPool = new PacketProcessThreadPool(10, packetProc);

		iocp = new IOCPserver(portTCP, portUDP, packetThreadPool);
		if (iocp && !iocp->initialize()) throw "IOCP";

		sendManager = new SendManager(10, iocp->GetUDPSocket());
		if (sendManager && !sendManager->initialize())  throw "SendManager";

		if (packetThreadPool && !packetThreadPool->initialize()) throw "PacketThreadPool";

		if (use_broadcast)
		{
			broadcaster = new Broadcaster(10, iocp->GetUDPSocket());
			if (!broadcaster) throw "Broadcaster";
			broadcaster->initialize();
		}
	}
	catch (const char* msg)
	{
		return logs.log_error(msg, "ServerComponent::initialize()");
	}

	return true;
}

bool ServerFramework::InitializeDBconnector(int count, USHORT startPort)
{
	for (int i = 0; i < count; i++)
	{
		DBconnector* c = new DBconnector(startPort + i);
		if (c == nullptr) return false;
		c->initialize();
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
		if (broadcaster && !broadcaster->Start()) throw "Broadcaster";

		for (auto* c : DBconnectors)
		{
			if (c && !c->Start()) throw "DBconnector";
		}
	}
	catch (const char* msg)
	{
		logs.log_error(msg, "ServerComponent::Start()");
		return false;
	}
	return true;
}

void ServerFramework::Work()
{
	// 메인 스레드
	_tprintf(_T("KeyInput mode is working.\n"));
	_tprintf(_T("[Commands] You can use [ctrl + a] to write all commands on this window.\n"));
	while (!exit_flag)
	{
		roomManager.Update();

		if (use_debug_mode)
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

				if (input.isKeyDown('D')) printf("delete list Count %lld\n", roomManager.GetDeleteReservationCount());
			}
			Sleep(60);
		}

	
	}
}

void ServerFramework::Quit()
{
	if (iocp)
	{
		iocp->Quit();
		_tprintf(_T("Quit IOCP.\n"));
	}
	if (sendManager)
	{
		sendManager->Quit();
		_tprintf(_T("Quit Send manager.\n"));
	}
	if (packetThreadPool)
	{
		packetThreadPool->Quit();
		_tprintf(_T("Quit Packet Thread Pool.\n"));
	}

	if (broadcaster)
	{
		broadcaster->Quit();
		_tprintf(_T("Quit Broadcaster.\n"));
	}
	for (auto* con : DBconnectors)
	{
		con->Quit();
		//_tprintf(_T("Quit DBconnector.\n"));
	}


	iocp->WaitThreadClosing();
	sendManager->WaitThreadClosing();
	packetThreadPool->WaitThreadClosing();
	broadcaster->WaitThreadClosing();
	for (auto* con : DBconnectors)
	{
		con->WaitThreadClosing();
	}

	DispatcherHub::DeleteInstance();

	_tprintf(_T("Quit the server.\n"));
}

void ServerFramework::Run()
{

	try
	{
		if (!Start()) throw false;
		Work();
	}
	catch (...)
	{
		_tprintf(_T("Start Error.\n"));
	}
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