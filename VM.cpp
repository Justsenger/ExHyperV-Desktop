#define WIN32_LEAN_AND_MEAN
#define HV_PROTOCOL_RAW 1
#define DEFAULT_BUFFER_LENGTH	32768

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <combaseapi.h>
#include <vector>
#include <fstream>
#pragma comment(lib, "Ws2_32.lib")

//HvSock�ṹ
struct SOCKADDR_HV
{
	ADDRESS_FAMILY Family;
	USHORT Reserved;
	GUID VmId;
	GUID ServiceId;
};

class Client {
public:
	Client() {ConnectSocket = INVALID_SOCKET;} //��ʼ������

	//��������
	bool Start() {
		WSADATA wsaData;
		SOCKADDR_HV clientService;
		CLSID VmID, ServiceID;

		// ��ʼ��GUID
		const wchar_t* clsid_str = L"{a42e7cda-d03f-480c-9cc2-a4de20abb878}"; // ������ĵ�����������ж��ܽ���
		CLSIDFromString(clsid_str, &VmID);
		clsid_str = L"{2b174636-bc38-474e-9396-b87f3877c1e8}";
		CLSIDFromString(clsid_str, &ServiceID); //����GUID

		CONST GUID* vmId = &VmID;
		CONST GUID* serviceId = &ServiceID;

		// ��ʼ�� Winsock
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
		{
			printf("WSAStartup failed: %d\n", iResult);
			return false;
		}

		struct addrinfo* result = NULL,* ptr = NULL,hints;

		//����Э��ΪHvsocket
		ZeroMemory(&clientService, sizeof(clientService));
		clientService.Family = AF_HYPERV;
		clientService.VmId = *vmId;
		clientService.ServiceId = *serviceId;

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_HYPERV;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = HV_PROTOCOL_RAW;
		hints.ai_addrlen = sizeof(SOCKADDR_HV);
		hints.ai_addr = reinterpret_cast<SOCKADDR*>(&clientService);
		ConnectSocket = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);

		if (ConnectSocket == INVALID_SOCKET)
		{
			printf("Socket����ʧ�ܣ�%d\n", WSAGetLastError());
			freeaddrinfo(result);
			WSACleanup();
			return false;
		}

		// ׼�����ӵ�������
		iResult = connect(ConnectSocket, hints.ai_addr, (int)hints.ai_addrlen);

		if (iResult == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET; //���ڴ���λ
		}

		freeaddrinfo(result);

		if (ConnectSocket == INVALID_SOCKET)
		{
			printf("�޷����ӵ����������������: %d\n", WSAGetLastError());
			WSACleanup();
			return false;
		}

		return true;
	};

	//�ر�����
	void Stop() {
		int iResult = shutdown(ConnectSocket, SD_SEND);

		if (iResult == SOCKET_ERROR)
		{
			printf("�ر�����ʧ�ܣ�%d\n", WSAGetLastError());
		}

		closesocket(ConnectSocket);
		WSACleanup();
		printf("�ر����ӳɹ�\n");
	};

	// ����������Ϣ��������
	bool Send(char* szMsg)
	{

		int iResult = send(ConnectSocket, szMsg, strlen(szMsg), 0);

		if (iResult == SOCKET_ERROR)
		{
			printf("����ʧ��: %d\n", WSAGetLastError());
			closesocket(ConnectSocket); 
			WSACleanup();
			Stop();
			return false;
		}

		return true;
	};

	// �ӷ�����������Ϣ
	bool Recv()
	{
		char recvbuf[DEFAULT_BUFFER_LENGTH];
		int iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFFER_LENGTH, 0); //�յ�����Ϣ
		if (iResult > 0)
		{
			char msg[DEFAULT_BUFFER_LENGTH];
			memset(&msg, 0, sizeof(msg));
			strncpy_s(msg, recvbuf, iResult); //���յ�����Ϣ���浽������msg��
			printf("�յ��ظ�:%s\n", msg);
			return true;
		}
		return false;
	}

	bool SendImage(const std::vector<BYTE>& imageData) {
		// ����ͼ������
		int imageSize = imageData.size();
		int iResult = send(ConnectSocket, (char*)imageData.data(), imageSize, 0);
		if (iResult == SOCKET_ERROR) {
			printf("����ͼ������ʧ�ܣ�������룺%d\n", WSAGetLastError());
			return false;
		}

		// �ȴ�������ȷ��ͼ���ѽ���
		char buffer[DEFAULT_BUFFER_LENGTH];
		iResult = recv(ConnectSocket, buffer, sizeof(buffer) - 1, 0);  // ����һ���ռ�����ַ�
		if (iResult > 0) {
			buffer[iResult] = '\0';  // ȷ���ַ�����ֹ
			printf("�յ����Է�������ȷ����Ϣ��%s\n", buffer);
		}
		else {
			printf("û���յ���������ȷ����Ϣ��\n");
		}
		return true;
	}

