#define WIN32_LEAN_AND_MEAN
#define HV_PROTOCOL_RAW 1

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <combaseapi.h>
#include <vector>
#pragma comment(lib, "Ws2_32.lib")
#include <fstream>
#include <Windows.h>
#include <sstream>

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT: {
        // 获取绘制上下文 (HDC)
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);  // 获取当前窗口的 HDC
        EndPaint(hwnd, &ps);
    }
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wp, lp);  // 默认处理其他消息
    }
}


struct SOCKADDR_HV
{
    ADDRESS_FAMILY Family;
    USHORT Reserved;
    GUID VmId;
    GUID ServiceId;
};

DEFINE_GUID(HV_GUID_PARENT,
    0xa42e7cda, 0xd03f, 0x480c, 0x9c, 0xc2, 0xa, 0x4, 0xde, 0x20, 0xab, 0xb8, 0x78);
DEFINE_GUID(HV_GUID_ZERO,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0);


//显示图像
void DisplayImage(HDC hdc, const std::vector<BYTE>& imageData, int imageWidth, int imageHeight, int windowWidth, int windowHeight) {
    BITMAPINFOHEADER bmiHeader = { sizeof(BITMAPINFOHEADER), imageWidth, imageHeight, 1, 32 };
    BITMAPINFO bmi = { bmiHeader, { 0 } };
    SetStretchBltMode(hdc, HALFTONE);  // 设置为 HALFTONE 模式以获得更好的缩放效果
    StretchDIBits(hdc, 0, 0, windowWidth, windowHeight, 0, 0, imageWidth, imageHeight,imageData.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

//设置接收缓冲区大小
void SetSocketBufferSize(SOCKET ClientSocket, int bufferSize) {
    setsockopt(ClientSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&bufferSize, sizeof(bufferSize));
}

//检查接收缓冲区大小
void PrintSocketBufferSize(SOCKET ClientSocket) {
    int bufferSize = 0;
    int sizeLen = sizeof(bufferSize);
    if (getsockopt(ClientSocket, SOL_SOCKET, SO_RCVBUF, (char*)&bufferSize, &sizeLen) == SOCKET_ERROR) {
        printf("获取缓冲区大小失败，错误代码：%d\n", WSAGetLastError());
    }
    else {
        printf("当前接收缓冲区大小: %d bytes\n", bufferSize);
    }
}

std::vector<BYTE> ReceiveImage(SOCKET ClientSocket, int imageSize) {
    SetSocketBufferSize(ClientSocket,imageSize); //设置缓冲区大小
    std::vector<BYTE> imageData(imageSize);
    int received = recv(ClientSocket, (char*)imageData.data(), imageSize, MSG_WAITALL);
    if (received != imageSize) {
        printf("接收失败，错误代码：%d\n", WSAGetLastError());
        return {};
    }
    return imageData;
}

SOCKET Initial() {

    WSADATA wsaData;
    SOCKADDR_HV clientService;

    // 指定注册服务的GUID
    CLSID ServiceID;
    CLSIDFromString(L"{1f6be6bc-3e37-4d4a-97e3-46e7a5bdf739}", &ServiceID);

    // 初始化Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("Winsock初始化失败：%d\n", iResult);
        return 1;
    }
    struct addrinfo* result = NULL, hints;
    CONST GUID* serviceId = &ServiceID;

    ZeroMemory(&clientService, sizeof(clientService));
    clientService.Family = AF_HYPERV;
    clientService.VmId = HV_GUID_ZERO;
    clientService.ServiceId = *serviceId;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_HYPERV;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = HV_PROTOCOL_RAW;
    hints.ai_addrlen = sizeof(SOCKADDR_HV);
    hints.ai_addr = reinterpret_cast<SOCKADDR*>(&clientService);

    SOCKET ListenSocket = INVALID_SOCKET;

    // 创建接口以监听客户端连接
    ListenSocket = socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        printf("接口创建失败，错误代码：%d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // 绑定侦听接口
    iResult = bind(ListenSocket, hints.ai_addr, (int)hints.ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        printf("绑定失败，错误代码: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // 开启侦听接口
    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("侦听失败，错误代码: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    return ListenSocket;
}

bool Getset(SOCKET ClientSocket, int& width, int& height, int& imageSize) {
    // 定义分隔符
    const char delimiter = '|';
    // 接收数据
    char buffer[1024]; // 假设最大长度为1024
    int iResult = recv(ClientSocket, buffer, sizeof(buffer) - 1, 0);
    if (iResult == SOCKET_ERROR) {
        printf("接收数据失败: %d\n", WSAGetLastError());
        return false;
    }

    // 确保数据以空字符结束
    buffer[iResult] = '\0';

    // 解析数据
    std::string receivedData(buffer);
    std::stringstream ss(receivedData);

    // 解析宽度
    std::string token;
    std::getline(ss, token, delimiter);
    width = std::stoi(token);  // 转为整数

    // 解析高度
    std::getline(ss, token, delimiter);
    height = std::stoi(token);  // 转为整数

    // 解析图像大小
    std::getline(ss, token, delimiter);
    imageSize = std::stoi(token);  // 转为整数

    printf("接收到协商信息：宽度 = %d, 高度 = %d, 图像大小 = %d\n", width, height, imageSize);

    // 发送确认信息给客户端
    const char* confirmMsg = "协商成功，开始接收图像数据";
    iResult = send(ClientSocket, confirmMsg, strlen(confirmMsg), 0);
    if (iResult == SOCKET_ERROR) {
        printf("发送确认信息失败: %d\n", WSAGetLastError());
        return false;
    }
}

void Show(SOCKET ClientSocket, int& width, int& height, int& imageSize) {
    // 缩小后的窗口尺寸
    int windowWidth = static_cast<int>(width * 0.8);  // 计算0.8倍的宽度
    int windowHeight = static_cast<int>(height * 0.8); // 计算0.8倍的高度

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;     // 窗口过程
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ServerWindowClass";
    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindowEx(
        0, L"ServerWindowClass", L"Server Screen", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,   // 调整为适当的窗口大小
        NULL, NULL, wc.hInstance, NULL);
    HDC hdc = GetDC(hwnd);

    // 显示窗口
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);


    int imageCount = 0; // 变量，用于跟踪已接收的完整图像数量


    MSG msg;


    while (true) {


        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }


        std::vector<BYTE> imageData = ReceiveImage(ClientSocket, imageSize);
        if (imageData.empty()) {
            printf("接收图像失败或客户端断开连接\n");
            break; // 如果接收失败或连接关闭，退出
        }



        // 显示接收到的图像
        //printf("显示接收到的图像\n");


        // 获取客户区的宽度和高度
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // 获取客户区的宽度和高度
        int clientWidth = clientRect.right - clientRect.left;
        int clientHeight = clientRect.bottom - clientRect.top;

        InvalidateRect(hwnd, NULL, TRUE);  // 强制刷新窗口
        // 调用 DisplayImage 函数，传入客户区的宽高
        DisplayImage(hdc, imageData, width, height, clientWidth, clientHeight);

        //// 发送确认消息给客户端，表示已经收到图像数据
        //char confirmMessage[100];
        //imageCount++;
        //snprintf(confirmMessage, sizeof(confirmMessage), "已接收的完整图像数：%d", imageCount);
        //send(ClientSocket, confirmMessage, strlen(confirmMessage), 0);
        //g_imageData = imageData;

    }

}

int main() {

    SOCKET ClientSocket = INVALID_SOCKET,ListenSocket = Initial(); //初始化监听接口

    while (true) { 
        printf("等待客户端连接...\n");
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET)
        {
            printf("连接失败，错误代码：%d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }else{
            printf("客户端连接成功!\n");
        }

        int width, height, imageSize;
        Getset(ClientSocket, width, height, imageSize); //协商参数
        Show(ClientSocket, width, height, imageSize); //显示窗口
        closesocket(ClientSocket);
    }
    closesocket(ListenSocket);
    WSACleanup();
    return 0;
}
