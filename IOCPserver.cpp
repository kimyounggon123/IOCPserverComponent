#include "IOCPserver.h"


IOCPserver::IOCPserver(USHORT port_num, PacketProcessThreadPool* packetThreadPool) :
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
		// 1. make a socket
		sockV4 = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (sockV4 == INVALID_SOCKET) throw "WSASocket()";

		INT retval;
		BOOL optval = TRUE;
		retval = setsockopt(sockV4, SOL_SOCKET, SO_REUSEADDR,
			(char*)&optval, sizeof(BOOL));
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

		HANDLE h = CreateIoCompletionPort((HANDLE)sockV4, IOCP, (ULONG_PTR)this, 0);
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

		// AcceptEx ver. �̸� client socket ���� �� ���
		for (int i = 0; i < countThreads * 2; i++)
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
		PostQueuedCompletionStatus(IOCP, 0, 0, nullptr); // ���� ȣ�� �̿�
}

bool IOCPserver::makeClientSocket()
{
	// Session�� Ǯ ���·� ���� ó���� �ұ�?

	SOCKETINFO* ptr = nullptr;
	try {
		// if (!isGateOpen.load()) throw "Gate is closed.";
		ptr = new SOCKETINFO;
		if (!ptr) throw "memory limit";

		BOOL retval;

		// AcceptEx �Լ� ������ ��������
		LPFN_ACCEPTEX lpfnAcceptEx;
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		DWORD bytes;
		retval = WSAIoctl(sockV4, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidAcceptEx, sizeof(guidAcceptEx),
			&lpfnAcceptEx, sizeof(lpfnAcceptEx),
			&bytes, NULL, NULL);
		if (retval == SOCKET_ERROR) throw "WSAIoctl() failed";

		// �� client socket ����
		ptr->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (ptr->sock == INVALID_SOCKET)
		{
			throw "WSASocket() failed";
		}

		CreateIoCompletionPort((HANDLE)ptr->sock, IOCP, (ULONG_PTR)ptr, 0);
		ZeroMemory(&ptr->request.overlapped, sizeof(OVERLAPPED));

		DWORD bytesReceived = 0;
		retval = lpfnAcceptEx(
			sockV4,                // listen ����
			ptr->sock,             // �� client ����
			(PVOID)ptr->request.IO_buffer,     // �ּ�+������ ����
			0,                     // ù ������ ���� ���� ũ��
			sizeof(SOCKADDR_STORAGE) + 16,
			sizeof(SOCKADDR_STORAGE) + 16,
			&bytesReceived,
			&ptr->request.overlapped
		); // �ѱ�� KEY ���� Server Port ����� �� �ѱ�� key = nullptr;

		if (!retval)
		{
			DWORD err = WSAGetLastError();
			if (err != ERROR_IO_PENDING)
			{
				throw "AcceptEx() failed";
			}
		}

		sessionManager.input_socketinfo(ptr); // map�� ����
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
			//printf("key=%zu overlapped=%p cbTransferred=%d retval=%d\n", key, overlapped, cbTransferred, retval);

			if (This->exit_flag || key == 0 ||overlapped == nullptr)  continue;

			IO_CONTEXT* io = CONTAINING_RECORD(overlapped, IO_CONTEXT, overlapped); // �����ϰ� OVERLAPPED�� IO_CONTEXT�� ����
			socketinfo = io->owner; // IO_CONTEXT���� ������
			if (socketinfo == nullptr) continue;

			// ����/Ŭ���̾�Ʈ ���� ���� ��
			if (retval == 0)
			{
				int err = WSAGetLastError();
				std::string errorMsg = "IO error, WSAError=" + std::to_string(err);
				logs.log(errorMsg.c_str(), ThreadIDstring.c_str());
				sessionManager.delete_socketinfo(socketinfo->id);
				continue;
			}

			// ���� ���� Ȥ�� ���� ��
			if (cbTransferred == 0)
			{
				// ���� ���� ����
				if (socketinfo->acceptCompleted.load())
				{
					logs.log("Remote closed connection", ThreadIDstring.c_str());
					sessionManager.delete_socketinfo(socketinfo->id);
					continue;
				}

				// �ű� ����
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

			// cbTransferred > 0 �� ���
			if (io->ioType == IO_TYPE::Request)
			{
				This->makePacketFromIOresult(socketinfo, cbTransferred);
				if (!This->recvFromSOCKETINFO(socketinfo)) throw "request()";
			}

			if (io->ioType == IO_TYPE::Response)
			{
				if (socketinfo) socketinfo->setEvent();
			}

			// update last activity time
			socketinfo->updateActivity();

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
		if (setsockopt(ptr->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&sockV4, sizeof(sockV4))) throw "setsockopt()";
		makeClientSocket(); // ���� AcceptEx�� ���� �� ���� �غ�
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

	ptr->request.IO_buffer[cbTransferred] = '\0'; // close buffer

	bool result = true;
	TaskQueueInput* input = nullptr;
	size_t offset = 0;

	try 
	{
		while (cbTransferred - offset > 0) // ��Ŷ ���Ἲ ����
		{
			if (!dispatcher.pop(input)) throw "memory limit";
			if (input == nullptr) throw "input is nullptr!";
			input->sessionInfo = ptr;
			ERROR_CODE err = input->packet->deserialize(ptr->request.IO_buffer, cbTransferred, offset);
			if (!err)
			{
				if (err == ERROR_CODE::NEED_EXTRA_DATA) // �����Ͱ� �� �Դٸ�
				{
					dispatcher.push(input);
					break;
				}
				else
				{
					throw "deserialize()"; // ��Ÿ ���� �߻� ��
				}
			}

			if (isGateClosed.load()) input->packet->set_header_type(PacketType::ServerIsClosed);
			if (!dispatcher.enqueue(input, QueueInformation::PacketProcess)) throw "enqueue()";
		}
	}

	catch (const char* msg)
	{
		if (input != nullptr) dispatcher.push(input);
		result = logs.log_error(msg, "makePacketFromIOresult()");
		return result;
	}

	// ���� ������ �̵�
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
		TaskQueueInput* output = nullptr;
		try
		{
			if (!dispatcher.dequeue(output, QueueInformation::Send)) continue;
			if (output == nullptr) throw "output error";
			if (output->isInvalid()) throw "output field error";
			output->packet->setClientID(0); // Ŭ���̾�Ʈ�� ���� �� ��Ŷ�� ����� client id�� �ʱ�ȭ��Ŵ

			// Serialize
			int pk_len;
			ERROR_CODE err = output->packet->serialize(output->sessionInfo->response.IO_buffer, pk_len);
			if (!err) throw "Serialize fail";
			output->sessionInfo->response.reset_overlapped(output->sessionInfo->response.IO_buffer, pk_len);


			if (output->sessionInfo->waitEvent() == WAIT_TIMEOUT) throw "waitMutex() time up";
			
			// Sending data
			INT retval;
			DWORD sendbytes;
			retval = WSASend(output->sessionInfo->sock, &output->sessionInfo->response.wsabuf, 1, &sendbytes,
				0, &output->sessionInfo->response.overlapped, NULL);
			if (retval == SOCKET_ERROR) {
				if (WSAGetLastError() != WSA_IO_PENDING) {
					throw "WSASend()";
				}
			}
		}
		catch (const char* msg)
		{
			logs.log_error(msg, "SendManager::work()");
		}

		if (output != nullptr)	dispatcher.push(output);
	}
	return 0;
}

//////////////////////// DBconnector /////////////////////////////// 
DBconnector::DBconnector(USHORT DBserverPort, PacketProcessThreadPool* packetThreadPool) :
	exit_flag(false), sock(INVALID_SOCKET), addr{}, logs(Logs::getInstance()), serverPort(DBserverPort), 
	sessionManager(IOCPSessionManager::getInstance()), dispatcher(Dispatcher::getInstance())
{
	hEvent = WSACreateEvent();
	hThread = NULL;
	dwThreadID = 0;
	
	memset(recv_buf, 0, BUFFERSIZE + 1);
	memset(send_buf, 0, BUFFERSIZE + 1);
}
DBconnector::~DBconnector()
{
	closesocket(sock);
	CloseHandle(hThread);
	CloseHandle(hEvent);
}

bool DBconnector::initialize() {

	try {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == INVALID_SOCKET) throw "[DBconnector] socket()";

		u_long mode = 1;
		ioctlsocket(sock, FIONBIO, &mode); // ����ŷ ��� Ȱ��ȭ
		WSAEventSelect(sock, hEvent, FD_READ | FD_WRITE | FD_CLOSE); // �б�, ����, ���� �̺�Ʈ

		// connect and bind socket
		// ZeroMemory�� ����ü �ʱ�ȭ
		ZeroMemory(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(serverPort); // DB ���� ��Ʈ ��ȣ�� ����

		// IP �ּҸ� sin_addr�� ��ȯ �� ����
		if (inet_pton(AF_INET, DB_SERVER_IP, &(addr.sin_addr)) != 1) throw "[DBconnector] inet_pton() failed";

		INT retval = connect(sock, (SOCKADDR*)&addr, sizeof(addr));
		if (retval == SOCKET_ERROR) throw "connect()";
	}

	catch (const char* msg)
	{
		logs.log_error(msg, "DBconnector");
		logs.err_exit(_T("connect DB server"));
	}
	return true;
}

bool DBconnector::Start()
{
	try
	{
		// Make threads
		hThread = (HANDLE)_beginthreadex(
			NULL, 0,
			workerThread, this,
			0, (unsigned*)&dwThreadID
		);
		if (hThread == NULL) throw "[DBconnector] _beginthreadex()";
	}
	catch (const char* msg)
	{
		logs.log_error(msg, "DBconnector");
		return false;
	}

	return true;
}

void DBconnector::Quit()
{
	exit_flag.store(false);
}

/// Thread process
INT DBconnector::send_to_DB() {
	INT retval = 0;

	TaskQueueInput* output = nullptr;
	int serialize_size = 0;
	try {
		if (!dispatcher.dequeue(output, QueueInformation::Database)) return 0;
		if (output == nullptr) throw "output error";
		if (output->isInvalid()) throw "output field error";

		// serialize
		output->packet->setClientID(output->sessionInfo->id); // �Ŀ� Ŭ���̾�Ʈ ��ȸ�� ���� ��Ŷ�� Ŭ���̾�Ʈ id input
		ERROR_CODE code = output->packet->serialize(send_buf, serialize_size);
		if (code != ERROR_CODE::SUCCESS) throw "serialize";

		// send packet 
		retval = send(sock, send_buf, serialize_size, 0);
		if (retval == SOCKET_ERROR) throw "send()";
	}
	catch (const char* msg) {
		logs.log_error(msg, "DBconnector");
	}

	if (output != nullptr)	dispatcher.push(output);

	return retval;
}

bool DBconnector::make_pk_and_push(char* recv_buf, int recv_len, size_t& offset) {

	TaskQueueInput* input = nullptr;
	bool result = true;
	try {
		while (recv_len - offset > 0) {
			if (!dispatcher.pop(input)) throw "Memory limit";
			if (input == nullptr) throw "input is nullptr!";

			ERROR_CODE code = input->packet->deserialize(recv_buf, recv_len, offset);
			if (code == ERROR_CODE::NEED_EXTRA_DATA) {
				dispatcher.push(input);
				break;
			}
			if (code != ERROR_CODE::SUCCESS) throw "deserialize()";

			// find who send this
			SOCKETINFO* whoSendPacket = nullptr;
			if (!sessionManager.find_socketinfo(input->packet->getClientID(), whoSendPacket)) throw "non-exist client";
			
			input->sessionInfo = whoSendPacket;
			if (!dispatcher.enqueue(input, QueueInformation::PacketProcess)) throw "enqueue()";
		}
	}

	catch (const char* msg) {
		if (input != nullptr) dispatcher.push(input);
		logs.log_error(msg, "DBconnector");
		result = false;
	}

	return result;
}

INT DBconnector::recv_from_DB() {
	INT retval = 0;

	int recv_len = 0; // ������� ���ŵ� ������
	size_t offset = 0; // �д� ������ ��ġ

	try {
		retval = recv(sock, recv_buf + recv_len, BUFFERSIZE - recv_len, 0);
		if (retval == SOCKET_ERROR) 
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK) throw "recv() ����";
			return 0; // ������ ���� �� ��� ��ȯ
		}

		if (retval == 0) throw (_T("������ ������ �����߽��ϴ�."));

		recv_len += retval;  // ���ŵ� ������ ���� ����

		bool result = make_pk_and_push(recv_buf, recv_len, offset);
		if (!result) throw "make_pk_and_push()";

		memmove(recv_buf, recv_buf + offset, recv_len - offset); // ���� ������ �̵�
		recv_len -= static_cast<int>(offset);
		offset = 0;
	}

	catch (const char* msg) {
		logs.log_error(msg, "DBconnector");
	}

	return retval;
}

