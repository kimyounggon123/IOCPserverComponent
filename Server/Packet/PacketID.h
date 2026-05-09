#ifndef _PACKETID_H
#define _PACKETID_H
#include <cstdint>


// 패킷 당 작업을 구별하는 flag
struct PacketType
{
	static constexpr int32_t Base = 0;
	static constexpr int32_t Default =			Base + 0;
	static constexpr int32_t ServerIsClosed =	Base + 1;
};


// 패킷 작업 성공 여부 및 브로드캐스팅/DB flag
struct PacketResult
{
	static constexpr int32_t Base = 0; 

	static constexpr int32_t Try =			Base + 0;
	static constexpr int32_t Success =		Base + 1;
	static constexpr int32_t Fail =			Base + 2;

	static constexpr int32_t Broadcast =	Base + 3;

	static constexpr int32_t WaitDatabase = Base + 4;
	static constexpr int32_t DatabaseSuccess = Base + 5;
	static constexpr int32_t DatabaseFail = Base + 6;

	static bool IsValidFlag(int32_t yourFlag)
	{	
		return yourFlag == Try || yourFlag == Success || yourFlag == Fail || yourFlag == Broadcast || yourFlag == WaitDatabase
			|| yourFlag ==  DatabaseSuccess || yourFlag == DatabaseFail;
	}
};


#endif