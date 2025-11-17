#include "IOCPserver.h"


IOCPserver::IOCPserver(USHORT port_num, PacketProcessThreadPool* packetThreadPool) : serverPtr(0),
	IOCP(NULL), exit_flag(false), isGateClosed(false), countThreads(0), port(port_num),
	sockV4(INVALID_SOCKET), addrV4{}, logs(Logs::getInstance()), 
	sessionManager(IOCPSessionManager::getInstance()), dispatcher(Dispatcher::getInstance())
{}

IOCPserver::~IOCPserver()
{
	closesocket(sockV4);
	CloseHandle(IOCP);

	for (HANDLE h : workerThreads)
		CloseHandle(h);
	workerThreads.clear();
}

bool IOCPserver::initialize()
{
	try
	{
		serverPtr = (ULONG_PTR)this;

		// 1. make a socket
		sockV4 = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (sockV4 == INVALID_SOCKET) throw "WSASocket()";

		INT retval;
		int optval = 1;
		retval = setsockopt(sockV4, SOL_SOCKET, SO_REUSEADDR,
			(char*)&optval, sizeof(optval));
		if (retval == SOCKET_ERROR) throw "setsockopt()";

		// 2.  binding socket
		memset(&addrV4, 0, sizeof(SOCKADDR_IN));
		addrV4.sin_family = AF_INET;
		addrV4.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		addrV4.sin_port = htons(port);
		retval = bind(sockV4, (SOCKADDR*)&addrV4, sizeof(SOCKADDR_IN));
		if (retval == SOCKET_ERROR) throw "bind()";

		// listen()
		retval = listen(sockV4, SOMAXCONN);
		if (retval == SOCKET_ERROR) throw "listen()";


		// initialize IOCP
		IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, NULL);
		if (IOCP)  _tprintf(_T("IOCP Handle Address: %p\n"), IOCP);
		else throw "IOCP error!";

		HANDLE h = CreateIoCompletionPort((HANDLE)sockV4, IOCP, serverPtr, 0);
		if (h != IOCP) throw "CreateIoCompletionPort() bind failed";

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
	try
	{

		HANDLE hThread; // make threads to put in IOCP
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

		// AcceptEx ver. 미리 client socket 제작 후 등록
		for (int i = 0; i < countThreads * 3; i++)
		{
			if (!makeClientSocket())
			{
				throw "makeClientSocket()";
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
	exit_flag.store(false);
	closeServerGate();
	for (int i = 0; i < countThreads; i++)
		PostQueuedCompletionStatus(IOCP, 0, 0, nullptr); // 더미 호출 이용
}

bool IOCPserver::makeClientSocket()
{
	// Session도 풀 형태로 만들어서 처리를 할까?

	SOCKETINFO* ptr = nullptr;
	try {
		// if (!isGateOpen.load()) throw "Gate is closed.";
		ptr = new SOCKETINFO;
		if (!ptr) throw "memory limit";
		BOOL retval;

		// AcceptEx 함수 포인터 가져오기
		LPFN_ACCEPTEX lpfnAcceptEx;
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		DWORD bytes;
		retval = WSAIoctl(sockV4, SIO_GET_EXTENSION_FUNCTION_POINTER,
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
			sockV4,                // listen 소켓
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

		sessionManager.input_socketinfo(ptr); // map에 저장
	}
	catch (const char* msg)
	{
		SAFE_FREE(ptr);
		logs.log_error(msg, "makeClientSocket()");
		return false;
	}
	return true;
}

unsigned int WINAPI IOCPserver::workerThread(LPVOID server_info)
{
	IOCPserver* This = reinterpret_cast<IOCPserver*>(server_info);
	Logs& logs = This->logs;
	IOCPSessionManager& sessionManager = This->sessionManager;
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

		try {
			// <Get IO result>
			retval = GetQueuedCompletionStatus(IOCP, &cbTransferred, &key, &overlapped, INFINITE);

			if (This->exit_flag || key == 0 || overlapped == nullptr)  continue;

			IO_CONTEXT* io = reinterpret_cast<IO_CONTEXT*>(overlapped);
			socketinfo = io->owner; // IO_CONTEXT에서 역추적
			
			if (socketinfo == nullptr)
			{
				continue;
			}

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
				logs.log(errorMsg.c_str(), ThreadIDstring.c_str());
				sessionManager.delete_socketinfo(socketinfo->id);
				continue;
			}

			// 접속 성공 혹은 종료 시
			if (cbTransferred == 0)
			{
				// 기존 접속 종료
				if (socketinfo->acceptCompleted.load())
				{
					logs.log("Remote closed connection", ThreadIDstring.c_str());
					sessionManager.delete_socketinfo(socketinfo->id);
					continue;
				}

				// 신규 접속
				else
				{
					if (!This->welcomeClient(socketinfo))
					{
						sessionManager.delete_socketinfo(socketinfo->id);
						throw "welcomeClient() failed";
					}
					socketinfo->acceptCompleted.store(true);
					if (sessionManager.getClientCount() % 50 == 0)printf("count: %d\n", sessionManager.getClientCount());
				}
			}

			// cbTransferred > 0 일 경우
			if (io->ioType == IO_TYPE::Request)
			{
				This->makePacketFromIOresult(socketinfo, cbTransferred);
				if (!This->recvFromSOCKETINFO(socketinfo)) throw "request()";
				socketinfo->updateActivity();
			}

		}

		catch (const char* msg)
		{
			logs.log_error(msg, "IOCPserver::workerThread()");
		}
	}


	return 0;
}

bool IOCPserver::welcomeClient(SOCKETINFO* ptr)
{
	bool result = true;
	try
	{
		if (!ptr) throw "got nullptr";
		if (setsockopt(ptr->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&sockV4, sizeof(sockV4))) 
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
	TaskQueueInput* input = nullptr;
	size_t offset = 0;

	const int MAX_RESYNC = 5;
	int resyncCount = 0;

	try 
	{
		while (cbTransferred > offset) // 패킷 무결성 검증
		{
			if (!dispatcher.pop(input)) throw "memory limit";
			if (input == nullptr) throw "input is nullptr!";
			input->sessionInfo = ptr;

			size_t localOffset = offset; 
			//ERROR_CODE err = input->packet->deserialize(ptr->request.IO_buffer, cbTransferred - offset, offset);
			ERROR_CODE err = input->packet->deserialize(ptr->request.IO_buffer, cbTransferred - localOffset, localOffset);
			if (err == ERROR_CODE::NEED_EXTRA_DATA)
			{
				dispatcher.push(input);
				break;
			}
			else if (err != ERROR_CODE::SUCCESS)
			{
				offset += 1; // 한 바이트씩 버리면서 다음 패킷 탐색 -> 그냥 전부 날려버릴까?
				resyncCount++;
				dispatcher.push(input);
				if (resyncCount >= MAX_RESYNC)
				{
					// 너무 많이 재동기화 했으면 남은 데이터 모두 버림
					offset = cbTransferred;
					break;
				}
				continue;
			}
			if (isGateClosed.load()) input->packet->set_header_type(PacketType::ServerIsClosed);
			if (!dispatcher.enqueue(input, QueueInformation::PacketProcess)) throw "enqueue()";

			offset = localOffset;
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

	// 남은 데이터 이동
	memmove(ptr->request.IO_buffer, ptr->request.IO_buffer + offset, cbTransferred - offset);

	return result;
}

//////////////////////// SendManager ///////////////////////////////
bool SendManager::initialize()
{
	if (!ThreadPool::initialize()) return false;
	return true;
}

unsigned int SendManager::workLoop()
{
	while (!exit_flag.load())
	{
		SOCKETINFO* ptr = nullptr;
		TaskQueueInput* output = nullptr;

		try
		{
			if (!dispatcher.dequeue(output, QueueInformation::Send)) continue;
			if (output == nullptr) throw "output error";
			if (output->isInvalid()) throw "output field error";

			DWORD waitResult = output->sessionInfo->waitSendEvent();
			if (waitResult != WAIT_OBJECT_0)
			{
				if (waitResult == WAIT_TIMEOUT)	throw "waitMutex() time up";
				if (waitResult == WAIT_FAILED) throw "waitSendEvent() failed";
			}

			output->sessionInfo->addResponseCount(); // 전송 카운트 추가

			// output->packet->setClientID(0); // 클라이언트로 전송 시 패킷에 저장된 client id를 초기화시킴

			// Serialize
			ERROR_CODE err = output->packet->serialize(output->sessionInfo->response.IO_buffer);
			if (!err) throw "Serialize fail";

			output->sessionInfo->response.reset_overlapped(output->sessionInfo->response.IO_buffer, output->packet->getPacketSerializedLength());
			
			// Sending data
			INT retval;
			DWORD sendbytes;
			retval = WSASend(output->sessionInfo->sock, &output->sessionInfo->response.wsabuf, 1, &sendbytes,
				0, &output->sessionInfo->response.overlapped, NULL);
			if (retval == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING) 
				{
					throw "WSASend()";
				}
			}
		}
		catch (const char* msg)
		{
			logs.log_error(msg, "SendManager::work()");
		}

		if (output != nullptr)
		{
			dispatcher.push(output);
		}
	}
	return 0;
}