private:
	SOCKET ConnectSocket;
};


// ��ȡ��Ļ������ͼ������
std::vector<BYTE> CaptureScreen() {
	// ��ȡ��Ļ���豸������
	HDC hdcScreen = GetDC(NULL);

	// ��ȡ��Ļ��Ⱥ͸߶�
	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);

	// �����ڴ��豸������
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	// ��������Ļ��ͬ��С��λͼ
	HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);

	// ����Ļ���ݸ��Ƶ�λͼ
	SelectObject(hdcMem, hBitmap);
	BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);

	// ��ȡλͼ��λͼ��Ϣ
	BITMAP bitmap;
	GetObject(hBitmap, sizeof(bitmap), &bitmap);

	// ��ȡλͼ���ݲ����浽������
	int imgSize = bitmap.bmWidth * bitmap.bmHeight * 4;  // ÿ������ 4 �ֽ� (RGBA)
	std::vector<BYTE> imageData(imgSize);

	// ������ɫ��Ϣ
	BITMAPINFOHEADER bmiHeader = { sizeof(BITMAPINFOHEADER), bitmap.bmWidth, bitmap.bmHeight, 1, 32 };
	BITMAPINFO bmi = { bmiHeader, { 0 } };

	// ��ȡͼ������
	GetDIBits(hdcMem, hBitmap, 0, bitmap.bmHeight, imageData.data(), &bmi, DIB_RGB_COLORS);

	// ������Դ
	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);

	return imageData;
}


int main(int argc, CHAR* argv[])
{
	std::string msg;
	Client client;
	if (!client.Start()) return 1;


	// Э�̽׶Σ����ͷֱ��ʡ�ͼ���С����Ϣ
	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);
	int imageSize = width * height * 4;  // ����ÿ������ռ��4�ֽڣ�RGBA��ʽ��

	// ���ͷֱ��ʺ�ͼ���С
	if (!client.Send((char*)&width)) {
		printf("���Ϳ��ʧ��\n");
		client.Stop();
		return 1;
	}

	if (!client.Send((char*)&height)) {
		printf("���͸߶�ʧ��\n");
		client.Stop();
		return 1;
	}

	if (!client.Send((char*)&imageSize)) {
		printf("����ͼ���Сʧ��\n");
		client.Stop();
		return 1;
	}

	// �ȴ�������ȷ��Э�̳ɹ�
	if (!client.Recv()) {
		printf("Э��ʧ�ܣ�δ�յ�������ȷ��\n");
		client.Stop();
		return 1;
	}

	// Э�̳ɹ�����ʼ����ͼ������
	while (true) {
		std::vector<BYTE> imageData = CaptureScreen();
		if (!client.SendImage(imageData)) {
			printf("ͼ����ʧ��\n");
			break;
		}
	}

	while (true) {
		std::vector<BYTE> imageData = CaptureScreen();
		if (!client.SendImage(imageData)) {
			printf("ͼ����ʧ��\n");
			break;
		}
	}
	client.Stop();
	return 0;
}
