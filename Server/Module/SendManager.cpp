#include "SendManager.h"

//////////////////////// SendManager ///////////////////////////////
bool SendManager::initialize()
{
	if (!ThreadPool::initialize()) return false;
	if (!dispatcher.AddNewDispatcher(DispatcherID::ProcessToServer)) throw "Dispatcher"; // 사용할 디스패처 추가
	return true;
}

unsigned int SendManager::workLoop()
{
	INT retval;
	DWORD sendbytes;

	while (!exit_flag.load())
	{
		Task* output = nullptr;

		try
		{
			if (!dispatcher.DequeueTaskPTR(output, DispatcherID::ProcessToServer)) continue;
			if (output == nullptr) throw "output error";
			if (output->isInvalid()) throw "output field error";


			DWORD waitResult = output->sessionInfo->waitSendEvent();
			if (waitResult != WAIT_OBJECT_0)
			{
				if (waitResult == WAIT_TIMEOUT)	throw "waitMutex() time up";
				if (waitResult == WAIT_FAILED) throw "waitSendEvent() failed";
			}

			output->sessionInfo->addResponseCount(); // 전송 카운트 추가

			output->packet->setClientID(0); // 클라이언트로 전송 시 패킷에 저장된 client id를 초기화시킴



			if (output->sessionInfo->sessionType == SESSION_TYPE::TCP)
			{

				output->sessionInfo->response.reset_overlapped
				(output->sessionInfo->response.IO_buffer, output->packet->getPacketSerializedLength(), false);

				// Serialize
				ERROR_CODE err = output->packet->serialize(output->sessionInfo->response.IO_buffer);
				if (!err) throw "Serialize fail TCP";


				// Sending data
				retval = WSASend(output->sessionInfo->sock, &output->sessionInfo->response.wsabuf, 1, &sendbytes,
					0, &output->sessionInfo->response.overlapped, NULL);
				if (retval == SOCKET_ERROR) {
					if (WSAGetLastError() != WSA_IO_PENDING)
					{
						throw "WSASend()";
					}
				}

			}

			if (output->sessionInfo->sessionType == SESSION_TYPE::UDP)
			{

				output->sessionInfo->response.reset_overlapped
				(output->sessionInfo->response.IO_buffer, output->packet->getPacketSerializedLength(), true);

				// Serialize
				ERROR_CODE err = output->packet->serialize(output->sessionInfo->response.IO_buffer);
				if (!err) throw "Serialize fail UDP";

				// Sending data
				retval = WSASendTo(sockUDP,
					&output->sessionInfo->response.wsabuf,
					1,
					&sendbytes,
					0,
					(SOCKADDR*)&output->udpInfo,
					sizeof(SOCKADDR_IN),
					&output->sessionInfo->response.overlapped,
					NULL);
				if (retval == SOCKET_ERROR)
				{
					if (WSAGetLastError() != WSA_IO_PENDING)
					{
						throw "WSASend()";
					}
				}
			}
		}
		catch (const char* msg)
		{
			logs.log_error(msg, "SendManager::work()");
		}

		if (output != nullptr)
		{
			dispatcher.ReturnTaskPTR(std::move(output), DispatcherID::ProcessToServer);
		}
	}
	return 0;
}


