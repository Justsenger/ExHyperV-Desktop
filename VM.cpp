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
#include <sstream>

#include <windows.h>
#include <stdio.h>
#include <vector>
// ȫ��֡��ͳ�Ʊ���
unsigned long frameCount = 0;
DWORD lastReportTime = GetTickCount();



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
	Client() { ConnectSocket = INVALID_SOCKET; } //��ʼ������

	//��������
	bool Start() {
		WSADATA wsaData;
		SOCKADDR_HV clientService;
		CLSID VmID, ServiceID;

		// ��ʼ��GUID
		const wchar_t* clsid_str = L"{a42e7cda-d03f-480c-9cc2-a4de20abb878}"; // ������ĵ�����������ж��ܽ���
		CLSIDFromString(clsid_str, &VmID);
		clsid_str = L"{1f6be6bc-3e37-4d4a-97e3-46e7a5bdf739}";
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

		struct addrinfo* result = NULL, * ptr = NULL, hints;

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
		//char buffer[DEFAULT_BUFFER_LENGTH];
		//iResult = recv(ConnectSocket, buffer, sizeof(buffer) - 1, 0);  // ����һ���ռ�����ַ�
		//if (iResult > 0) {
		//	buffer[iResult] = '\0';  // ȷ���ַ�����ֹ
		//	printf("�յ����Է�������ȷ����Ϣ��%s\n", buffer);
		//}
		//else {
		//	printf("û���յ���������ȷ����Ϣ��\n");
		//}
		return true;
	}

private:
	SOCKET ConnectSocket;
};


#include <iostream>
#include <vector>
#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <fstream>

// ����ȫ�ֱ���
IDXGIOutputDuplication* g_pDeskDupl = nullptr;
ID3D11Device* g_d3dDevice = nullptr;
ID3D11DeviceContext* g_d3dContext = nullptr;
IDXGIAdapter1* g_dxgiAdapter = nullptr;
IDXGIOutput* g_dxgiOutput = nullptr;



// ��ʼ�� D3D �豸�����渴�ƽӿ�
HRESULT InitD3D() {
	if (g_d3dDevice && g_d3dContext) {
		return S_OK; // ����豸���������Ѿ�������ֱ�ӷ���
	}

	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &g_d3dDevice, &featureLevel, &g_d3dContext);
	if (FAILED(hr)) {
		return hr;
	}

	IDXGIDevice* dxgiDevice = nullptr;
	hr = g_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
	if (FAILED(hr)) {
		return hr;
	}

	hr = dxgiDevice->GetAdapter((IDXGIAdapter**)&g_dxgiAdapter);
	dxgiDevice->Release();
	if (FAILED(hr)) {
		return hr;
	}

	hr = g_dxgiAdapter->EnumOutputs(0, &g_dxgiOutput);  // ��ȡ�������ʾ����
	if (FAILED(hr)) {
		return hr;
	}

	// ����������ƽӿ�
	IDXGIOutput1* g_dxgiOutput1 = nullptr;
	hr = g_dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&g_dxgiOutput1);
	if (FAILED(hr)) {
		return hr;
	}

	hr = g_dxgiOutput1->DuplicateOutput(g_d3dDevice, &g_pDeskDupl);
	if (FAILED(hr)) {
		return hr;
	}

	return S_OK;
}


