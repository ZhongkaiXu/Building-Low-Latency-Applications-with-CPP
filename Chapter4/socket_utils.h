#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <ifaddrs.h>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_set>

#include "macros.h"

#include "logging.h"

namespace Common {
struct SocketCfg {
    std::string ip_;
    std::string iface_;
    int port_ = -1;
    bool is_udp_ = false;
    bool is_listening_ = false;
    bool needs_so_timestamp_ = false;

    auto toString() const
    {
        std::stringstream ss;
        ss << "SocketCfg[ip:" << ip_
           << " iface:" << iface_
           << " port:" << port_
           << " is_udp:" << is_udp_
           << " is_listening:" << is_listening_
           << " needs_SO_timestamp:" << needs_so_timestamp_
           << "]";

        return ss.str();
    }
};

/// Represents the maximum number of pending / unaccepted TCP connections.
constexpr int MaxTCPServerBacklog = 1024;
constexpr int SocketConnectTimeoutMillis = 5000;

/// Convert interface name "eth0" to ip "123.123.123.123".
inline auto getIfaceIP(const std::string& iface) -> std::string
{
    char buf[NI_MAXHOST] = { '\0' };
    ifaddrs* ifaddr = nullptr;

    if (getifaddrs(&ifaddr) == -1)
        return { };

    for (ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name) {
            const auto rc = getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf),
                nullptr, 0, NI_NUMERICHOST);
            if (rc != 0)
                buf[0] = '\0';
            break;
        }
    }
    freeifaddrs(ifaddr);

    return buf;
}

/// Sockets will not block on read, but instead return immediately if data is not available.
inline auto setNonBlocking(int fd) -> bool
{
    const auto flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    if ((flags & O_NONBLOCK) != 0)
        return true;
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
}

/// Disable Nagle's algorithm and associated delays.
inline auto disableNagle(int fd) -> bool
{
    const int one = 1;
    return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != -1);
}

/// Allow software receive timestamps on incoming packets.
inline auto setSOTimestamp(int fd) -> bool
{
    const int one = 1;
    return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &one, sizeof(one)) != -1);
}

/// Select the local interface used to publish multicast packets.
inline auto setMcastInterface(int fd, const std::string& iface) -> bool
{
    const auto iface_ip = getIfaceIP(iface);
    in_addr local_addr { };
    return !iface_ip.empty() && inet_pton(AF_INET, iface_ip.c_str(), &local_addr) == 1 && setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &local_addr, sizeof(local_addr)) != -1;
}

/// Add / Join membership / subscription to the multicast stream specified and on the interface specified.
inline auto join(int fd, const std::string& ip, const std::string& iface) -> bool
{
    ip_mreq mreq { };
    if (inet_pton(AF_INET, ip.c_str(), &mreq.imr_multiaddr) != 1)
        return false;

    if (iface.empty()) {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    } else {
        const auto iface_ip = getIfaceIP(iface);
        if (iface_ip.empty() || inet_pton(AF_INET, iface_ip.c_str(), &mreq.imr_interface) != 1)
            return false;
    }

    return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
}

/// Wait for a non-blocking connect() to complete and verify its final status.
inline auto waitForConnect(int fd, int timeout_ms) -> bool
{
    // 要监听的fd以及要监听的状态
    // pollout表示可以写入数据，说明连接成功
    pollfd poll_fd { fd, POLLOUT, 0 };
    int poll_rc = -1;
    do {
        // poll：要检查的fd数组，数组个数，最多等待多久
        // 返回值：0表示超时，-1表示失败，>0表示有多少个fd状态发生了变化
        // errno代表poll失败的原因，EINTR表示被信号中断了，不算失败
        poll_rc = poll(&poll_fd, 1, timeout_ms);
    } while (poll_rc == -1 && errno == EINTR);

    if (poll_rc == 0) {
        // 等待超时
        errno = ETIMEDOUT;
        return false;
    }
    if (poll_rc == -1)
        // poll本身失败了
        return false;

    // poll返回>0，说明fd状态发生了变化，检查fd的状态，但不一定是连接成功了，还要检查SO_ERROR
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);
    // sol_socket表示socket通用选项
    // so error：我要查看这个fd的错误状态，返回值是0表示没有错误，非0表示有错误，errno设置为socket_error
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) == -1)
        return false;
    // 非0代表连接失败，errno设置为socket_error
    if (socket_error != 0) {
        errno = socket_error;
        return false;
    }
    return true;
}

