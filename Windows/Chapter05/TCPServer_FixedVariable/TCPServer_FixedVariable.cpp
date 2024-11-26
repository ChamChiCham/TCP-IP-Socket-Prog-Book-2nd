#include "..\..\Common.h"
#include <fstream>
#include <iostream>
#include <print>
#include <unordered_map>

enum
{
	SERVERPORT	= 9000,
	BUFSIZE		= 512,
	SOCKET_SIZE = 1000
};

struct Progress {
	double max{};
	double cur{};
};

std::unordered_map<SOCKET, Progress> g_progress_table(SOCKET_SIZE);

int doRecvWaitAll(SOCKET socket, char* buf, int len)
{
	int retval = recv(socket, buf, len, MSG_WAITALL);
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		retval = 0;
	}
	return retval;
}


void moveCursorUp() {
	// 현재 콘솔 핸들을 가져온다
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// 현재 커서 위치를 가져온다
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);

	// 커서를 위쪽으로 이동시킨다.
	csbi.dwCursorPosition.Y = 0;

	// 커서 위치를 업데이트한다
	SetConsoleCursorPosition(hConsole, csbi.dwCursorPosition);
}

bool flag{ true };
DWORD WINAPI workerProgress(LPVOID arg)
{
	sockaddr_in clientaddr{};
	int addrlen{ sizeof(clientaddr) };
	char addr[INET_ADDRSTRLEN]{};
	std::string blank(100, ' ');

	while (flag) {
		// 커서를 올린다.
		moveCursorUp();
		
		// 전송률을 출력한다.
		for (const auto [socket, progress] : g_progress_table) {
			getpeername(socket, (struct sockaddr*)&clientaddr, &addrlen);
			inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
			if (progress.cur == progress.max) {
				std::println("\r{}\r[주소: {} | 포트: {}]: 파일 받기가 완료되었습니다.",
					blank, addr, ntohs(clientaddr.sin_port));
			}
			else {
				std::println("\r{}\r[주소: {} | 포트: {}]: 현재 진행 상황: {:.2f}%",
					blank, addr, ntohs(clientaddr.sin_port), progress.cur / progress.max * 100.);
			}
		}
	}
}

DWORD WINAPI workerSaveFile(LPVOID arg)
{
	SOCKET client_sock = (SOCKET)arg;
	int len{};					// 고정 길이 데이터
	char buf[BUFSIZE + 1]{};	// 가변 길이 데이터

	// 파일 이름 받기 (고정 길이)
	auto retval = doRecvWaitAll(client_sock, (char*)&len, sizeof(int));
	if (0 == retval) {
		closesocket(client_sock);
		return 0;
	}

	// 파일 이름 받기 (가변 길이)
	retval = doRecvWaitAll(client_sock, buf, len);
	if (0 == retval) {
		closesocket(client_sock);
		return 0;
	}

	// 받은 정보로 파일 열기
	buf[retval] = '\0';
	std::ofstream file(buf, std::ios::binary);
	if (not file.is_open()) {
		std::cout << buf << ": out 파일을 열 수 없습니다.\n";
		closesocket(client_sock);
		return 0;
	}

	// 저장할 데이터의 사이즈를 받기
	double file_size{};
	retval = doRecvWaitAll(client_sock, (char*)&file_size, sizeof(double));
	g_progress_table[client_sock] = Progress{ file_size, double{} };

	// 클라이언트와 데이터 통신
	while (true) {

		// 데이터 받기(고정 길이)
		retval = doRecvWaitAll(client_sock, (char*)&len, sizeof(int));
		if (0 == retval) { break; }

		// 데이터 받기(가변 길이)
		retval = doRecvWaitAll(client_sock, buf, len);
		if (0 == retval) { break; }

		g_progress_table[client_sock].cur += static_cast<double>(len);

		// 받은 데이터 저장
		file.write(buf, retval);
	}

	// 클라이언트 주소 가져오기
	sockaddr_in clientaddr{};
	int addrlen{ sizeof(clientaddr) };
	char addr[INET_ADDRSTRLEN]{};

	// 진행 테이블에서 소켓 삭제 및 소켓 닫기
	Sleep(3000);
	g_progress_table.erase(client_sock);
	closesocket(client_sock);
	system("cls");
	return 0;
}

int main(int argc, char *argv[])
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	int retval = bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// 전송률 출력 스레드 생성
	auto th_progress = CreateThread(NULL, 0, workerProgress, (LPVOID)NULL, 0, NULL);
	if (th_progress == NULL) { exit(1); }

	while (true) {

		// accept()
		sockaddr_in clientaddr{};
		int addrlen{ sizeof(clientaddr) };
		auto client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// 파일 저장 스레드 생성
		while (g_progress_table.size() >= SOCKET_SIZE);
		auto th = CreateThread(NULL, 0, workerSaveFile, (LPVOID)client_sock, 0, NULL);
		if (NULL == th) { closesocket(client_sock); }
		else { CloseHandle(th); }
	}

	// 출력 스레드 종료
	flag = false;
	WaitForSingleObject(th_progress, INFINITE);

	// 소켓 닫기
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}
