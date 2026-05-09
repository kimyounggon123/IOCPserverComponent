#include "Broadcaster.h"


bool Broadcaster::initialize()
{
	if (!ThreadPool::initialize()) return false;
	dispatcherHub.AddNewDispatcher(DispatcherID::Broadcast);
	roomManager.GetSessionsOwner().MakeSOCKETINFOforUDPbroadcast(120); // UDP 브로드캐스팅용 더미 SOCKETINFO 제작
	return true;
}

bool Broadcaster::SendAllRoomMember(const Task& info)
{
	IOCPSessionManager& sessionManager = roomManager.GetSessionsOwner();
	INT retval;
	DWORD sendbytes;
	char buffer[2048]; // 미리 버퍼에 serialize

	if (!info.packet->serialize(buffer)) return false; // 패킷이 망가져있을 경우 보내지 않기
	ULONG len = info.packet->getPacketSerializedLength();

	if (info.sessionType == SESSION_TYPE::TCP)
	{

		std::vector<SOCKETINFO*> members;
		info.target.room->CopySOCKETINFOPointers(members);

		for (SOCKETINFO* ptr : members)
		{
			try
			{
				if (ptr == nullptr) throw "nullptr";
				if (!ptr->acceptCompleted.load()) continue;
				if (info.packet->getClientID() == ptr->id) continue;

				ptr->response.reset_overlapped(ptr->response.IO_buffer, len, false); // wsabuf 초기화
				memcpy(ptr->response.IO_buffer, buffer, len); // 미리 serialize한 버퍼를 복사

				DWORD waitResult = ptr->waitSendEvent();
				if (waitResult != WAIT_OBJECT_0)
				{
					if (waitResult == WAIT_TIMEOUT)	throw "waitMutex() time up TCP";
					if (waitResult == WAIT_FAILED) throw "waitSendEvent() failed";
				}

				retval = WSASend(ptr->sock, &ptr->response.wsabuf, 1, &sendbytes,
					0, &ptr->response.overlapped, NULL);
				if (retval == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING)
					{
						throw "WSASend()";
					}
				}
				ptr->addResponseCount();
			}

			catch (const char* msg)
			{
				logs.log(msg, "SendAllRoomMember()");
			}
		}
		//if (info) dispatcherHub.PushTask(std::move(info), Route::Broadcast);
	}

	else if (info.sessionType == SESSION_TYPE::UDP)
	{
		SOCKETINFO* forUDPconnection = nullptr;
		std::vector<SOCKADDR_IN> udpmember;
		info.target.room->CopyMemberPointersUDP(udpmember);

		for (SOCKADDR_IN udp : udpmember)
		{
			try
			{
				forUDPconnection = nullptr; // 임시 socketinfo 빌려옴
				if (!sessionManager.GetSOCKETINFOforUDP(forUDPconnection)) throw "pop fail!";

		
				DWORD waitResult = forUDPconnection->waitSendEvent();
				if (waitResult != WAIT_OBJECT_0)
				{
					if (waitResult == WAIT_TIMEOUT)	throw "waitMutex() time up";
					if (waitResult == WAIT_FAILED) throw "waitSendEvent() failed";
				}

				forUDPconnection->response.reset_overlapped(forUDPconnection->response.IO_buffer, len, true); // wsabuf 초기화
				memcpy(forUDPconnection->response.IO_buffer, buffer, len); // 미리 serialize한 버퍼를 복사

				// Sending data
				retval = WSASendTo(sockUDP,
					&forUDPconnection->response.wsabuf,
					1,
					&sendbytes,
					0,
					(SOCKADDR*)&udp,
					sizeof(SOCKADDR_IN),
					&forUDPconnection->response.overlapped,
					NULL);
				if (retval == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING)
					{
						throw "WSASendTo()";
					}
				}
			}
			catch (const char* msg)
			{
				if (forUDPconnection) sessionManager.ReleaseSOCKETINFOforUDP(std::move(forUDPconnection));
				logs.log(msg, "SendAllRoomMember()");
			}
		}
		//if (info) dispatcherHub.PushTask(std::move(info), Route::Broadcast);
	}
	else
	{
		//if (info) dispatcherHub.PushTask(std::move(info), Route::Broadcast);
		return false;
	}
	return true;
}

unsigned int Broadcaster::workLoop()
{
	while (!exit_flag.load())
	{
		Task* output = nullptr;
		
		try
		{
			if (!dispatcherHub.DequeueTaskPTR(output, DispatcherID::Broadcast)) continue;
			if (output == nullptr) throw "output error";
			if (output->isInvalid()) throw "output field error";

			switch (output->target.type)
			{
			case TARGET_TYPE::Room:
				break;
			default:
				if (!SendAllRoomMember(*output)) throw "SendAllRoomMember()";
				break;
			}
		}
		catch (const char* msg)
		{
			logs.log_error(msg, "SendManager::work()");
		}

		if (output != nullptr)
		{
			dispatcherHub.ReturnTaskPTR(std::move(output), DispatcherID::Broadcast);
		}
	}
	return 0;
}