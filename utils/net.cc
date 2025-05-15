#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <utils/net.h>

bool GetLocalIPAddress(std::string& ip_addr) {
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;
    void* tmpAddrPtr = nullptr;
    // 获取网络接口列表
    if (getifaddrs(&ifAddrStruct) == -1) {
        return false;
    }
    // 遍历网络接口链表
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // 如果是 IPv4 地址
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (ifa->ifa_name == std::string("lo")) {
                // 忽略本地主机 (localhost)
                continue;
            }
            ip_addr = addressBuffer;
            break; // 获取到一个非本地的 IPv4 地址后退出
        }
    }
    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct); // 释放内存
    }
    return ip_addr.empty() ? false : true;
}
