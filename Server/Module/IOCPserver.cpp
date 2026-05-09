#include "IOCPserver.h"


IOCPserver::IOCPserver(USHORT portTCP, USHORT portUDP, PacketProcessThreadPool* packetThreadPool) :
	serverPtr(0),
	IOCP(NULL), exit_flag(false), isGateClosed(false), countThreads(0), portTCP(portTCP), portUDP(portUDP),
	sockTCP(INVALID_SOCKET), sockUDP(INVALID_SOCKET), addrV4{}, logs(Logs::getInstance()),
	roomManager(RoomManager::getInstance()), dispatcher(DispatcherHub::getInstance())
{}

IOCPserver::~IOCPserver()
{
	closesocket(sockTCP);
	closesocket(sockUDP);
	CloseHandle(IOCP);

	for (HANDLE h : workerThreads)
		CloseHandle(h);
	workerThreads.clear();
}

bool IOCPserver::initialize()
{
	if (!dispatcher.AddNewDispatcher(DispatcherID::ServerToProcess)) throw "Dispatcher"; // 사용할 디스패처 추가
	INT retval;
	int optval = 1;
	serverPtr = (ULONG_PTR)this;
	try
	{
		if (portTCP != 0)
		{
			// 1. make a socket TCP
			sockTCP = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
			if (sockTCP == INVALID_SOCKET) throw "WSASocket()";

			retval = setsockopt(sockTCP, SOL_SOCKET, SO_REUSEADDR,
				(char*)&optval, sizeof(optval));
			if (retval == SOCKET_ERROR) throw "setsockopt()";

			// 2.  binding socket
			memset(&addrV4, 0, sizeof(SOCKADDR_IN));
			addrV4.sin_family = AF_INET;
			addrV4.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
			addrV4.sin_port = htons(portTCP);
			retval = bind(sockTCP, (SOCKADDR*)&addrV4, sizeof(SOCKADDR_IN));
			if (retval == SOCKET_ERROR) throw "bind()";

			// listen()
			retval = listen(sockTCP, SOMAXCONN);
			if (retval == SOCKET_ERROR) throw "listen()";

			// initialize IOCP
			IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, NULL);
			if (IOCP)  _tprintf(_T("IOCP Handle Address: %p\n"), IOCP);
			else throw "IOCP error!";

			HANDLE h = CreateIoCompletionPort((HANDLE)sockTCP, IOCP, serverPtr, 0);
			if (h != IOCP) throw "CreateIoCompletionPort() bind failed";
		}

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		if (portUDP != 0)
		{
			// 1. make a socket UDP
			sockUDP = WSASocket(AF_INET, SOCK_DGRAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
			if (sockUDP == INVALID_SOCKET) throw "WSASocket()";

			// 2.  binding socket
			memset(&addrV4, 0, sizeof(SOCKADDR_IN));
			addrV4.sin_family = AF_INET;
			addrV4.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
			addrV4.sin_port = htons(portUDP);
			retval = bind(sockUDP, (SOCKADDR*)&addrV4, sizeof(SOCKADDR_IN));
			if (retval == SOCKET_ERROR) throw "bind()";

			HANDLE hUdp = CreateIoCompletionPort((HANDLE)sockUDP, IOCP, serverPtr, 0);
			if (hUdp != IOCP) throw "CreateIoCompletionPort() bind failed (UDP)";
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

		// 3. Make IOCP thread pool
		SYSTEM_INFO si; // Get num of CPU
		GetSystemInfo(&si);

		countThreads = static_cast<size_t>(si.dwNumberOfProcessors * 2);
	}

	catch (const char* msg)
	{
		logs.log_error(msg, "IOCPserver::initialize()");
		return false;
	}

	return true;
}

bool IOCPserver::Start()
{
	HANDLE hThread; // make threads to put in IOCP
	try
	{
		for (size_t i = 0; i < countThreads; i++) {
			hThread = (HANDLE)_beginthreadex(
				NULL, 0,
				workerThread, this,
				0, NULL);
			if (hThread == NULL)
			{
				throw "_beginthreadex()";
			}
			workerThreads.push_back(hThread);
		}
		for (int i = 0; i < countThreads * 3; i++)
		{
			if (!makeClientSocket())
			{
				throw "TCP socket initialize fail";
			}
			if (!MakeSocketInfoToRecvFrom())
			{
				throw "UDP socket initialize fail";
			}
		}
	}
	catch (const char* msg)
	{
		logs.log_error(msg, "IOCPserver::Start()");
		return false;
	}

	return true;
}

void IOCPserver::openServerGate() 
{ 
	isGateClosed.store(false);
	logs.log("Open Server Gate!!", "IOCP");
}
void IOCPserver::closeServerGate()
{
	isGateClosed.store(true);
	
	logs.log("Close Server Gate!!", "IOCP");
}
void IOCPserver::Quit()
{
	exit_flag.store(true);
	closeServerGate();
	for (int i = 0; i < countThreads; i++)
		PostQueuedCompletionStatus(IOCP, 0, 0, nullptr); // 더미 호출 이용
}


void IOCPserver::WaitThreadClosing()
{
	DWORD result = WaitForMultipleObjects(
		static_cast<DWORD>(workerThreads.size()),
		workerThreads.data(),   // 핵심
		TRUE,
		INFINITE
	);
	if (result == WAIT_OBJECT_0)
	{
		logs.log("Threads are over.", "IOCP");
	}
}

unsigned int WINAPI IOCPserver::workerThread(LPVOID server_info)
{
	IOCPserver* This = reinterpret_cast<IOCPserver*>(server_info);
	Logs& logs = This->logs;
	RoomManager& roomManager = This->roomManager;
	HANDLE IOCP = This->IOCP;

	INT retval;
	DWORD cbTransferred;

	int addrlen = sizeof(SOCKADDR_IN);
	ULONG_PTR key;

	std::string ThreadIDstring = std::to_string(GetCurrentThreadId());
	std::string gotId = "GetQueuedCompletionStatus (" + ThreadIDstring + ")";


	while (!This->exit_flag.load())
	{
		LPOVERLAPPED overlapped;
		SOCKETINFO* socketinfo = nullptr;

		// <Get IO result>
		retval = GetQueuedCompletionStatus(IOCP, &cbTransferred, &key, &overlapped, INFINITE);

		if (This->exit_flag || key == 0 || overlapped == nullptr)  continue;

		IO_CONTEXT* io = reinterpret_cast<IO_CONTEXT*>(overlapped);
		socketinfo = io->owner; // IO_CONTEXT에서 역추적

		if (socketinfo == nullptr)	continue;
		if (socketinfo->isBlocked)
		{
			This->roomManager.DeleteClientFromHere(socketinfo);
			continue;
		}

		if (socketinfo->sessionType == SESSION_TYPE::TCP)
			This->TCPLogic(socketinfo, io, retval, cbTransferred);
		if (socketinfo->sessionType == SESSION_TYPE::UDP)
			This->UDPLogic(socketinfo, io, retval, cbTransferred);
	}

	return 0;
}

// TCP
bool IOCPserver::TCPLogic(SOCKETINFO* socketinfo, IO_CONTEXT* io, INT retval, DWORD cbTransferred)
{

	try 
	{
		if (socketinfo == nullptr || io == nullptr)	throw "nullptr";
		if (io->ioType == IO_TYPE::Response)
		{
			socketinfo->subResponseCount();
			socketinfo->setSendEvent();
		}

		// 서버/클라이언트 강제 종료 시
		if (retval == 0)
		{
			int err = WSAGetLastError();
			std::string errorMsg = "IO error, WSAError=" + std::to_string(err);
			logs.log(errorMsg.c_str());
			roomManager.DeleteClientFromHere(socketinfo);
			return true;
		}

		// 접속 성공 혹은 종료 시
		if (cbTransferred == 0)
		{
			// 기존 접속 종료
			if (socketinfo->acceptCompleted.load())
			{
				logs.log("Remote closed connection");
				roomManager.DeleteClientFromHere(socketinfo);
				return true;
			}

			// 신규 접속
			else
			{
				if (!welcomeClient(socketinfo))
				{
					roomManager.DeleteClientFromHere(socketinfo);
					throw "welcomeClient() failed";
				}
				socketinfo->acceptCompleted.store(true);
				// if (sessionManager.getClientCount() % 50 == 0)printf("count: %d\n", sessionManager.getClientCount());
			}
		}

		// cbTransferred > 0 일 경우
		if (io->ioType == IO_TYPE::Request)
		{
			makePacketFromIOresult(socketinfo, cbTransferred);
			if (!recvFromSOCKETINFO(socketinfo)) throw "request()";
			socketinfo->ResetHearthBeats();
		}

	}

	catch (const char* msg)
	{
		logs.log_error(msg, "IOCPserver::TCPLogic()");
		return false;
	}
	return true;
}

bool IOCPserver::LeaveServer(SOCKETINFO* ptr)
{
	return true;
}

bool IOCPserver::makeClientSocket()
{
	// Session도 풀 형태로 만들어서 처리를 할까?

	SOCKETINFO* ptr = nullptr;
	try {
		// if (!isGateOpen.load()) throw "Gate is closed.";
		ptr = new SOCKETINFO(SESSION_TYPE::TCP);
		if (!ptr) throw "memory limit";
		BOOL retval;

		// AcceptEx 함수 포인터 가져오기
		LPFN_ACCEPTEX lpfnAcceptEx;
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		DWORD bytes;
		retval = WSAIoctl(sockTCP, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidAcceptEx, sizeof(guidAcceptEx),
			&lpfnAcceptEx, sizeof(lpfnAcceptEx),
			&bytes, NULL, NULL);
		if (retval == SOCKET_ERROR) throw "WSAIoctl() failed";

		// 새 client socket 생성
		ptr->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (ptr->sock == INVALID_SOCKET)
		{
			throw "WSASocket() failed";
		}
		CreateIoCompletionPort((HANDLE)ptr->sock, IOCP, (ULONG_PTR)ptr, 0);
		ZeroMemory(&ptr->request.overlapped, sizeof(OVERLAPPED));

		DWORD bytesReceived = 0;
		retval = lpfnAcceptEx(
			sockTCP,                // listen 소켓
			ptr->sock,             // 새 client 소켓
			(PVOID)ptr->request.IO_buffer,     // 주소+데이터 버퍼
			0,                     // 첫 데이터 수신 버퍼 크기
			sizeof(SOCKADDR_STORAGE) + 16,
			sizeof(SOCKADDR_STORAGE) + 16,
			&bytesReceived,
			&ptr->request.overlapped
		); // 넘기는 KEY 값은 Server Port 사용할 때 넘기는 key = nullptr;

		if (!retval)
		{
			DWORD err = WSAGetLastError();
			if (err != ERROR_IO_PENDING)
			{
				throw "AcceptEx() failed";
			}
		}

		roomManager.GetSessionsOwner().input_socketinfo(ptr);
	}
	catch (const char* msg)
	{
		SAFE_FREE(ptr);
		logs.log_error(msg, "makeClientSocket()");
		return false;
	}
	return true;
}

bool IOCPserver::welcomeClient(SOCKETINFO* ptr)
{
	bool result = true;
	try
	{
		if (!ptr) throw "got nullptr";
		if (setsockopt(ptr->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&sockTCP, sizeof(sockTCP)))
			throw "setsockopt()";
		makeClientSocket(); // 다음 AcceptEx를 위해 새 소켓 준비
	}
	catch (const char* msg)
	{
		logs.log_error(msg, "welconeClient()");
		result =  false;
	}
	return result;
}
bool IOCPserver::recvFromSOCKETINFO(SOCKETINFO* ptr)
{
	if (ptr == nullptr) return false;

	// get data from clients
	ptr->request.reset_overlapped(ptr->request.IO_buffer);

	INT retval;
	DWORD recvbytes;
	DWORD flags = 0;

	retval = WSARecv(ptr->sock, &ptr->request.wsabuf, 1,
		&recvbytes, &flags, &ptr->request.overlapped, NULL);
	if (retval == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			return logs.log_error("WSARecv() error", "recvFromSOCKETINFO()");
		}
	}

	return true;
}
bool IOCPserver::makePacketFromIOresult(SOCKETINFO* ptr, DWORD cbTransferred)
{
	if (!ptr) return false;

	bool result = true;
	Task* input = nullptr;
	size_t offset = 0;

	const int MAX_RESYNC = 5;
	int resyncCount = 0;

	try 
	{
		while (cbTransferred > offset) // 패킷 무결성 검증
		{
			if (!ptr->CheckFullRequestProcessTCP()) throw "Request TCP deny";

			if (!dispatcher.BorrowTaskPTR(input, DispatcherID::ServerToProcess)) throw "memory limit";
			if (input == nullptr) throw "input is nullptr!";

			input->sessionInfo = ptr;
			input->sessionType = SESSION_TYPE::TCP;

			size_t localOffset = offset; 
			ERROR_CODE err = input->packet->deserialize(ptr->request.IO_buffer, cbTransferred - localOffset, localOffset);
			if (err == ERROR_CODE::NEED_EXTRA_DATA)
			{
				dispatcher.ReturnTaskPTR(std::move(input), DispatcherID::ServerToProcess);
				break;
			}
			else if (err != ERROR_CODE::SUCCESS)
			{
				offset += 1; // 한 바이트씩 버리면서 다음 패킷 탐색 -> 그냥 전부 날려버릴까?
				resyncCount++;
				dispatcher.ReturnTaskPTR(std::move(input), DispatcherID::ServerToProcess);
				if (resyncCount >= MAX_RESYNC)
				{
					// 너무 많이 재동기화 했으면 남은 데이터 모두 버림
					offset = cbTransferred;
					break;
				}
				continue;
			}
			if (isGateClosed.load()) input->packet->set_header_type(PacketType::ServerIsClosed);
			if (!dispatcher.EnqueueTaskPTR(std::move(input), DispatcherID::ServerToProcess)) throw "enqueue()";

			ptr->AddRequestCountTCP();
			offset = localOffset;
		}
	}

	catch (const char* msg)
	{
		result = logs.log_error(msg, "makePacketFromIOresult()");
		offset = 0;
		cbTransferred = 0;
		return result;
	}

	// 남은 데이터 이동
	memmove(ptr->request.IO_buffer, ptr->request.IO_buffer + offset, cbTransferred - offset);

	return result;
}


