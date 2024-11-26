#include "..\..\Common.h"
#include <iostream>
#include <fstream>

#define SERVERPORT 9000
#define BUFSIZE    512

std::streampos getFileSize(std::ifstream& file)
{
	file.seekg(0, std::ios::end);  // 파일의 마지막 위치로 이동.
	auto fileSize = file.tellg();  // 현재 위치로 파일의 크기를 구함.
	file.seekg(0, std::ios::beg);  // 다시 파일을 시작 위치로 이동.
	return fileSize;
}

void doSend(SOCKET socket, const char* buf, int len)
{
	int retval = send(socket, buf, len, 0);
	if (SOCKET_ERROR == retval) {
		err_display("send()");
		exit(-1);
	}	
}


int main(int argc, char *argv[])
{
	std::string filename{};

	// 인자로 들어온 파일의 이름을 가져온다.
	const char* SERVERIP{};
	if (argc > 2) {
		SERVERIP = argv[1];
		filename = argv[2];
		for (int i{ 3 }; i < argc; ++i) {
			filename += ' ';
			filename += argv[i];
		}
	}
	else {
		std::cout << "서버 주소 또는 파일 이름이 없습니다.\n";
		return 1;
	}

	// 이름을 통해 파일 스트림을 만든다.
	std::ifstream file(filename, std::ios::binary);
	if (not file) {
		std::cout << "해당 이름을 가진 파일이 없습니다.\n";
		return 1;
	}



	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;

	inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(SERVERPORT);
	auto retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");


	std::cout << "데이터 전송을 시작합니다...\n";


	// 데이터 통신에 사용할 변수
	char buf[BUFSIZE]{};
	int len{};
	bool loop{ true };

	// 서버에게 파일 이름 보내기
	len = static_cast<int>(filename.size());

	// 파일 이름의 크기를 보내기.
	doSend(sock, (char*)&len, sizeof(int));

	// 데이터 보내기(가변 길이)
	doSend(sock, filename.c_str(), len);


	// 저장할 데이터의 사이즈 보내기
	double file_size{ static_cast<double>( getFileSize(file)) };
	doSend(sock, (char*)&file_size, sizeof(double));


	// 서버와 데이터 통신
	while (loop) {

		// 데이터 입력(시뮬레이션)
		if (not file.read(buf, BUFSIZE)) {
			loop = false;
		}

		// 파일 길이 읽기
		len = static_cast<int>(file.gcount());
		if (0 == len) break;

		// 데이터 보내기(고정 길이)
		doSend(sock, (char *)&len, sizeof(int));

		// 데이터 보내기(가변 길이)
		doSend(sock, buf, len);
	}
	std::cout << "전송이 완료되었습니다." << '\n';

	// 소켓 닫기
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}
