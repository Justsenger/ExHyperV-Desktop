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
// 全局帧率统计变量
unsigned long frameCount = 0;
DWORD lastReportTime = GetTickCount();



//HvSock结构
struct SOCKADDR_HV
{
	ADDRESS_FAMILY Family;
	USHORT Reserved;
	GUID VmId;
	GUID ServiceId;
};

class Client {
public:
	Client() { ConnectSocket = INVALID_SOCKET; } //初始化函数

	//创建连接
	bool Start() {
		WSADATA wsaData;
		SOCKADDR_HV clientService;
		CLSID VmID, ServiceID;

		// 初始化GUID
		const wchar_t* clsid_str = L"{a42e7cda-d03f-480c-9cc2-a4de20abb878}"; // 请查阅文档，这个是所有都能接收
		CLSIDFromString(clsid_str, &VmID);
		clsid_str = L"{1f6be6bc-3e37-4d4a-97e3-46e7a5bdf739}";
		CLSIDFromString(clsid_str, &ServiceID); //服务GUID

		CONST GUID* vmId = &VmID;
		CONST GUID* serviceId = &ServiceID;

		// 初始化 Winsock
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
		{
			printf("WSAStartup failed: %d\n", iResult);
			return false;
		}

		struct addrinfo* result = NULL, * ptr = NULL, hints;

		//设置协议为Hvsocket
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
			printf("Socket创建失败：%d\n", WSAGetLastError());
			freeaddrinfo(result);
			WSACleanup();
			return false;
		}

		// 准备连接到服务器
		iResult = connect(ConnectSocket, hints.ai_addr, (int)hints.ai_addrlen);

		if (iResult == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET; //置于错误位
		}

		freeaddrinfo(result);

		if (ConnectSocket == INVALID_SOCKET)
		{
			printf("无法连接到服务器，错误代码: %d\n", WSAGetLastError());
			WSACleanup();
			return false;
		}

