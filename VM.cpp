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
	Client() {ConnectSocket = INVALID_SOCKET;} //初始化函数

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

		struct addrinfo* result = NULL,* ptr = NULL,hints;

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
		char buffer[DEFAULT_BUFFER_LENGTH];
		iResult = recv(ConnectSocket, buffer, sizeof(buffer) - 1, 0);  // 留出一个空间给空字符
		if (iResult > 0) {
			buffer[iResult] = '\0';  // 确保字符串终止
			printf("收到来自服务器的确认信息：%s\n", buffer);
		}
		else {
			printf("没有收到服务器的确认信息。\n");
		}
		return true;
	}

private:
	SOCKET ConnectSocket;
};


// 截取屏幕并返回图像数据
// 截取屏幕并保存图像数据
std::vector<BYTE> CaptureScreen() {
    // 获取屏幕的设备上下文
    HDC hdcScreen = GetDC(NULL);

    // 获取屏幕宽度和高度
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // 创建内存设备上下文
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // 创建与屏幕相同大小的位图
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);

    // 将屏幕内容复制到位图
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);

    // 获取位图的位图信息
    BITMAP bitmap;
    GetObject(hBitmap, sizeof(bitmap), &bitmap);

    // 获取位图数据并保存到缓冲区
    int imgSize = bitmap.bmWidth * bitmap.bmHeight * 4;  // 每个像素 4 字节 (RGBA)
    std::vector<BYTE> imageData(imgSize);

    // 创建颜色信息
    BITMAPINFOHEADER bmiHeader = { sizeof(BITMAPINFOHEADER), bitmap.bmWidth, bitmap.bmHeight, 1, 32 };
    BITMAPINFO bmi = { bmiHeader, { 0 } };

    // 获取图像数据
    GetDIBits(hdcMem, hBitmap, 0, bitmap.bmHeight, imageData.data(), &bmi, DIB_RGB_COLORS);

    // 保存图像为 BMP 文件
    std::ofstream outFile("debug.bmp", std::ios::binary);
    if (outFile) {
        // BMP文件头
        BITMAPFILEHEADER bfh;
        bfh.bfType = 0x4D42;  // "BM" 文件标识符
        bfh.bfReserved1 = 0;
        bfh.bfReserved2 = 0;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  // 位图数据偏移量
        bfh.bfSize = bfh.bfOffBits + imgSize;  // 文件大小

        // 写入文件头
        outFile.write((char*)&bfh, sizeof(BITMAPFILEHEADER));
        outFile.write((char*)&bmiHeader, sizeof(BITMAPINFOHEADER));

        // 写入位图数据
        outFile.write((char*)imageData.data(), imgSize);

        outFile.close();
        printf("图像已保存到文件：%s\n", "debug.bmp");
    } else {
        printf("保存图像失败！\n");
    }

    // 清理资源
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

	// 等待服务器确认协商成功
	if (!client.Recv()) {
		printf("协商失败，未收到服务器确认\n");
		client.Stop();
		return 1;
	}

	// 协商成功，开始发送图像数据
	while (true) {
		std::vector<BYTE> imageData = CaptureScreen();
		if (!client.SendImage(imageData)) {
			printf("图像发送失败\n");
			break;
		}
	}

	while (true) {
		std::vector<BYTE> imageData = CaptureScreen();
		if (!client.SendImage(imageData)) {
			printf("图像发送失败\n");
			break;
		}
	}
	client.Stop();
	return 0;
}