unsigned int WINAPI DBconnector::workerThread(LPVOID lpParam) {
	DBconnector* This = static_cast<DBconnector*>(lpParam);
	Logs& logs = This->logs;

	INT sent; INT recvd;
	DWORD waitResult;
	WSANETWORKEVENTS events;
	while (!This->exit_flag.load()) {

		waitResult = WaitForSingleObject(This->hEvent, 100); // �ִ� 100ms ���
		if (waitResult == WAIT_TIMEOUT || This->exit_flag.load())  continue; // send/recv ������ ���� �ٽ�

		else if (waitResult == WAIT_OBJECT_0)
		{
			WSAEnumNetworkEvents(This->sock, This->hEvent, &events);

			if (events.lNetworkEvents & FD_READ) 
			{
				recvd = This->recv_from_DB();  // ������ ����
				if (recvd == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) logs.log_error("recv() fail", "DBconnector");
			}

			if (events.lNetworkEvents & FD_WRITE) {
				sent = This->send_to_DB();  // �۽� ���ۿ� ������ �������� �ٽ� �õ�
				if (sent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
					logs.log_error("send() fail", "DBconnector");
			}

			if (events.lNetworkEvents & FD_CLOSE)
			{
				This->Quit();
			}
		}
	}
	return 0;
}

