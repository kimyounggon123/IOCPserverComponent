#ifndef _CLIENTROOM_H
#define _CLIENTROOM_H

#include "SOCKETINFO.h"
#include "DataToConnectWithClient.h"
class Client
{
	SOCKETINFO* sessionInfo;
	Vector3f pos;
	Vector3f axis;

public:
	Client(SOCKETINFO* sessionInfo) : sessionInfo(sessionInfo)
	{}
	~Client()
	{
		sessionInfo = nullptr; // session 정보는 여기서 삭제 금지
	}


	SOCKETINFO* getSessionInfo() noexcept { return sessionInfo; }

	Vector3f getPos() noexcept { return pos; }
	Vector3f getAsix() noexcept { return axis; }

	void setPos(const Vector3f& got) noexcept { pos = got; }
	void setAsix(const Vector3f& got) noexcept { axis = got; }
};

#define MAX_ROOMSIZE 8
class ClientRoom
{
	int id; // room id
	Client* clientList[MAX_ROOMSIZE];
	Client* host;
	CRITICAL_SECTION room_cs;

public:
	ClientRoom() : id(0), clientList{}, host(nullptr)
	{
		InitializeCriticalSection(&room_cs);
	}
	~ClientRoom()
	{
		DeleteCriticalSection(&room_cs);
	}

	int getID() noexcept { return id; }
	void setID(int id) noexcept { this->id = id; }

	bool inputClient(Client* client);
	bool quitClient(Client* client);
	Client* findClient(int clientID);

	bool setHost(Client* client);
	bool isEmptyRoom();

};

class RoomManager
{
	int next_id;
	int latest_deleted_id;

	std::unordered_map<int, ClientRoom*> client_map;
	CRITICAL_SECTION map_cs;

	static RoomManager* instance;
	RoomManager(): next_id(1), latest_deleted_id(0)
	{
		InitializeCriticalSection(&map_cs);
	}
public:
	static RoomManager& getInstance()
	{
		if (instance == nullptr) instance = new RoomManager;
		return *instance;
	}
	~RoomManager()
	{
		DeleteCriticalSection(&map_cs);
	}

	bool addNewRoom(ClientRoom* room)
	{
		EnterCriticalSection(&map_cs);

		room->setID(next_id);
		auto pair = client_map.emplace(room->getID(), room);
		if (!pair.second) // 삽입 실패 시
		{
			LeaveCriticalSection(&map_cs);
			return false;
		}
		next_id++; // increase next id

		LeaveCriticalSection(&map_cs);

		return true;
	}

	bool deleteRoom(int id)
	{
		EnterCriticalSection(&map_cs);

		auto it = client_map.find(id);
		if (it == client_map.end())
		{
			LeaveCriticalSection(&map_cs);
			return false;
		}

		// 방에 유저 없으면
		if (it->second->isEmptyRoom())
		{
			delete it->second;
			client_map.erase(it);
			latest_deleted_id = id;
		}

		LeaveCriticalSection(&map_cs);
		return true;
	}
};
#endif