// ������Ļ���ݲ����ض���������
std::vector<BYTE> CaptureScreenD3D() {
	if (!g_pDeskDupl) {
		std::cerr << "���渴�ƽӿ�δ��ʼ��!" << std::endl;
		return {};
	}

	// ����֡
	IDXGIResource* pDesktopResource = nullptr;
	DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
	HRESULT hr = g_pDeskDupl->AcquireNextFrame(0, &frameInfo, &pDesktopResource);



	if (FAILED(hr)) {
		//std::cerr << "�޷���ȡ��һ֡! HRESULT: " << std::hex << hr << std::dec << std::endl;
		return {}; // ����֡ʧ��
	}

	// ��ȡ����֡�ı���
	ID3D11Texture2D* pCapturedFrame = nullptr;
	hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pCapturedFrame);
	pDesktopResource->Release();
	if (FAILED(hr)) {
		std::cerr << "�޷���ѯ�����֡!" << std::endl;
		return {};
	}

	// ��ȡ�����������Ϣ
	D3D11_TEXTURE2D_DESC desc0;
	pCapturedFrame->GetDesc(&desc0);

	// �������֡����ϸ��Ϣ
	//std::cout << "Captured Frame Texture Description:" << std::endl;
	//std::cout << "Width: " << desc0.Width << std::endl;
	//std::cout << "Height: " << desc0.Height << std::endl;
	//std::cout << "MipLevels: " << desc0.MipLevels << std::endl;
	//std::cout << "ArraySize: " << desc0.ArraySize << std::endl;
	//std::cout << "Format: " << desc0.Format << std::endl;  // ��ӡ��ʽö��ֵ
	//std::cout << "Usage: " << desc0.Usage << std::endl;
	//std::cout << "BindFlags: " << desc0.BindFlags << std::endl;
	//std::cout << "CPUAccessFlags: " << desc0.CPUAccessFlags << std::endl;
	//std::cout << "MiscFlags: " << desc0.MiscFlags << std::endl;

	// ����Ŀ������ʹ��֧��ӳ��ĸ�ʽ��B8G8R8A8_UNORM��
	D3D11_TEXTURE2D_DESC desc = desc0;
	desc.Usage = D3D11_USAGE_STAGING;  // ����Ϊ D3D11_USAGE_STAGING������ӳ��
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;  // �����ȡ
	desc.BindFlags = 0;  // �����а�
	desc.SampleDesc.Count = 1;  // ǿ������Ϊ������
	desc.SampleDesc.Quality = 0;  // �����ȼ���Ϊ0
	desc.MiscFlags = 0;  // ����Ϊ0����ʹ����������
	//desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // ����Ŀ���ʽ

	// ��ӡĿ�����������
	//std::cout << "Target Texture Description:" << std::endl;
	//std::cout << "Width: " << desc.Width << std::endl;
	//std::cout << "Height: " << desc.Height << std::endl;
	//std::cout << "MipLevels: " << desc.MipLevels << std::endl;
	//std::cout << "ArraySize: " << desc.ArraySize << std::endl;
	//std::cout << "Format: " << desc.Format << std::endl;
	//std::cout << "Usage: " << desc.Usage << std::endl;
	//std::cout << "BindFlags: " << desc.BindFlags << std::endl;
	//std::cout << "CPUAccessFlags: " << desc.CPUAccessFlags << std::endl;
	//std::cout << "MiscFlags: " << desc.MiscFlags << std::endl;

	// ����Ŀ������
	ID3D11Texture2D* pConvertedTexture = nullptr;
	HRESULT hr2 = g_d3dDevice->CreateTexture2D(&desc, nullptr, &pConvertedTexture);
	if (FAILED(hr2)) {
		std::cerr << "����Ŀ������ʧ��! HRESULT: " << std::hex << hr2 << std::dec << std::endl;
		pCapturedFrame->Release();
		return {};  // ����Ŀ������ʧ��
	}

	// ���Ʋ���֡��Ŀ������
	g_d3dContext->CopyResource(pConvertedTexture, pCapturedFrame);

	// ������չ��Ϊ�����Ա��ȡ
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	hr = g_d3dContext->Map(pConvertedTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(hr)) {
		std::cerr << "ӳ������ʧ��! HRESULT: " << std::hex << hr << std::dec << std::endl;
		pCapturedFrame->Release();
		pConvertedTexture->Release();
		return {};  // ӳ��ʧ��
	}

	// ��ȡ���ݲ�����Ϊ������
	int width = static_cast<int>(desc.Width);
	int height = static_cast<int>(desc.Height);

	std::vector<BYTE> imageData;
	imageData.resize(width * height * 4); // 4 �ֽ�ÿ���أ�RGBA ��ʽ��

	// ������������
	memcpy(imageData.data(), mappedResource.pData, width * height * 4);

	g_d3dContext->Unmap(pConvertedTexture, 0);
	pCapturedFrame->Release();
	pConvertedTexture->Release();

	// �ͷŲ����֡
	g_pDeskDupl->ReleaseFrame();

	return imageData;


}



