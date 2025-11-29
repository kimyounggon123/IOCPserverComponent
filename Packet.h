#ifndef _PACKET_H
#define _PACKET_H

#include <Windows.h>
#include "stdafx.h"
#include <cstdint>
#include <cstring>
#include "PacketID.h"

#pragma pack(push, 1)   // 1바이트 단위로 정렬 시작
struct PacketHeader
{
	// Header informations
	int32_t clientID;
	int32_t type;
	int32_t result;
	int32_t length;

	PacketHeader() : clientID(0), type(PacketType::Default), result(PacketResult::Try), length(0)
	{}
	PacketHeader(int32_t clientID, int32_t type, int32_t result) : clientID(clientID), type(type), result(result), length(0)
	{}

	PacketHeader(const PacketHeader& other) :
		clientID(other.clientID), type(other.type), result(other.result), length(other.length)
	{}

	PacketHeader& operator=(const PacketHeader& other)
	{
		if (this != &other) {
			clientID = other.clientID;
			type = other.type;
			result = other.result;
			length = other.length;
		}
		return *this;
	}
};

class Packet
{
	// Header informations
	PacketHeader header;

	// memory data part
	char data[BUFFERSIZE + 1]; // 해당 containor에 들어가는 integer 등의 value는 직접 endian 처리하도록 할까?
	
	bool is_ascii(const std::string& str);
	bool is_ascii(const char* str);


	void inputHeader(char* buffer, size_t& offset);
	void copyHeader(const char* buffer, size_t& offset);
public:
	static const unsigned int end_mark;

	Packet(): header{}, data{}
	{}
	Packet(int clientID, int32_t type, int32_t result): header(clientID, type, result), data{}
	{}

	Packet(const Packet& other) :
		header(other.header)
	{
		memcpy(data, other.data, BUFFERSIZE + 1);
	}
	~Packet()
	{
		memset(data, 0, BUFFERSIZE + 1);
	}

	Packet& operator=(const Packet& other)
	{
		if (this != &other) {
			header = other.header;
			memcpy(data, other.data, BUFFERSIZE + 1);
		}
		return *this;
	}

	/// <control methods>
	// These methods'll be used when you use or input data in the packet
	void CLEAR_PACKET(bool delete_pk = false); // clear packet data. you can change header

	ERROR_CODE inputData(const void* src, const size_t& data_size, size_t& offset); // 미리 데이터 endian 처리해야 함.

	ERROR_CODE inputDataInt(int32_t src, size_t& offset);
	ERROR_CODE inputDataFloat(float src, size_t& offset);
	ERROR_CODE inputString(const char* data, size_t& offset);
	ERROR_CODE inputString(const std::string& data, size_t& offset);

	ERROR_CODE readData(void* dest, const size_t& data_size, size_t& offset);

	ERROR_CODE readDataInt(int32_t* dest, size_t& offset);
	ERROR_CODE readDataFloat(float* dest, size_t& offset);
	ERROR_CODE readString(char* dest, size_t& offset);
	ERROR_CODE readString(std::string& dest, size_t& offset);

	/// <Serialize methods>
	ERROR_CODE serialize(char* buffer);
	ERROR_CODE deserialize(const char* buffer, int recvLength, size_t& offset);

	/// <getter / setter>
	void set_header_type(const int32_t& change) noexcept { header.type = change; }
	const int32_t& get_type() noexcept { return header.type; }
	bool is_header(const int32_t& compare) noexcept { return header.type == compare; }

	void set_process_result(const int32_t& change) noexcept { header.result = change; }
	const int32_t& get_process_result() noexcept { return header.result; }

	void setClientID(int id) noexcept { header.clientID = id; }
	int getClientID() noexcept { return header.clientID; }

	int getContentsLength() noexcept { return header.length; }
	int getPacketSerializedLength() noexcept { return sizeof(PacketHeader) + header.length + sizeof(end_mark); }

	// Debug method
	void print_packet_contents(const char* where);
};
#pragma pack(pop)
#endif