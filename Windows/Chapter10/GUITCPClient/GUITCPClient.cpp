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

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// 소켓 통신 스레드 함수
DWORD WINAPI workerSend(LPVOID arg);

// 네트워크
SOCKET		sock{NULL};  // 소켓
char		ipAddress[INET_ADDRSTRLEN];  // 아이피 주소 저장 버퍼
char		buf[BUFSIZE + 1];  // 데이터 송수신 버퍼
std::string path;  // 파일 경로
std::string filename;  // 파일 이름
double		maxSize;  // 전송 중인 파일 최대 크기
double		currentSize;  // 전송 중인 파일 현재 크기 



// 윈도우 핸들 
HWND hSendButton, hFileButton; // 보내기 버튼
HWND hFileEdit, hStatusEdit; // 파일 이름 에디트
HWND hIpAddress; // 파일 선택 에디트 컨트롤
HWND hProgress; // 프로그레스 바


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 대화상자 생성
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		
		// 아이템 초기화
		hFileButton	= GetDlgItem(hDlg, IDC_FILE_BUTTON);
		hSendButton	= GetDlgItem(hDlg, IDC_SEND_BUTTON);
		hIpAddress	= GetDlgItem(hDlg, IDC_IP_CONTROL);
		hProgress	= GetDlgItem(hDlg, IDC_PROGRESS_BAR);
		hFileEdit	= GetDlgItem(hDlg, IDC_FILE_EDIT);
		hStatusEdit = GetDlgItem(hDlg, IDC_STATUS_EDIT);

		// 프로그레스 바 초기 설정
		SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));	// 범위 설정 (0 ~ 100)
		SendMessage(hProgress, PBM_SETSTEP, (WPARAM)1, 0);				// 단계 설정

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SEND_BUTTON:
		{
			// 보내기 버튼 비활성화
			EnableWindow(hSendButton, FALSE);

			// IpAddress에서 IP를 가져온다.
			DWORD ip;
			SendMessage(hIpAddress, IPM_GETADDRESS, 0, (LPARAM)&ip);
			BYTE b1 = FIRST_IPADDRESS(ip);
			BYTE b2 = SECOND_IPADDRESS(ip);
			BYTE b3 = THIRD_IPADDRESS(ip);
			BYTE b4 = FOURTH_IPADDRESS(ip);
			sprintf(ipAddress, "%d.%d.%d.%d", b1, b2, b3, b4);

			// 가져온 IP로 파일을 보내는 쓰레드를 만든다.
			auto th{ CreateThread(NULL, 0, workerSend, NULL, 0, NULL) };
			if (NULL != th) { CloseHandle(th); }
			return TRUE;
		}
	
		case IDC_FILE_BUTTON:
		{
			// 파일 경로를 얻어온다.
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
				// 파일 경로를 Edit에 보여주기
				SetWindowText(hFileEdit, szFile);

				// 파일 경로를 기존 코드에 맞게 string 형태로 저장
				char szFileA[BUFSIZE];
				WideCharToMultiByte(CP_ACP, 0, szFile, -1, szFileA, BUFSIZE / 2, NULL, NULL);
				path = szFileA;

				// 파일 이름을 저장
				const wchar_t* szFilePath = PathFindFileName(szFile);
				WideCharToMultiByte(CP_ACP, 0, szFilePath, -1, szFileA, BUFSIZE / 2, NULL, NULL);
				filename = szFileA;
			}
			return TRUE;
		}

		case IDC_CANCEL_BUTTON:
		{
			EndDialog(hDlg, IDC_CANCEL_BUTTON); // 대화상자 닫기
			if (NULL != sock) { closesocket(sock); }
			return TRUE;
		}
		}
		return FALSE;
	}
	return FALSE;
}

// 에디트 컨트롤 출력 함수
void displayText(const wchar_t *fmt)
{
	SetWindowText(hStatusEdit, fmt);
}

// 파일 크기를 읽는 함수
std::streampos getFileSize(std::ifstream& file)
{
	file.seekg(0, std::ios::end);  // 파일의 마지막 위치로 이동.
	auto fileSize = file.tellg();  // 현재 위치로 파일의 크기를 구함.
	file.seekg(0, std::ios::beg);  // 다시 파일을 시작 위치로 이동.
	return fileSize;
}

// 보내는 함수
void doSend(SOCKET socket, const char* buf, int len)
{
	int retval = send(socket, buf, len, 0);
	if (SOCKET_ERROR == retval) {
		err_display("send()");
		exit(-1);
	}
}

// 데이터 전송 함수
DWORD WINAPI workerSend(LPVOID arg)
{
	int retval;

	std::ifstream file(path, std::ios::binary);
	if (not file) {
		displayText(L"파일이 존재하지 않습니다.");
		EnableWindow(hSendButton, TRUE);
		return 1;
	}

	// 소켓 생성
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

	displayText(L"데이터 전송을 시작합니다.");

	// 서버에게 파일 이름 보내기
	int len = static_cast<int>(filename.size());

	// 파일 이름의 크기를 보내기.
	doSend(sock, (char*)&len, sizeof(int));

	// 데이터 보내기(가변 길이)
	doSend(sock, filename.c_str(), len);


	// 저장할 데이터의 사이즈 보내기
	maxSize = static_cast<double>(getFileSize(file));
	doSend(sock, (char*)&maxSize, sizeof(double));
	
	currentSize = 0.;
	int currentProgress{};

	// 서버와 데이터 통신
	bool loop{ true };
	while (loop) {

		// 데이터 입력(시뮬레이션)
		if (not file.read(buf, BUFSIZE)) {
			loop = false;
		}

		// 파일 길이 읽기
		len = static_cast<int>(file.gcount());
		if (0 == len) break;

		// 데이터 보내기(고정 길이)
		doSend(sock, (char*)&len, sizeof(int));

		// 데이터 보내기(가변 길이)
		doSend(sock, buf, len);

		// 프로그레스 바 업데이트
		currentSize += static_cast<double>(len);
		auto val{ static_cast<int>(min(currentSize / maxSize * 100., 100.)) };
		if (currentProgress < val) {
			currentProgress = val;
			SendMessage(hProgress, PBM_SETPOS, (WPARAM)currentProgress, 0);
		}
		
	}
	displayText(L"전송이 완료되었습니다.");

	// 소켓 닫기
	closesocket(sock);
	sock = NULL;
	EnableWindow(hSendButton, TRUE);
	return 0;
}
