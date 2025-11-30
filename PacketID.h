#ifndef _PACKETID_H
#define _PACKETID_H
#include <cstdint>


// Headers
struct PacketType
{
	static constexpr int32_t Base = 0;
	static int count;
	static int Next() { return count++; }

	static const int32_t Default;
	static const int32_t ServerIsClosed;
};

struct PacketResult
{
	static constexpr int32_t Base = 0;
	static int count;
	static int Next() { return count++; }

	static const int32_t Try;
	static const int32_t WaitDatabase;
	static const int32_t Success;
	static const int32_t Fail;
	static const int32_t BroadCast;
};


#endif