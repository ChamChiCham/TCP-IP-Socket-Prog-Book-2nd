#include "..\..\Common.h"
#include <fstream>
#include <iostream>
#include <print>
#include <array>
#include <thread>

// 상수
enum
{
	SERVERPORT = 9000,
	BUFSIZE = 512,
	SOCKET_SIZE = 1000
};

// 진행상황 출력을 위한 Progress sturct
struct Progress
{
	SOCKET socket{ NULL };
	double max{};
	double cur{};
};

// worker에게 인자를 전달하기 위한 struct
struct WorkerArg
{
	SOCKET socket{};
	int id{};
};

// 진행상황을 저장하는 배열.
std::array<Progress, SOCKET_SIZE> g_progress;

int doRecvWaitAll(SOCKET socket, char* buf, int len)
{
	int retval = recv(socket, buf, len, MSG_WAITALL);
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		retval = 0;
	}
	return retval;
}

// 커서를 맨 위로 올리는 함수
void moveCursorUp()
{
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


// 진행상황을 출력하는 스레드.
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
		for (const auto& progress : g_progress) {
			if (NULL == progress.socket) { continue; }
			getpeername(progress.socket, (struct sockaddr*)&clientaddr, &addrlen);
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
	// 인자 가져오기
	WorkerArg* worker_arg{ reinterpret_cast<WorkerArg*>(arg) };
	SOCKET client_sock{ worker_arg->socket };
	int id{ worker_arg->id };
	delete worker_arg;

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
	if (0 == retval) { closesocket(client_sock);  return 0; }
	
	// 여기에 등록
	g_progress[id].max = file_size;
	g_progress[id].cur = 0.;
	g_progress[id].socket = client_sock;


	// 클라이언트와 데이터 통신
	while (true) {

		// 데이터 받기(고정 길이)
		retval = doRecvWaitAll(client_sock, (char*)&len, sizeof(int));
		if (0 == retval) { break; }

		// 데이터 받기(가변 길이)
		retval = doRecvWaitAll(client_sock, buf, len);
		if (0 == retval) { break; }

		// 진행상황 업데이트
		g_progress[id].cur += static_cast<double>(len);

		// 받은 데이터 저장
		file.write(buf, retval);
	}

	// 종료 확인할 시간 제공.
	Sleep(3000);

	// 진행 배열에서 소켓 삭제 및 소켓 닫기
	g_progress[id].socket = NULL;
	closesocket(client_sock);
	system("cls");
	return 0;
}

int main(int argc, char* argv[])
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
	int retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// 전송률 출력 스레드 생성
	auto th_progress{ CreateThread(NULL, 0, workerProgress, (LPVOID)NULL, 0, NULL) };
	if (th_progress == NULL) { exit(1); }

	int id{};
	system("cls");
	while (true) {

		// accept()
		sockaddr_in clientaddr{};
		int addrlen{ sizeof(clientaddr) };
		auto client_sock{ accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen) };
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}

		// 파일 저장 스레드 생성
		WorkerArg* arg{ new WorkerArg{ client_sock , id } };
		auto th{ CreateThread(NULL, 0, workerSaveFile, reinterpret_cast<LPVOID*>(arg), 0, NULL) };
		if (NULL == th) { closesocket(client_sock); }
		else { CloseHandle(th); }
		id = (id + 1) % SOCKET_SIZE;

		// 진행 배열의 모든 스레드가 사용중이면 대기.
		while (NULL != g_progress[id].socket) { std::this_thread::yield(); }
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
