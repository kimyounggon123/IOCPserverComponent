#include "SOCKETINFO.h"


bool Room::input_socketinfo(SOCKETINFO * client_info)
{
	if (countClient.load() == maxClientsNum) return false;

	EnterCriticalSection(&map_cs);
	int id = next_id.fetch_add(1); // 안전하게 id 할당
	client_info->id = id;

	auto pair = client_map.emplace(id, client_info);
	if (!pair.second) {
		LeaveCriticalSection(&map_cs);
		return false;
	}

	countClient.fetch_add(1);
	LeaveCriticalSection(&map_cs);
	return true;
}

bool Room::find_socketinfo(int id, SOCKETINFO*& found)
{
	bool result = false;
	EnterCriticalSection(&map_cs);

	auto it = client_map.find(id);
	if (it != client_map.end())
	{
		found = it->second;
		result = true;
	}

	LeaveCriticalSection(&map_cs);
	return result;
}

bool Room::socketinfo_isin_here(int id)
{
	bool result = false;
	EnterCriticalSection(&map_cs);

	auto it = client_map.find(id);

	if (it != client_map.end())	result = true;

	LeaveCriticalSection(&map_cs);

	return result;
}

bool Room::delete_socketinfo(int id)
{
	EnterCriticalSection(&map_cs);
	EnterCriticalSection(&deleteCS);

	bool result = false;

	auto it = client_map.find(id);
	if (it != client_map.end())
	{
		countClient.fetch_sub(1);
		deletedClients.push_back(it->second);
		result = true;
	}

	LeaveCriticalSection(&deleteCS);
	LeaveCriticalSection(&map_cs);
	return result;
}

void Room::destroyInvalidSOCKETINFO()
{
	EnterCriticalSection(&map_cs);
	EnterCriticalSection(&deleteCS);

	for (auto it = deletedClients.begin(); it != deletedClients.end(); )
	{
		SOCKETINFO* info = *it;

		if (info->responseCount.load() == 0)
		{
			client_map.erase(info->id);
			info->cleanupSession();
			delete info;
			it = deletedClients.erase(it);
		}
		else
		{
			++it;
		}
	}
	LeaveCriticalSection(&deleteCS);
	LeaveCriticalSection(&map_cs);
}

void Room::delete_all()
{
	EnterCriticalSection(&map_cs);
	for (auto it = client_map.begin(); it != client_map.end(); )
	{
		delete it->second;
		it = client_map.erase(it);
	}
	LeaveCriticalSection(&map_cs);
}

void Room::CopyMemberPointers(std::vector<SOCKETINFO*>& out)
{
	EnterCriticalSection(&map_cs);
	out.reserve(client_map.size());
	for (auto& pair : client_map)
		out.push_back(pair.second);  // 포인터 얕은 복사
	LeaveCriticalSection(&map_cs);
}

IOCPSessionManager* IOCPSessionManager::instance = nullptr;