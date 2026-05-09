#include "SOCKETINFO.h"


void Room::UpdateWithFrame(bool freeFlag)
{
	auto now = std::chrono::steady_clock::now(); // 현 시간 측정

	// 마지막 loop을 수행한 시간과 현 시간 차이 계산
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick);
	last_tick = now; // 다음 프레임을 위한 갱신

	delta += elapsed.count(); // delta 누적

	while (delta >= target_duration.count())
	{
		Update();
		destroyInvalidSOCKETINFO(freeFlag);
		// 기준 프레임 시간만큼 차감 (잔여 시간 보존)
		delta -= target_duration.count();
	}
}

bool Room::input_socketinfo(SOCKETINFO* client_info)
{
	if (countClient.load() == maxClientsNum) return false;

	EnterCriticalSection(&map_cs);
	int id = next_id.fetch_add(1); // 안전하게 id 할당
	client_info->id = id;

	auto pair = client_map.emplace(id, client_info);
	if (!pair.second)
	{
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

void Room::destroyInvalidSOCKETINFO(bool freeFlag)
{
	EnterCriticalSection(&map_cs);
	EnterCriticalSection(&deleteCS);

	for (auto it = deletedClients.begin(); it != deletedClients.end(); )
	{
		SOCKETINFO* info = *it;

		if (info->responseCount.load() == 0 || info->hearthBeats > MAX_HEARTHBEATS)
		{
			client_map.erase(info->id);
			deleteUDPsession(info->addr);
			info->cleanupSession();
			if (freeFlag) SAFE_FREE(info); 
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

	EnterCriticalSection(&udpCS);
	for (auto it = udpTargets.begin(); it != udpTargets.end();)
	{
		it = udpTargets.erase(it);
	}
	LeaveCriticalSection(&udpCS);
}

void Room::CopySOCKETINFOPointers(std::vector<SOCKETINFO*>& out)
{
	EnterCriticalSection(&map_cs);
	out.reserve(client_map.size());
	for (auto& pair : client_map)
		out.push_back(pair.second);  // 포인터 얕은 복사
	LeaveCriticalSection(&map_cs);
}

bool Room::inputUDPsession(const SOCKADDR_IN& addr)
{
	EnterCriticalSection(&udpCS);
	udpTargets.push_back(addr);
	LeaveCriticalSection(&udpCS);
	return true;
}

bool Room::SOCKADDRisinHere(const SOCKADDR_IN& addr)
{
	return std::any_of(
		udpTargets.begin(), udpTargets.end(),
		[&](const SOCKADDR_IN& a) {
			return a.sin_addr.s_addr == addr.sin_addr.s_addr &&
				a.sin_port == addr.sin_port &&
				a.sin_family == addr.sin_family;
		}
	);
}

bool Room::deleteUDPsession(const SOCKADDR_IN& addr)
{
	EnterCriticalSection(&udpCS);

	auto it = std::find_if(
		udpTargets.begin(), udpTargets.end(),
		[&](const SOCKADDR_IN& a) {
			return a.sin_addr.s_addr == addr.sin_addr.s_addr &&
				a.sin_port == addr.sin_port &&
				a.sin_family == addr.sin_family;
		}
	);

	if (it != udpTargets.end())
		udpTargets.erase(it);

	LeaveCriticalSection(&udpCS);
	return true;
}

void Room::CopyMemberPointersUDP(std::vector<SOCKADDR_IN>& out)
{
	EnterCriticalSection(&udpCS);
	out = udpTargets;  // 통째로 복사
	LeaveCriticalSection(&udpCS);
}


IOCPSessionManager* IOCPSessionManager::instance = nullptr;
void IOCPSessionManager::Update()
{
	EnterCriticalSection(&map_cs);
	for (auto& it : client_map)
	{
		it.second->ResetRequestCount();
	}
	LeaveCriticalSection(&map_cs);
}

bool IOCPSessionManager::MakeSOCKETINFOforUDPbroadcast(int count)
{
	for (int i = 0; i < count; i++)
	{
		std::unique_ptr<SOCKETINFO> dummy = std::make_unique<SOCKETINFO>(SESSION_TYPE::UDP, true);
		DummySOCKETINFOpool.AddElement(std::move(dummy));
	}
	/*
	if (!SOCKETINFOforUDPpool.empty()) return false;
	for (int i = 0; i < count; i++)
	{
		SOCKETINFO* forBroadcast = new SOCKETINFO(SESSION_TYPE::UDP, true);
		SOCKETINFOforUDPpool.push_back(forBroadcast);
	}
	*/
	return true;
}
// 삭제
void IOCPSessionManager::deleteUDPSOCKET()
{
	/*
	//std::lock_guard<std::mutex> lock(pool_mtx);
	EnterCriticalSection(&pool_cs);
	for (auto si : SOCKETINFOforUDPpool)
	{
		SAFE_FREE(si);
	}
	SOCKETINFOforUDPpool.clear();
	LeaveCriticalSection(&pool_cs);
	*/
}

// 현재 풀 크기
size_t IOCPSessionManager::getUDPSocketPoolSize()
{
	/*
	//std::lock_guard<std::mutex> lock(pool_mtx);
	EnterCriticalSection(&pool_cs);
	size_t result = SOCKETINFOforUDPpool.size();
	LeaveCriticalSection(&pool_cs);
	*/
	return  DummySOCKETINFOpool.GetPoolSize();
}

bool IOCPSessionManager::GetSOCKETINFOforUDP(SOCKETINFO*& output) // Udp 전송에 필요한 임시 SOCKETINFO 빌리기
{
	//std::lock_guard<std::mutex> lock(pool_mtx);
	bool result = false;
	result = DummySOCKETINFOpool.Pop(output);


	/*
	EnterCriticalSection(&pool_cs);
	for (auto si : SOCKETINFOforUDPpool)
	{
		if (!si->inUdpUse.load())
		{
			si->inUdpUse.store(true);
			output = si;  // pop 없이 참조만 전달
			result = true;
			break;
		}
	}
	LeaveCriticalSection(&pool_cs);
	*/

	return result; // 사용 가능한 객체 없음
}

void IOCPSessionManager::ReleaseSOCKETINFOforUDP(SOCKETINFO*&& input)
{
	if (!input) return;
	input->inUdpUse.store(false);
	DummySOCKETINFOpool.Push(std::move(input));
}

RoomManager* RoomManager::instance = nullptr;
bool RoomManager::Initialize()
{
	if (isInitialized) return isInitialized;

	isInitialized = true;
	return isInitialized;
}
Room* RoomManager::GetRoom(int ID)
{
	if (ID == 0) return &allClients;

	Room* found = nullptr;
	EnterCriticalSection(&map_cs);
	auto it = rooms.find(ID);
	if (it != rooms.end()) found = it->second;
	LeaveCriticalSection(&map_cs);
	return found;
}
Room* RoomManager::AddRoom(int max_client)
{
	Room* room = new Room(nextID, max_client);
	nextID.fetch_add(1);
	return room;
}
bool RoomManager::DeleteRoom(int ID)
{
	auto it = rooms.find(ID);
	if (it == rooms.end()) return false;
	delete it->second;
	it = rooms.erase(it);
	return true;
}
void RoomManager::DeleteAll()
{
	EnterCriticalSection(&map_cs);

	for (auto it = rooms.begin(); it != rooms.end();)
	{
		delete it->second;
		it = rooms.erase(it);
	}

	LeaveCriticalSection(&map_cs);

}

void RoomManager::Update()
{
	Room* room = nullptr;

	EnterCriticalSection(&map_cs);
	for (auto it = rooms.begin(); it != rooms.end(); it++)
	{
		room = it->second;
		room->UpdateWithFrame(false);
	}
	LeaveCriticalSection(&map_cs);


	allClients.UpdateWithFrame(true);
}


bool RoomManager::DeleteClientFromHere(const SOCKETINFO* info)
{
	for (auto it : info->roomIDlist)
	{
		auto room = rooms.find(it);
		if (!room->second) continue;
		room->second->delete_socketinfo(info->id);
	}
	allClients.delete_socketinfo(info->id);

	return true;
}
