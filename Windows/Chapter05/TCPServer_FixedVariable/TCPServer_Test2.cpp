#include "..\..\Common.h"
#include <fstream>
#include <iostream>
#include <print>
#include <array>
#include <thread>

// ���
enum
{
	SERVERPORT = 9000,
	BUFSIZE = 512,
	SOCKET_SIZE = 1024
};

// �����Ȳ ����� ���� Progress sturct
struct Progress
{
	SOCKET socket{ NULL };
	UINT  id{};
	std::string name{};
	double max{};
	double cur{};
};

// worker���� ���ڸ� �����ϱ� ���� struct
struct WorkerArg
{
	SOCKET socket{};
	UINT id{};
};

// �����Ȳ�� �����ϴ� �迭.
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

inline UINT getIndex(UINT id)
{
	return (id + 1) % SOCKET_SIZE;
}

// Ŀ���� ��ġ�� ���ڷ� ���� ������ �ٲٴ� �Լ�
void moveCursor(UINT line)
{
	// ���� �ܼ� �ڵ��� �����´�
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// ���� Ŀ�� ��ġ�� �����´�
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);

	// Ŀ���� �Է��� �ٿ� ����Ѵ�.
	csbi.dwCursorPosition.Y = static_cast<short>(line);

	// Ŀ�� ��ġ�� ������Ʈ�Ѵ�
	SetConsoleCursorPosition(hConsole, csbi.dwCursorPosition);
}


// �����Ȳ�� ����ϴ� ������.
bool flag{ true };
DWORD WINAPI workerProgress(LPVOID arg)
{
	sockaddr_in clientaddr{};
	int addrlen{ sizeof(clientaddr) };
	char addr[INET_ADDRSTRLEN]{};
	std::string blank(100, ' ');

	while (flag) {
		// ���۷��� ����Ѵ�.
		for (const auto& progress : g_progress) {
			if (NULL == progress.socket) { continue; }
			ZeroMemory(addr, sizeof(addr));
			moveCursor(progress.id);
			getpeername(progress.socket, (struct sockaddr*)&clientaddr, &addrlen);
			inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
			if (progress.cur == progress.max) {
				std::println("\r{}\r[�ּ�: {} | ��Ʈ: {}] {}: ���� �ޱⰡ �Ϸ�Ǿ����ϴ�.",
					blank, addr, ntohs(clientaddr.sin_port), progress.name);
				g_progress[getIndex(progress.id)].socket = NULL;
			}
			else {
				std::println("\r{}\r[�ּ�: {} | ��Ʈ: {}] {} : ���� ���� ��Ȳ: {:.2f}%",
					blank, addr, ntohs(clientaddr.sin_port), progress.name, progress.cur / progress.max * 100.);
			}
		}
	}
}

DWORD WINAPI workerSaveFile(LPVOID arg)
{
	// ���� ��������
	WorkerArg* worker_arg{ reinterpret_cast<WorkerArg*>(arg) };
	SOCKET client_sock{ worker_arg->socket };
	UINT id{ worker_arg->id };
	delete worker_arg;

	UINT index{ getIndex(id) };
	int len{};					// ���� ���� ������
	char buf[BUFSIZE + 1]{};	// ���� ���� ������
	char filename[BUFSIZE + 1]{};

	// ���� �̸� �ޱ� (���� ����)
	auto retval = doRecvWaitAll(client_sock, (char*)&len, sizeof(int));
	if (0 == retval) {
		closesocket(client_sock);
		return 0;
	}

	// ���� �̸� �ޱ� (���� ����)
	retval = doRecvWaitAll(client_sock, buf, len);
	if (0 == retval) {
		closesocket(client_sock);
		return 0;
	}

	// ���� ������ ���� ����
	buf[retval] = '\0';
	std::ofstream file(buf, std::ios::binary);
	if (not file.is_open()) {
		std::cout << buf << ": out ������ �� �� �����ϴ�.\n";
		closesocket(client_sock);
		return 0;
	}

	// ������ �������� ����� �ޱ�
	double file_size{};
	retval = doRecvWaitAll(client_sock, (char*)&file_size, sizeof(double));
	if (0 == retval) { closesocket(client_sock);  return 0; }

	// ���⿡ ���
	g_progress[index].max = file_size;
	g_progress[index].cur = 0.;
	g_progress[index].id = id;
	g_progress[index].name = buf;
	g_progress[index].socket = client_sock;


	// Ŭ���̾�Ʈ�� ������ ���
	while (true) {

		// ������ �ޱ�(���� ����)
		retval = doRecvWaitAll(client_sock, (char*)&len, sizeof(int));
		if (0 == retval) { break; }

		// ������ �ޱ�(���� ����)
		retval = doRecvWaitAll(client_sock, buf, len);
		if (0 == retval) { break; }

		// �����Ȳ ������Ʈ
		g_progress[index].cur += static_cast<double>(len);

		// ���� ������ ����
		file.write(buf, retval);
	}

	// ��� �Ϸ� ���
	while (NULL != g_progress[index].socket) { std::this_thread::yield(); }
	closesocket(client_sock);
	return 0;
}

int main(int argc, char* argv[])
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ���� ����
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

	// ���۷� ��� ������ ����
	auto th_progress{ CreateThread(NULL, 0, workerProgress, (LPVOID)NULL, 0, NULL) };
	if (th_progress == NULL) { exit(1); }

	UINT id{};
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

		// ���� ���� ������ ����
		WorkerArg* arg{ new WorkerArg{ client_sock , id++ } };
		auto th{ CreateThread(NULL, 0, workerSaveFile, reinterpret_cast<LPVOID*>(arg), 0, NULL) };
		if (NULL == th) { closesocket(client_sock); }
		else { CloseHandle(th); }

		// ���� �迭�� ��� �����尡 ������̸� ���.
		while (NULL != g_progress[getIndex(id)].socket) { std::this_thread::yield(); }
	}

	// ��� ������ ����
	flag = false;
	WaitForSingleObject(th_progress, INFINITE);

	// ���� �ݱ�
	closesocket(listen_sock);

	// ���� ����
	WSACleanup();
	return 0;
}