		return true;
	};

	//关闭连接
	void Stop() {
		int iResult = shutdown(ConnectSocket, SD_SEND);

		if (iResult == SOCKET_ERROR)
		{
			printf("关闭连接失败：%d\n", WSAGetLastError());
		}

		closesocket(ConnectSocket);
		WSACleanup();
		printf("关闭连接成功\n");
	};

	// 发送文字信息到服务器
	bool Send(char* szMsg)
	{

		int iResult = send(ConnectSocket, szMsg, strlen(szMsg), 0);

		if (iResult == SOCKET_ERROR)
		{
			printf("发送失败: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			Stop();
			return false;
		}

		return true;
	};

	// 从服务器接收信息
	bool Recv()
	{
		char recvbuf[DEFAULT_BUFFER_LENGTH];
		int iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFFER_LENGTH, 0); //收到的信息
		if (iResult > 0)
		{
			char msg[DEFAULT_BUFFER_LENGTH];
			memset(&msg, 0, sizeof(msg));
			strncpy_s(msg, recvbuf, iResult); //将收到的信息保存到缓冲区msg中
			printf("收到回复:%s\n", msg);
			return true;
		}
		return false;
	}

	bool SendImage(const std::vector<BYTE>& imageData) {
		// 发送图像数据
		int imageSize = imageData.size();
		int iResult = send(ConnectSocket, (char*)imageData.data(), imageSize, 0);
		if (iResult == SOCKET_ERROR) {
			printf("发送图像数据失败，错误代码：%d\n", WSAGetLastError());
			return false;
		}

		// 等待服务器确认图像已接收
		//char buffer[DEFAULT_BUFFER_LENGTH];
		//iResult = recv(ConnectSocket, buffer, sizeof(buffer) - 1, 0);  // 留出一个空间给空字符
		//if (iResult > 0) {
		//	buffer[iResult] = '\0';  // 确保字符串终止
		//	printf("收到来自服务器的确认信息：%s\n", buffer);
		//}
		//else {
		//	printf("没有收到服务器的确认信息。\n");
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

// 声明全局变量
IDXGIOutputDuplication* g_pDeskDupl = nullptr;
ID3D11Device* g_d3dDevice = nullptr;
ID3D11DeviceContext* g_d3dContext = nullptr;
IDXGIAdapter1* g_dxgiAdapter = nullptr;
IDXGIOutput* g_dxgiOutput = nullptr;



// 初始化 D3D 设备和桌面复制接口
HRESULT InitD3D() {
	if (g_d3dDevice && g_d3dContext) {
		return S_OK; // 如果设备和上下文已经创建，直接返回
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

	hr = g_dxgiAdapter->EnumOutputs(0, &g_dxgiOutput);  // 获取输出（显示器）
	if (FAILED(hr)) {
		return hr;
	}

	// 创建输出复制接口
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


// 捕获屏幕内容并返回二进制数据
std::vector<BYTE> CaptureScreenD3D() {
	if (!g_pDeskDupl) {
		std::cerr << "桌面复制接口未初始化!" << std::endl;
		return {};
	}

	// 捕获帧
	IDXGIResource* pDesktopResource = nullptr;
	DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
	HRESULT hr = g_pDeskDupl->AcquireNextFrame(0, &frameInfo, &pDesktopResource);



	if (FAILED(hr)) {
		//std::cerr << "无法获取下一帧! HRESULT: " << std::hex << hr << std::dec << std::endl;
		return {}; // 捕获帧失败
	}

	// 获取捕获帧的表面
	ID3D11Texture2D* pCapturedFrame = nullptr;
	hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pCapturedFrame);
	pDesktopResource->Release();
	if (FAILED(hr)) {
		std::cerr << "无法查询捕获的帧!" << std::endl;
		return {};
	}

	// 获取纹理的描述信息
	D3D11_TEXTURE2D_DESC desc0;
	pCapturedFrame->GetDesc(&desc0);

	// 输出捕获帧的详细信息
	//std::cout << "Captured Frame Texture Description:" << std::endl;
	//std::cout << "Width: " << desc0.Width << std::endl;
	//std::cout << "Height: " << desc0.Height << std::endl;
	//std::cout << "MipLevels: " << desc0.MipLevels << std::endl;
	//std::cout << "ArraySize: " << desc0.ArraySize << std::endl;
	//std::cout << "Format: " << desc0.Format << std::endl;  // 打印格式枚举值
	//std::cout << "Usage: " << desc0.Usage << std::endl;
	//std::cout << "BindFlags: " << desc0.BindFlags << std::endl;
	//std::cout << "CPUAccessFlags: " << desc0.CPUAccessFlags << std::endl;
	//std::cout << "MiscFlags: " << desc0.MiscFlags << std::endl;

	// 创建目标纹理，使用支持映射的格式（B8G8R8A8_UNORM）
	D3D11_TEXTURE2D_DESC desc = desc0;
	desc.Usage = D3D11_USAGE_STAGING;  // 设置为 D3D11_USAGE_STAGING，允许映射
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;  // 允许读取
	desc.BindFlags = 0;  // 不进行绑定
	desc.SampleDesc.Count = 1;  // 强制设置为单采样
	desc.SampleDesc.Quality = 0;  // 质量等级设为0
	desc.MiscFlags = 0;  // 设置为0，不使用其他功能
	//desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // 设置目标格式

	// 打印目标纹理的描述
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

	// 创建目标纹理
	ID3D11Texture2D* pConvertedTexture = nullptr;
	HRESULT hr2 = g_d3dDevice->CreateTexture2D(&desc, nullptr, &pConvertedTexture);
	if (FAILED(hr2)) {
		std::cerr << "创建目标纹理失败! HRESULT: " << std::hex << hr2 << std::dec << std::endl;
		pCapturedFrame->Release();
		return {};  // 创建目标纹理失败
	}

	// 复制捕获帧到目标纹理
	g_d3dContext->CopyResource(pConvertedTexture, pCapturedFrame);

	// 将纹理展开为数据以便读取
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	hr = g_d3dContext->Map(pConvertedTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(hr)) {
		std::cerr << "映射纹理失败! HRESULT: " << std::hex << hr << std::dec << std::endl;
		pCapturedFrame->Release();
		pConvertedTexture->Release();
		return {};  // 映射失败
	}

	// 获取数据并保存为二进制
	int width = static_cast<int>(desc.Width);
	int height = static_cast<int>(desc.Height);

	std::vector<BYTE> imageData;
	imageData.resize(width * height * 4); // 4 字节每像素（RGBA 格式）

	// 拷贝纹理数据
	memcpy(imageData.data(), mappedResource.pData, width * height * 4);

	g_d3dContext->Unmap(pConvertedTexture, 0);
	pCapturedFrame->Release();
	pConvertedTexture->Release();

	// 释放捕获的帧
	g_pDeskDupl->ReleaseFrame();

	return imageData;


}



void SaveDataAsBinary(const std::vector<BYTE>& data, const std::string& filename)
{
	std::ofstream outFile(filename, std::ios::binary);
	if (!outFile.is_open()) {
		// 打开文件失败，可根据需要添加错误处理
		return;
	}

	// 将整个 data 写入二进制文件
	outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
	outFile.close();
}


int main(int argc, CHAR* argv[])
{

	//Sleep(10000);
	std::string msg;
	Client client;
	if (!client.Start()) return 1;
	// 定义一个分隔符，假设我们使用 | 来分隔
	const char delimiter = '|';

	// 获取屏幕分辨率
	int width = GetSystemMetrics(SM_CXSCREEN);
	int height = GetSystemMetrics(SM_CYSCREEN);

	// 输出分辨率的调试信息
	printf("屏幕宽度: %d, 屏幕高度: %d\n", width, height);

	if (width == 0 || height == 0) {
		printf("错误：无法获取屏幕分辨率！\n");
		client.Stop();
		return 1;
	}

	// 计算图像大小：假设每个像素4字节（RGBA格式）
	int imageSize = width * height * 4;  // 4字节每像素（RGBA）

	// 输出图像大小的调试信息
	printf("计算出的图像大小: %d 字节\n", imageSize);

	// 使用字符串流将参数拼接成字符串
	std::stringstream ss;
	ss << width << delimiter << height << delimiter << imageSize << delimiter;

	// 获取拼接后的字符串
	std::string dataToSend = ss.str();

	// 将字符串转换为 C 风格字符串
	const char* cstr = dataToSend.c_str();

	// 发送所有数据
	if (!client.Send(const_cast<char*>(cstr))) {
		printf("发送图像参数失败\n");
		client.Stop();
		return 1;
	}
	else {
		printf("图像参数发送成功\n");
	}

	// 等待服务器确认协商成功
	if (!client.Recv()) {
		printf("协商失败，未收到服务器确认\n");
		client.Stop();
		return 1;
	}
	else {
		printf("服务器已经确认参数\n");
	}




	// 协商成功，开始发送图像数据
	while (true) {



		// 初始化 D3D 设备
		HRESULT hr = InitD3D();
		if (FAILED(hr)) {
			std::cerr << "D3D 初始化失败! HRESULT: " << std::hex << hr << std::dec << std::endl;
			return -1;
		}

		std::vector<BYTE> imageData = CaptureScreenD3D();

		SaveDataAsBinary(imageData, "raw_screenshot.bin");

		if (!client.SendImage(imageData)) {
			printf("图像发送失败\n");
			break;
		}

		Sleep(1); //帧率控制

		// 帧率统计
		frameCount++;  // 增加帧计数器
		DWORD currentTime = GetTickCount();

		// 每秒输出一次帧率
		if (currentTime - lastReportTime >= 1000) {
			printf("当前帧率: %lu FPS\n", frameCount);

			// 重置统计
			frameCount = 0;
			lastReportTime = currentTime;
		}
		//Sleep(10000);
	}

	client.Stop();
	return 0;
}
