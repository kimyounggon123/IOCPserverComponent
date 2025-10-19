#include "ClientRoom.h"
RoomManager* RoomManager::instance = nullptr;


bool ClientRoom::inputClient(Client* client)
{
	if (client == nullptr) return false;

	EnterCriticalSection(&room_cs);
	for (int i = 0; i < MAX_ROOMSIZE; i++)
	{
		if (clientList[i] == client) // �̹� ���� �� ��� �� ��
		{
			LeaveCriticalSection(&room_cs);
			return false;
		}
		if (clientList[i] == nullptr) // ���� ����� ������
		{
			clientList[i] = client; break;
		}
	}
	LeaveCriticalSection(&room_cs);
	return true;
}

bool ClientRoom::quitClient(Client* client)
{
	if (client == nullptr) return false;

	EnterCriticalSection(&room_cs);
	for (int i = 0; i < MAX_ROOMSIZE; i++)
	{
		if (clientList[i] == client) clientList[i] = nullptr;
	}
	LeaveCriticalSection(&room_cs);
	return true;
}

Client* ClientRoom::findClient(int clientID)
{
	Client* found = nullptr;

	EnterCriticalSection(&room_cs);
	for (int i = 0; i < MAX_ROOMSIZE; i++)
	{
		if (clientList[i]->getSessionInfo()->id == clientID)
		{
			found = clientList[i]; break;
		}
	}
	LeaveCriticalSection(&room_cs);

	return found;
}

bool ClientRoom::setHost(Client * client)
{
	if (client == nullptr || host == client) return false;
	EnterCriticalSection(&room_cs);
	host = client;
	LeaveCriticalSection(&room_cs);
	return true;
}

bool ClientRoom::isEmptyRoom()
{
	EnterCriticalSection(&room_cs);
	for (int i = 0; i < MAX_ROOMSIZE; i++)
	{
		if (clientList[i] != nullptr)
		{
			LeaveCriticalSection(&room_cs);
			return false;
		}
	}
	LeaveCriticalSection(&room_cs);
	return true;
}