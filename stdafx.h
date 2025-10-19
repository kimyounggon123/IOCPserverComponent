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

// 단일 메모리 해제
template <typename T>
bool SAFE_FREE(T*& ptr) {
	if (ptr) {
		delete ptr;
		ptr = nullptr;
		return true;
	}
	return false;
}

// 배열 메모리 해제
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
/// 바이트 단위의 endian changer
/// 해당 코드는 바이트 단위로 endian을 change해 Big <-> little 형태로 만들도록 지원하는 커스텀 함수임
/// 
/// 
/// <원리>
/// 기존 값에 1바이트 단위로 마스크를 씌워 특정 위치의 바이트만 추출함. 
/// 4bytes 인 integer는 4번 실행하며, 이렇게 추출한 byte의 위치를 바꿈.
/// 이후 이동시킨 바이트 값 4개를 전부 or operator로 합침
/// </summary>
/// 


inline int changeEndianInt(int value)
{
	unsigned int uvalue = static_cast<unsigned int>(value); // unsigned로 변환
	unsigned int result = (uvalue & 0x000000ff) << 24 | (uvalue & 0x0000ff00) << 8 | (uvalue & 0x00ff0000) >> 8 | (uvalue & 0xff000000) >> 24;
	return static_cast<int>(result);
}
inline short changeEndianShort(short value)
{
	unsigned short uvalue = static_cast<unsigned short>(value); // unsigned로 변환
	unsigned short result =  (uvalue & 0x00ff) << 8 | (uvalue & 0xff00) >> 8;
	return static_cast<short>(result);
}



#endif