/// Create a TCP / UDP socket to either connect to or listen for data on or listen for connections on the specified interface and IP:port information.
[[nodiscard]] inline auto createSocket(Logger& logger, const SocketCfg& socket_cfg) -> int
{
    std::string time_str;

    // 1. 校验端口，并在未指定 IP 时从网卡名获取本地 IPv4 地址。
    ASSERT(socket_cfg.port_ >= 0 && socket_cfg.port_ <= 65535,
        "Invalid socket port:" + std::to_string(socket_cfg.port_));

    auto ip = socket_cfg.ip_;
    if (ip.empty() && !socket_cfg.iface_.empty()) {
        ip = getIfaceIP(socket_cfg.iface_);
        ASSERT(!ip.empty(), "Could not resolve interface:" + socket_cfg.iface_);
    }

    // 2. 记录本次 socket 的完整配置。
    logger.log("%:% %() % cfg:%\n", __FILE__, __LINE__, __FUNCTION__,
        Common::getCurrentTimeStr(&time_str), socket_cfg.toString());

    // 3. 指定 IPv4、TCP/UDP，以及服务端或客户端模式。
    const int input_flags = (socket_cfg.is_listening_ ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);

    // hints描述了我需要一个什么样的网络地址
    addrinfo hints { };
    hints.ai_flags = input_flags; // 数字ip + 数字port
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = socket_cfg.is_udp_ ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = socket_cfg.is_udp_ ? IPPROTO_UDP : IPPROTO_TCP;

    // 4. 把 IP 和端口转换成 socket API 可用的地址结构；result_guard 负责自动释放结果。
    addrinfo* result = nullptr;
    const auto port = std::to_string(socket_cfg.port_);
    // 把我们的 ip + port 转换成 socket API 可用的结构
    const auto rc = getaddrinfo(ip.empty() ? nullptr : ip.c_str(), port.c_str(), &hints, &result);
    ASSERT(rc == 0, "getaddrinfo() failed. error:" + std::string(gai_strerror(rc)));

    // unique_ptr<T> 某个类型的指针，离开作用域自动delete，不用手动delete（跑到一半return，来不及delete，unique_ptr帮你delete）
    // unique_ptr<T, Deleter> 第二个参数是删除器，离开作用域自动调用删除器
    // addrinfo系统规定要使用freeaddrinfo释放result（因为它是链表，不能直接delete）
    // 这里需要指定删除器的类型，也就是freeaddrinfo这个函数指针的类型，直接用decltype得出
    // freeaddrinfo的函数指针类型：void (*)(struct addrinfo*)
    const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> result_guard(result, freeaddrinfo);

    int socket_fd = -1;
    int last_error = 0;
    const int one = 1;

    // 返回了一堆可能可以connect成功的地址，逐个尝试，直到成功为止
    for (addrinfo* rp = result; rp; rp = rp->ai_next) {
        // 用这个addrinfo去创建一个socket来尝试
        int candidate_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (candidate_fd == -1) {
            // 记录当前失败的原因
            // errno是Linux系统调用失败的错误码，无需手动设置，系统调用失败时会自动设置
            last_error = errno;
            continue;
        }

        // 当前候选失败时保存错误、关闭 fd，再尝试下一个地址。
        auto reject_candidate = [&]() {
            last_error = errno;
            close(candidate_fd);
            candidate_fd = -1;
        };

        // 6. 设置非阻塞模式，避免网络 I/O 长时间阻塞线程。
        // 也就是说不会等待数据到来，而是立即返回，返回值是-1，errno是EAGAIN或者EWOULDBLOCK
        if (!setNonBlocking(candidate_fd)) {
            reject_candidate();
            continue;
        }

        // 7. TCP 关闭 Nagle；UDP/组播指定发送网卡。
        if (!socket_cfg.is_udp_) {
            if (!disableNagle(candidate_fd)) {
                reject_candidate();
                continue;
            }
        } else if (!socket_cfg.iface_.empty() && !setMcastInterface(candidate_fd, socket_cfg.iface_)) {
            // tcp不需要指定网卡，udp/组播需要指定网卡
            reject_candidate();
            continue;
        }

        // 8. 客户端连接远端；非阻塞连接需等待并确认最终结果。
        if (!socket_cfg.is_listening_) {
            // 使用fd去connect远端的ip+port，connect是非阻塞的，返回-1并且errno是EINPROGRESS表示连接开始但没有完成
            const auto connect_rc = connect(candidate_fd, rp->ai_addr, rp->ai_addrlen);
            if (connect_rc == -1 && (errno != EINPROGRESS || !waitForConnect(candidate_fd, SocketConnectTimeoutMillis))) {
                reject_candidate();
                continue;
            }
        }

        // 9. 服务端允许地址复用，然后绑定本地地址和端口。
        if (socket_cfg.is_listening_) {
            if (setsockopt(candidate_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
                reject_candidate();
                continue;
            }

            const sockaddr_in addr { AF_INET, htons(socket_cfg.port_), { htonl(INADDR_ANY) }, { } };
            const auto* bind_addr = socket_cfg.is_udp_ ? reinterpret_cast<const sockaddr*>(&addr) : rp->ai_addr;
            const auto bind_addr_len = socket_cfg.is_udp_ ? sizeof(addr) : rp->ai_addrlen;
            if (bind(candidate_fd, bind_addr, bind_addr_len) == -1) {
                reject_candidate();
                continue;
            }
        }

        // 10. TCP 服务端开始监听连接；UDP 不需要 listen()。
        if (!socket_cfg.is_udp_ && socket_cfg.is_listening_) {
            if (listen(candidate_fd, MaxTCPServerBacklog) == -1) {
                reject_candidate();
                continue;
            }
        }

        // 11. 按需开启内核接收时间戳，用于测量网络延迟。
        if (socket_cfg.needs_so_timestamp_) {
            if (!setSOTimestamp(candidate_fd)) {
                reject_candidate();
                continue;
            }
        }

        // 当前候选完成全部初始化，保存 fd 并停止尝试。
        socket_fd = candidate_fd;
        break;
    }

    // 所有候选均失败则终止；否则把可用 fd 交给上层对象管理。
    ASSERT(socket_fd != -1, "createSocket() failed. errno:" + std::string(strerror(last_error)));
    return socket_fd;
}
}
