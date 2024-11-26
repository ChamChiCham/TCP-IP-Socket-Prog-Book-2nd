#include "..\..\Common.h"
#include "resource.h"

#include <fstream>
#include <string>

#include <commctrl.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")


#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    512

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// ���� ��� ������ �Լ�
DWORD WINAPI workerSend(LPVOID arg);

// ��Ʈ��ũ
SOCKET		sock{NULL};  // ����
char		ipAddress[INET_ADDRSTRLEN];  // ������ �ּ� ���� ����
char		buf[BUFSIZE + 1];  // ������ �ۼ��� ����
std::string path;  // ���� ���
std::string filename;  // ���� �̸�
double		maxSize;  // ���� ���� ���� �ִ� ũ��
double		currentSize;  // ���� ���� ���� ���� ũ�� 



// ������ �ڵ� 
HWND hSendButton, hFileButton; // ������ ��ư
HWND hFileEdit, hStatusEdit; // ���� �̸� ����Ʈ
HWND hIpAddress; // ���� ���� ����Ʈ ��Ʈ��
HWND hProgress; // ���α׷��� ��


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ��ȭ���� ����
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		
		// ������ �ʱ�ȭ
		hFileButton	= GetDlgItem(hDlg, IDC_FILE_BUTTON);
		hSendButton	= GetDlgItem(hDlg, IDC_SEND_BUTTON);
		hIpAddress	= GetDlgItem(hDlg, IDC_IP_CONTROL);
		hProgress	= GetDlgItem(hDlg, IDC_PROGRESS_BAR);
		hFileEdit	= GetDlgItem(hDlg, IDC_FILE_EDIT);
		hStatusEdit = GetDlgItem(hDlg, IDC_STATUS_EDIT);

		// ���α׷��� �� �ʱ� ����
		SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));	// ���� ���� (0 ~ 100)
		SendMessage(hProgress, PBM_SETSTEP, (WPARAM)1, 0);				// �ܰ� ����

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SEND_BUTTON:
		{
			// ������ ��ư ��Ȱ��ȭ
			EnableWindow(hSendButton, FALSE);

			// IpAddress���� IP�� �����´�.
			DWORD ip;
			SendMessage(hIpAddress, IPM_GETADDRESS, 0, (LPARAM)&ip);
			BYTE b1 = FIRST_IPADDRESS(ip);
			BYTE b2 = SECOND_IPADDRESS(ip);
			BYTE b3 = THIRD_IPADDRESS(ip);
			BYTE b4 = FOURTH_IPADDRESS(ip);
			sprintf(ipAddress, "%d.%d.%d.%d", b1, b2, b3, b4);

			// ������ IP�� ������ ������ �����带 �����.
			auto th{ CreateThread(NULL, 0, workerSend, NULL, 0, NULL) };
			if (NULL != th) { CloseHandle(th); }
			return TRUE;
		}
	
		case IDC_FILE_BUTTON:
		{
			// ���� ��θ� ���´�.
			OPENFILENAME ofn;
			wchar_t szFile[BUFSIZE / 2] = { 0 };
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hDlg;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = L"All Files\0*.*\0Text Files\0*.TXT\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			if (GetOpenFileName(&ofn) == TRUE) {
				// ���� ��θ� Edit�� �����ֱ�
				SetWindowText(hFileEdit, szFile);

				// ���� ��θ� ���� �ڵ忡 �°� string ���·� ����
				char szFileA[BUFSIZE];
				WideCharToMultiByte(CP_ACP, 0, szFile, -1, szFileA, BUFSIZE / 2, NULL, NULL);
				path = szFileA;

				// ���� �̸��� ����
				const wchar_t* szFilePath = PathFindFileName(szFile);
				WideCharToMultiByte(CP_ACP, 0, szFilePath, -1, szFileA, BUFSIZE / 2, NULL, NULL);
				filename = szFileA;
			}
			return TRUE;
		}

		case IDC_CANCEL_BUTTON:
		{
			EndDialog(hDlg, IDC_CANCEL_BUTTON); // ��ȭ���� �ݱ�
			if (NULL != sock) { closesocket(sock); }
			return TRUE;
		}
		}
		return FALSE;
	}
	return FALSE;
}

// ����Ʈ ��Ʈ�� ��� �Լ�
void displayText(const wchar_t *fmt)
{
	SetWindowText(hStatusEdit, fmt);
}

// ���� ũ�⸦ �д� �Լ�
std::streampos getFileSize(std::ifstream& file)
{
	file.seekg(0, std::ios::end);  // ������ ������ ��ġ�� �̵�.
	auto fileSize = file.tellg();  // ���� ��ġ�� ������ ũ�⸦ ����.
	file.seekg(0, std::ios::beg);  // �ٽ� ������ ���� ��ġ�� �̵�.
	return fileSize;
}

// ������ �Լ�
void doSend(SOCKET socket, const char* buf, int len)
{
	int retval = send(socket, buf, len, 0);
	if (SOCKET_ERROR == retval) {
		err_display("send()");
		exit(-1);
	}
}

// ������ ���� �Լ�
DWORD WINAPI workerSend(LPVOID arg)
{
	int retval;

	std::ifstream file(path, std::ios::binary);
	if (not file) {
		displayText(L"������ �������� �ʽ��ϴ�.");
		EnableWindow(hSendButton, TRUE);
		return 1;
	}

	// ���� ����
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(ipAddress);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	displayText(L"������ ������ �����մϴ�.");

	// �������� ���� �̸� ������
	int len = static_cast<int>(filename.size());

	// ���� �̸��� ũ�⸦ ������.
	doSend(sock, (char*)&len, sizeof(int));

	// ������ ������(���� ����)
	doSend(sock, filename.c_str(), len);


	// ������ �������� ������ ������
	maxSize = static_cast<double>(getFileSize(file));
	doSend(sock, (char*)&maxSize, sizeof(double));
	
	currentSize = 0.;
	int currentProgress{};

	// ������ ������ ���
	bool loop{ true };
	while (loop) {

		// ������ �Է�(�ùķ��̼�)
		if (not file.read(buf, BUFSIZE)) {
			loop = false;
		}

		// ���� ���� �б�
		len = static_cast<int>(file.gcount());
		if (0 == len) break;

		// ������ ������(���� ����)
		doSend(sock, (char*)&len, sizeof(int));

		// ������ ������(���� ����)
		doSend(sock, buf, len);

		// ���α׷��� �� ������Ʈ
		currentSize += static_cast<double>(len);
		auto val{ static_cast<int>(min(currentSize / maxSize * 100., 100.)) };
		if (currentProgress < val) {
			currentProgress = val;
			SendMessage(hProgress, PBM_SETPOS, (WPARAM)currentProgress, 0);
		}
		
	}
	displayText(L"������ �Ϸ�Ǿ����ϴ�.");

	// ���� �ݱ�
	closesocket(sock);
	sock = NULL;
	EnableWindow(hSendButton, TRUE);
	return 0;
}
