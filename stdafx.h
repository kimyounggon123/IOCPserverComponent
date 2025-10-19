#ifndef _STDAFX_H
#define _STDAFX_H
//////////////////////////////////////////////////////////////////////////////////////////
// You can define macro and include frequently used header like std::vector in here.
//////////////////////////////////////////////////////////////////////////////////////////


#include <cstdint> // int_t

#include <iostream>
#include <vector>
#include <algorithm>

#include <string>
#include <tchar.h> // _t macro
#include <cstring>

#define BUFFERSIZE 1024
#define WINDOW_MODE 1
#define DEBUG_MODE 1

enum class ERROR_CODE : uint8_t {
	OTHER_ERROR, // default error code

	GET_NULLPTR,
	INCORRECT_SIZE,
	MEMORY_LIMIT,
	EMPTY_CONTAINOR,

	UNDEFINED_FUNCTION, // if you run undefined function
	FUNCTION_RUNNING_FAILED, // get function failed result 

	HAVETO_DELETE_PACKET_HEADER,
	NEED_EXTRA_DATA,		// packet comes here imperfectly
	OPENED_PACKET, // if packet doesn't have end mark;

	CONTAINOR_ERROR,
	WRONG_DATA,

	SUCCESS
};


constexpr bool operator!(ERROR_CODE code) {
	return code != ERROR_CODE::SUCCESS;
}


// memory free function

// ���� �޸� ����
template <typename T>
bool SAFE_FREE(T*& ptr) {
	if (ptr) {
		delete ptr;
		ptr = nullptr;
		return true;
	}
	return false;
}

// �迭 �޸� ����
template <typename T>
bool SAFE_FREE_ARRAY(T*& ptr) {
	if (ptr) {
		delete[] ptr;
		ptr = nullptr;
		return true;
	}
	return false;
}


/// <summary>
/// ����Ʈ ������ endian changer
/// �ش� �ڵ�� ����Ʈ ������ endian�� change�� Big <-> little ���·� ���鵵�� �����ϴ� Ŀ���� �Լ���
/// 
/// 
/// <����>
/// ���� ���� 1����Ʈ ������ ����ũ�� ���� Ư�� ��ġ�� ����Ʈ�� ������. 
/// 4bytes �� integer�� 4�� �����ϸ�, �̷��� ������ byte�� ��ġ�� �ٲ�.
/// ���� �̵���Ų ����Ʈ �� 4���� ���� or operator�� ��ħ
/// </summary>
/// 


inline int changeEndianInt(int value)
{
	unsigned int uvalue = static_cast<unsigned int>(value); // unsigned�� ��ȯ
	unsigned int result = (uvalue & 0x000000ff) << 24 | (uvalue & 0x0000ff00) << 8 | (uvalue & 0x00ff0000) >> 8 | (uvalue & 0xff000000) >> 24;
	return static_cast<int>(result);
}
inline short changeEndianShort(short value)
{
	unsigned short uvalue = static_cast<unsigned short>(value); // unsigned�� ��ȯ
	unsigned short result =  (uvalue & 0x00ff) << 8 | (uvalue & 0xff00) >> 8;
	return static_cast<short>(result);
}



#endif