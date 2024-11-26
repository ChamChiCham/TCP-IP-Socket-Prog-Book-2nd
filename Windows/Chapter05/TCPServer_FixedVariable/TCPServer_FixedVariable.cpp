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
	// ���� �ܼ� �ڵ��� �����´�
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// ���� Ŀ�� ��ġ�� �����´�
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);

	// Ŀ���� �������� �̵���Ų��.
	csbi.dwCursorPosition.Y = 0;

	// Ŀ�� ��ġ�� ������Ʈ�Ѵ�
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
		// Ŀ���� �ø���.
		moveCursorUp();
		
		// ���۷��� ����Ѵ�.
		for (const auto [socket, progress] : g_progress_table) {
			getpeername(socket, (struct sockaddr*)&clientaddr, &addrlen);
			inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
			if (progress.cur == progress.max) {
				std::println("\r{}\r[�ּ�: {} | ��Ʈ: {}]: ���� �ޱⰡ �Ϸ�Ǿ����ϴ�.",
					blank, addr, ntohs(clientaddr.sin_port));
			}
			else {
				std::println("\r{}\r[�ּ�: {} | ��Ʈ: {}]: ���� ���� ��Ȳ: {:.2f}%",
					blank, addr, ntohs(clientaddr.sin_port), progress.cur / progress.max * 100.);
			}
		}
	}
}

DWORD WINAPI workerSaveFile(LPVOID arg)
{
	SOCKET client_sock = (SOCKET)arg;
	int len{};					// ���� ���� ������
	char buf[BUFSIZE + 1]{};	// ���� ���� ������

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
	g_progress_table[client_sock] = Progress{ file_size, double{} };

	// Ŭ���̾�Ʈ�� ������ ���
	while (true) {

		// ������ �ޱ�(���� ����)
		retval = doRecvWaitAll(client_sock, (char*)&len, sizeof(int));
		if (0 == retval) { break; }

		// ������ �ޱ�(���� ����)
		retval = doRecvWaitAll(client_sock, buf, len);
		if (0 == retval) { break; }

		g_progress_table[client_sock].cur += static_cast<double>(len);

		// ���� ������ ����
		file.write(buf, retval);
	}

	// Ŭ���̾�Ʈ �ּ� ��������
	sockaddr_in clientaddr{};
	int addrlen{ sizeof(clientaddr) };
	char addr[INET_ADDRSTRLEN]{};

	// ���� ���̺��� ���� ���� �� ���� �ݱ�
	Sleep(3000);
	g_progress_table.erase(client_sock);
	closesocket(client_sock);
	system("cls");
	return 0;
}

int main(int argc, char *argv[])
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
	int retval = bind(listen_sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ���۷� ��� ������ ����
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

		// ���� ���� ������ ����
		while (g_progress_table.size() >= SOCKET_SIZE);
		auto th = CreateThread(NULL, 0, workerSaveFile, (LPVOID)client_sock, 0, NULL);
		if (NULL == th) { closesocket(client_sock); }
		else { CloseHandle(th); }
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
