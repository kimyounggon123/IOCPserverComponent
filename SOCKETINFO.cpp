#include "SOCKETINFO.h"




bool SOCKETINFO::response_proc()
{
	Logs& logs = Logs::getInstance();
	bool result = true;

	// 전송은 한 번에 한 번 씩
	Packet* pk = nullptr;
	try
	{
		// Serialize
		int pk_len;
		ERROR_CODE err = pk->serialize(response.IO_buffer, pk_len);
		if (!err) throw "Serialize fail";
		response.reset_overlapped(response.IO_buffer, pk_len);

		// Sending data
		INT retval;
		DWORD sendbytes;
		retval = WSASend(sock, &response.wsabuf, 1, &sendbytes,
			0, &response.overlapped, NULL);
		if (retval == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				throw "WSASend()";
			}
		}
	}

	catch (const char* msg)
	{
		result = logs.log_error(msg, "response()");
	}
	SAFE_FREE(pk);
	return result;
}

template class ClientSessionManager<SOCKETINFO*>;
template class ClientSessionManager<UDPsession*>;

template <typename T>
ClientSessionManager<T>* ClientSessionManager<T>::instance = nullptr;