// UDP
bool IOCPserver::UDPLogic(SOCKETINFO* socketinfo, IO_CONTEXT* io, INT retval, DWORD cbTransferred)
{
	try
	{
		if (socketinfo == nullptr || io == nullptr)	throw "nullptr";

		//UDPsession* info = nullptr;
		//printf("id: %d\n", socketinfo->id);
		//if (sessionManager.findUDPsession(socketinfo->id, info)) throw "undefined!";

		if (io->ioType == IO_TYPE::Response)
		{
			socketinfo->setSendEvent();
			if (socketinfo->isDummy)
			{
				//printf("broadcast sub response count\n");
				roomManager.GetSessionsOwner().ReleaseSOCKETINFOforUDP(std::move(socketinfo));
			}
			//socketinfo->subResponseCount();
		}

		// 서버/클라이언트 강제 종료 시
		/*if (retval == 0 || cbTransferred == 0)
		{
			int err = WSAGetLastError();
			sessionManager.delete_socketinfo(socketinfo->id);
			logs.log("Quit server");
			return true;
		}*/

		// cbTransferred > 0 일 경우
		if (io->ioType == IO_TYPE::Request)
		{
			if (!roomManager.GetSessionsOwner().SOCKADDRisinHere(socketinfo->addr))
			{
				WelcomeToUDP(socketinfo);
			}
			/*if (!socketinfo->acceptCompleted.load())
			{
				if (!WelcomeToUDP(socketinfo, info))
				{
					sessionManager.delete_socketinfo(socketinfo->id);
					throw "welcomeClient() failed";
				}
			}*/

			if (cbTransferred != 0)	MakePacketUDP(socketinfo, cbTransferred);
			if (!RecvUDP(socketinfo)) throw "request()";
			socketinfo->ResetHearthBeats();
		}

	}
	catch (const char* msg)
	{
		logs.log_error(msg, "IOCPserver::UDPLogic()");
		return false;
	}
	return true;
}