void SaveDataAsBinary(const std::vector<BYTE>& data, const std::string& filename)
{
	std::ofstream outFile(filename, std::ios::binary);
	if (!outFile.is_open()) {
		// ���ļ�ʧ�ܣ��ɸ�����Ҫ��Ӵ�����
		return;
	}

	// ������ data д��������ļ�
	outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
	outFile.close();
}


int main(int argc, CHAR* argv[])
{

	//Sleep(10000);
	std::string msg;
	Client client;
	if (!client.Start()) return 1;
	// ����һ���ָ�������������ʹ�� | ���ָ�
	const char delimiter = '|';

	// ��ȡ��Ļ�ֱ���
	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);

	// ����ֱ��ʵĵ�����Ϣ
	printf("��Ļ���: %d, ��Ļ�߶�: %d\n", width, height);

	if (width == 0 || height == 0) {
		printf("�����޷���ȡ��Ļ�ֱ��ʣ�\n");
		client.Stop();
		return 1;
	}

	// ����ͼ���С������ÿ������4�ֽڣ�RGBA��ʽ��
	int imageSize = width * height * 4;  // 4�ֽ�ÿ���أ�RGBA��

	// ���ͼ���С�ĵ�����Ϣ
	printf("�������ͼ���С: %d �ֽ�\n", imageSize);

	// ʹ���ַ�����������ƴ�ӳ��ַ���
	std::stringstream ss;
	ss << width << delimiter << height << delimiter << imageSize << delimiter;

	// ��ȡƴ�Ӻ���ַ���
	std::string dataToSend = ss.str();

	// ���ַ���ת��Ϊ C ����ַ���
	const char* cstr = dataToSend.c_str();

	// ������������
	if (!client.Send(const_cast<char*>(cstr))) {
		printf("����ͼ�����ʧ��\n");
		client.Stop();
		return 1;
	}
	else {
		printf("ͼ��������ͳɹ�\n");
	}

	// �ȴ�������ȷ��Э�̳ɹ�
	if (!client.Recv()) {
		printf("Э��ʧ�ܣ�δ�յ�������ȷ��\n");
		client.Stop();
		return 1;
	}
	else {
		printf("�������Ѿ�ȷ�ϲ���\n");
	}




	// Э�̳ɹ�����ʼ����ͼ������
	while (true) {



		// ��ʼ�� D3D �豸
		HRESULT hr = InitD3D();
		if (FAILED(hr)) {
			std::cerr << "D3D ��ʼ��ʧ��! HRESULT: " << std::hex << hr << std::dec << std::endl;
			return -1;
		}

		std::vector<BYTE> imageData = CaptureScreenD3D();

		SaveDataAsBinary(imageData, "raw_screenshot.bin");

		if (!client.SendImage(imageData)) {
			printf("ͼ����ʧ��\n");
			break;
		}

		Sleep(1); //֡�ʿ���

		// ֡��ͳ��
		frameCount++;  // ����֡������
		DWORD currentTime = GetTickCount();

		// ÿ�����һ��֡��
		if (currentTime - lastReportTime >= 1000) {
			printf("��ǰ֡��: %lu FPS\n", frameCount);

			// ����ͳ��
			frameCount = 0;
			lastReportTime = currentTime;
		}
		//Sleep(10000);
	}

	client.Stop();
	return 0;
}
