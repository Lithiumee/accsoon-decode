#include "accsoon_tcp_control.h"
#include "common.hpp"

int tcpInit();
int tcpSendTrigger(int sock);
void logSocketError(int sock, const char *msg);

const int TCP_PORT = 8001;
const int KEEP_ALIVE = 1;
const int REPLY_SIZE = 20;
const int TCP_DATA_SIZE = 19;
const int TCP_REPLY_SIZE = 20;
const int TCP_RECV_TIMEOUT_MS = 400;
const char TCP_DATA[TCP_DATA_SIZE] = {0x41, 0x43, 0x43, 0x53, 0x4f, 0x4f, 0x4e, 0x01, 0x00, 0x01,
                                      0x00, 0x0a, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00};
/**
 * @brief TCP线程
 *
 */
void tcpThread()
{
    LOG(LOG_INFO, "Starting TCP thread");

    int sock = tcpInit();
    if (sock < 0)
    {
        LOG(LOG_ERROR, "Failed to initialize TCP connection");
        return;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    while (!stop_threads.load())
    {
        // 发送开启图传的触发信号
        int triggerResult = tcpSendTrigger(sock);
        if (triggerResult < 0)
        {
            if (triggerResult == -2)
            {
                // Timeout occurred, retry sending the trigger
                continue;
            }
            // shutdown and close
            shutdown(sock, SHUT_RDWR);
            close(sock);
            sock = tcpInit();
            if (sock < 0)
            {
                LOG(LOG_ERROR, "Failed to reinitialize TCP connection");
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    // 关闭TCP连接
    close(sock);
    LOG(LOG_INFO, "TCP thread exit");
}
int tcpInit()
{
    // 初始化TCP连接
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        LOG(LOG_ERROR, "socket error: %s", strerror(errno));
        return -1;
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;       // 使用IPv4地址
    serv_addr.sin_port = htons(TCP_PORT); // 端口
    if (inet_pton(AF_INET, IP_ADDRESS, &serv_addr.sin_addr.s_addr) <= 0)
    {
        LOG(LOG_ERROR, "Invalid IP address: %s", IP_ADDRESS);
        close(sock);
        return -1;
    }
    // keep alive
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &KEEP_ALIVE, sizeof(KEEP_ALIVE)) < 0)
    {
        LOG(LOG_ERROR, "setsockopt SO_KEEPALIVE error: %s", strerror(errno));
        close(sock);
        return -1;
    }
    // set timeout
    struct timeval tv_out;
    tv_out.tv_sec = 0;
    tv_out.tv_usec = TCP_RECV_TIMEOUT_MS * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0)
    {
        LOG(LOG_ERROR, "setsockopt SO_RCVTIMEO error: %s", strerror(errno));
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        LOG(LOG_ERROR, "connect error: %s", strerror(errno));
        close(sock);
        return -1;
    }
    return sock;
}

/**
 * @brief 打印socket错误
 */
void logSocketError(int sock, const char *msg)
{
    int optval;
    socklen_t optlen = sizeof(optval);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &optval, &optlen) == 0)
    {
        LOG(LOG_ERROR, "%s: %d %s", msg, optval, strerror(optval));
    }
    else
    {
        LOG(LOG_ERROR, "getsockopt error: %s", strerror(errno));
    }
}

/**
 * @brief 通过TCP发送图传的KeepAlive信号
 *
 */
int tcpSendTrigger(int sock)
{

    int ret = send(sock, TCP_DATA, TCP_DATA_SIZE, 0);
    if (ret < 0)
    {
        logSocketError(sock, "tcp send error");
        // shutdown and close
        return -1;
    }

    // 接收回复
    char reply[TCP_REPLY_SIZE];
    ssize_t reply_length = recv(sock, reply, sizeof(reply), 0);
    if (reply_length < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG(LOG_ERROR, "tcp recv timeout");
            return -2;
        }
        else
        {
            logSocketError(sock, "tcp recv error");
            return -1;
        }
    }

    return 0;
}