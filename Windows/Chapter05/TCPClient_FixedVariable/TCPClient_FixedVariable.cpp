#include "..\..\Common.h"
#include <iostream>
#include <fstream>

#define SERVERPORT 9000
#define BUFSIZE    512

std::streampos getFileSize(std::ifstream& file)
{
	file.seekg(0, std::ios::end);  // ������ ������ ��ġ�� �̵�.
	auto fileSize = file.tellg();  // ���� ��ġ�� ������ ũ�⸦ ����.
	file.seekg(0, std::ios::beg);  // �ٽ� ������ ���� ��ġ�� �̵�.
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

	// ���ڷ� ���� ������ �̸��� �����´�.
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
		std::cout << "���� �ּ� �Ǵ� ���� �̸��� �����ϴ�.\n";
		return 1;
	}

	// �̸��� ���� ���� ��Ʈ���� �����.
	std::ifstream file(filename, std::ios::binary);
	if (not file) {
		std::cout << "�ش� �̸��� ���� ������ �����ϴ�.\n";
		return 1;
	}



	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ���� ����
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


	std::cout << "������ ������ �����մϴ�...\n";


	// ������ ��ſ� ����� ����
	char buf[BUFSIZE]{};
	int len{};
	bool loop{ true };

	// �������� ���� �̸� ������
	len = static_cast<int>(filename.size());

	// ���� �̸��� ũ�⸦ ������.
	doSend(sock, (char*)&len, sizeof(int));

	// ������ ������(���� ����)
	doSend(sock, filename.c_str(), len);


	// ������ �������� ������ ������
	double file_size{ static_cast<double>( getFileSize(file)) };
	doSend(sock, (char*)&file_size, sizeof(double));


	// ������ ������ ���
	while (loop) {

		// ������ �Է�(�ùķ��̼�)
		if (not file.read(buf, BUFSIZE)) {
			loop = false;
		}

		// ���� ���� �б�
		len = static_cast<int>(file.gcount());
		if (0 == len) break;

		// ������ ������(���� ����)
		doSend(sock, (char *)&len, sizeof(int));

		// ������ ������(���� ����)
		doSend(sock, buf, len);
	}
	std::cout << "������ �Ϸ�Ǿ����ϴ�." << '\n';

	// ���� �ݱ�
	closesocket(sock);

	// ���� ����
	WSACleanup();
	return 0;
}