bool IOCPserver::MakeSocketInfoToRecvFrom()
{
	// get data from clients
	SOCKETINFO* ptr = nullptr;
	INT retval;
	DWORD recvbytes;
	DWORD flags = 0;
	INT addrLen = sizeof(SOCKADDR_IN);
	try {
		// if (!isGateOpen.load()) throw "Gate is closed.";
		ptr = new SOCKETINFO(SESSION_TYPE::UDP);
		if (!ptr) throw "memory limit";

		ptr->request.reset_overlapped(ptr->request.IO_buffer, IO_BUFFER_LEN, true);

		retval = WSARecvFrom
		(
			sockUDP,                  // 서버 소켓
			&ptr->request.wsabuf,
			1,
			&recvbytes,
			&flags,
			(SOCKADDR*)&ptr->addr,   // 클라이언트 주소
			&addrLen,
			&ptr->request.overlapped,
			NULL
		);
		if (retval == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				return logs.log_error("WSARecv() error", "recvFromSOCKETINFO()");
			}
		}

		roomManager.GetSessionsOwner().input_socketinfo(ptr); // map에 저장
	}
	catch (const char* msg)
	{
		SAFE_FREE(ptr);
		logs.log_error(msg, "makeClientSocket()");
		return false;
	}

	return true;
}
bool IOCPserver::RecvUDP(SOCKETINFO* ptr)
{
	// get data from clients
	ptr->request.reset_overlapped(ptr->request.IO_buffer, IO_BUFFER_LEN,  true);

	INT retval;
	DWORD recvbytes;
	DWORD flags = 0;
	INT addrLen = sizeof(SOCKADDR_IN);

	retval = WSARecvFrom(
		sockUDP,                  // 서버 소켓
		&ptr->request.wsabuf,
		1,
		&recvbytes,
		&flags,
		(SOCKADDR*)&ptr->addr,   // 클라이언트 주소
		&addrLen,
		&ptr->request.overlapped,
		NULL
	);
	if (retval == SOCKET_ERROR) {
		if (WSAGetLastError() != WSA_IO_PENDING) {
			return logs.log_error("WSARecv() error", "recvFromSOCKETINFO()");
		}
	}

	return true;
}

