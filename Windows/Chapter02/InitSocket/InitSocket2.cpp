#include "..\..\Common.h"
#include <iostream>
#include <string>
#include <format>

constexpr WORD make_word(BYTE first, BYTE second) noexcept
{
	return (static_cast<WORD>(first) << 8) | static_cast<WORD>(second);
}

constexpr BYTE high_byte(WORD word) noexcept
{
	return static_cast<BYTE>(word >> 8);
}

constexpr BYTE low_byte(WORD word) noexcept
{
	return static_cast<BYTE>(word);
}

std::string word_to_version(WORD word) noexcept
{
	return std::format("{:d}.{:d}", high_byte(word), low_byte(word));
}

int main(int argc, char *argv[])
{	
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(make_word(2, 2), &wsa) != 0)
		return 1;
	printf("[알림] 윈속 초기화 성공\n");


	// wsa 변수 필드 출력
	std::cout
		<< "wVersion: " << word_to_version(wsa.wVersion) << std::endl
		<< "wHighVersion: " << word_to_version(wsa.wHighVersion) << std::endl
		<< "szDescription: " << wsa.szDescription << std::endl
		<< "szSystemStatus: " << wsa.szSystemStatus << std::endl;
	

	// 윈속 종료
	WSACleanup();
	return 0;
}