bool IOCPserver::WelcomeToUDP(SOCKETINFO* ptr)
{
	if (!ptr) return false;
	roomManager.GetSessionsOwner().inputUDPsession(ptr->addr);
	printf("welcome!\n");
	return	true;
}

bool IOCPserver::MakePacketUDP(SOCKETINFO* ptr, DWORD cbTransferred)
{
	if (!ptr) return false;

	bool result = true;
	Task* input = nullptr;
	size_t offset = 0;
	/*
	size_t offset = 0;
	const int MAX_RESYNC = 5;
	int resyncCount = 0;
	
	try
	{
		while (cbTransferred > offset) // 패킷 무결성 검증
		{
			if (!dispatcher.pop(input)) throw "memory limit";
			if (input == nullptr) throw "input is nullptr!";
			input->InputInfo(ptr,  ptr->addr);
			
			ERROR_CODE err = input->packet->deserialize(ptr->request.IO_buffer, cbTransferred, offset);

			if (err != ERROR_CODE::SUCCESS)
			{
				dispatcher.push(input);
				return logs.log_error("deserialize failed", "MakePacketUDP()");
			}
			
			if (isGateClosed.load()) input->packet->set_header_type(PacketType::ServerIsClosed);
			if (!dispatcher.enqueue(input, QueueInformation::PacketProcess)) throw "enqueue()";
		}
	}

	catch (const char* msg)
	{
		if (input != nullptr)
		{
			dispatcher.push(input);
		}
		result = logs.log_error(msg, "makePacketFromIOresult()");
		offset = 0;
		cbTransferred = 0;
		return result;
	}
	*/

	try
	{
		if (!ptr->CheckFullRequestProcessUDP()) throw "Request UDP deny";

		if (!dispatcher.BorrowTaskPTR(input, DispatcherID::ServerToProcess)) throw "memory limit";
		if (!input) throw "input is nullptr!";

		input->sessionInfo = ptr;
		input->udpInfo = ptr->addr;
		input->sessionType = SESSION_TYPE::UDP;

		ERROR_CODE err = input->packet->deserialize(ptr->request.IO_buffer, cbTransferred, offset);
		if (err != ERROR_CODE::SUCCESS)
		{
			dispatcher.ReturnTaskPTR(std::move(input), DispatcherID::ServerToProcess); // 다시 풀에 반환
			//logs.log_error("deserialize failed", "MakePacketUDP()");
			return false;
		}

		if (isGateClosed.load()) input->packet->set_header_type(PacketType::ServerIsClosed);

		if (!dispatcher.EnqueueTaskPTR(std::move(input), DispatcherID::ServerToProcess)) throw "enqueue()";

		ptr->AddRequestCountUDP();
	}
	catch (const char* msg)
	{
		logs.log_error(msg, "MakePacketUDP()");
		result = false;
	}

	return result;
}
