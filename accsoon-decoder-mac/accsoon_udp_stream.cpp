#include "accsoon_udp_stream.h"
#include "common.hpp"
#include <map>
#include <vector>

#define FRAC_HEADER_SIZE 14
#define MSG_HEADER_SIZE 23
const int UDP_PORT = 8000;
const int UDP_RECV_TIMEOUT_MS = 100;
const int RX_BUFFER_SIZE = 1048576;
const int TX_BUFFER_SIZE = 524288;

char udp_ack[12] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

struct Message
{
    uint32_t frac_cnt;
    std::map<uint32_t, std::vector<uint8_t>> frags; // Mapping from frac_id to frac data
};

std::map<uint32_t, Message> messages; // Mapping from msg_id to message data

void udpReceive(int sock);
uint32_t extractFrameInfo(char *data);
void udpAck(int sock, struct sockaddr_in *cliaddr, uint16_t msg_id);

/**
 * @brief UDP线程
 */
void udpThread()
{
    LOG(LOG_INFO, "Starting UDP thread");

    // 初始化UDP连接
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        LOG(LOG_ERROR, "Socket creation failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // 设置接收缓冲区大小
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &RX_BUFFER_SIZE, sizeof(RX_BUFFER_SIZE)) < 0)
    {
        LOG(LOG_ERROR, "Failed to set receive buffer size: %s", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 设置发送缓冲区大小
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &TX_BUFFER_SIZE, sizeof(TX_BUFFER_SIZE)) < 0)
    {
        LOG(LOG_ERROR, "Failed to set send buffer size: %s", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UDP_PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        LOG(LOG_ERROR, "Bind failed: %s", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // set timeout
    struct timeval tv_out;
    tv_out.tv_sec = 0;
    tv_out.tv_usec = UDP_RECV_TIMEOUT_MS * 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0)
    {
        LOG(LOG_ERROR, "Failed to set socket timeout: %s", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 接收图传数据
    udpReceive(sockfd);
}

/**
 * @brief 接收UDP数据
 *
 * @param sock
 */
void udpReceive(int sock)
{
    LOG(LOG_INFO, "Receiving UDP data");

    socklen_t addr_len;         // 客户端地址长度
    struct sockaddr_in cliaddr; // 客户端地址

    // ack地址
    struct sockaddr_in ackaddr; // ack地址
    memset(&ackaddr, 0, sizeof(ackaddr));
    ackaddr.sin_family = AF_INET;                             // 使用IPv4地址
    inet_pton(AF_INET, IP_ADDRESS, &ackaddr.sin_addr.s_addr); // IP地址
    ackaddr.sin_port = htons(UDP_PORT);                       // 端口

    // 最新的ID
    uint16_t decoded_msg_id = 0;
    uint32_t used_pointer = 0;
    uint32_t latest_pointer = 0;

    // 收到包中的ID
    uint16_t msg_id = 0;
    uint8_t frac_cnt = 0;
    uint8_t frac_id = 0;
    uint8_t nalu_type = 0;

    // 接收缓冲区
    char data[8192];
    char *frame = (char *)SHM_ADDR;

    uint32_t first_flag = 1;

    try
    {
        while (!stop_threads.load())
        {
            ssize_t data_length = recvfrom(sock, data, sizeof(data), 0,
                                           (struct sockaddr *)&cliaddr, &addr_len);
            if (data_length < 0)
            {
                LOG(LOG_ERROR, "recvfrom error: %s", strerror(errno));
                continue;
            }
            if (data_length < FRAC_HEADER_SIZE)
            {
                LOG(LOG_ERROR, "data length is too short");
                continue;
            }

            msg_id = *(uint16_t *)(data + 8); // 传输时候是小端，低字节在前
            frac_cnt = *(uint8_t *)(data + 10);
            frac_id = *(uint8_t *)(data + 11);

            Message &message = messages[msg_id];
            if (message.frac_cnt != frac_cnt)
            {
                message.frac_cnt = frac_cnt;
            }
            size_t fragment_size = data_length - (FRAC_HEADER_SIZE);
            if (fragment_size > message.frags.max_size())
            {
                LOG(LOG_ERROR, "fragment size %zu exceeds max allowable size %zu", fragment_size, message.frags.max_size());
                continue;
            }
            message.frags[frac_id] = std::vector<uint8_t>(data + FRAC_HEADER_SIZE, data + data_length);
            if (message.frags.size() == message.frac_cnt)
            {
                if (decoded_msg_id > msg_id)
                {
                    if (decoded_msg_id - msg_id < 30)
                    {
                        // future msg_id is already decoded, and seems not an overflow
                        //  will erase the old message
                        messages.erase(msg_id);
                        LOG(LOG_ERROR, "erase old full message: %d current msg_id: %d", msg_id, decoded_msg_id);
                        continue;
                    }
                }
                if (first_flag)
                {
                    if (message.frags[0].size() > MSG_HEADER_SIZE + 4)
                    {
                        nalu_type = message.frags[0][MSG_HEADER_SIZE + 4];
                    }
                    else
                    {
                        LOG(LOG_ERROR, "Index out of range for message.frags[0]");
                        continue;
                    }
                    if (nalu_type != 0x67)
                    {
                        LOG(LOG_INFO, "Waiting for first frame %x", nalu_type);
                        continue;
                    }
                    else
                    {
                        LOG(LOG_INFO, "Received first frame %x", nalu_type);
                        first_flag = 0;
                    }
                }

                decoded_msg_id = msg_id;
                uint32_t len = extractFrameInfo((char *)(message.frags[0].data()));
                // reconstruct the frame
                latest_pointer = 0;
                memcpy(frame, &len, 4);
                latest_pointer = 4;
                memcpy(frame + latest_pointer, message.frags[0].data() + MSG_HEADER_SIZE, message.frags[0].size() - MSG_HEADER_SIZE);
                latest_pointer += message.frags[0].size() - MSG_HEADER_SIZE;
                for (uint16_t i = 1; i < frac_cnt; i++)
                {
                    memcpy(frame + latest_pointer, message.frags[i].data(), message.frags[i].size());
                    latest_pointer += message.frags[i].size();
                }
                // prepare new buffer
                if (frame == (char *)SHM_ADDR)
                    frame = (char *)SHM_ADDR + SHM_SIZE / 2;
                else
                    frame = (char *)SHM_ADDR;
                used_pointer = latest_pointer;
                // 信号量通知解码
                frame_sem.post();
                messages.erase(msg_id);
                // remove too old message
                for (auto it = messages.begin(); it != messages.end();)
                {
                    if ((decoded_msg_id > it->first) && (decoded_msg_id - it->first > 30))
                    {
                        LOG(LOG_ERROR, "erase old message: %d current msg_id: %d", it->first, decoded_msg_id);
                        it = messages.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            else
            {
                continue;
            }

            // 应答
            //  udpAck(sock, &ackaddr, msg_id);
            //  udpAck(sock, &ackaddr, msg_id);
            //  spdlog::debug("client addr: {} port: {}",
            //  inet_ntoa(ackaddr.sin_addr), ntohs(ackaddr.sin_port));
            //  if(os_gettime_ns() - last_time > 1000000000)//1s
            //  {
            //      // udpAck(sock, &ackaddr, msg_id);
            //      last_time = os_gettime_ns();
            //      LOG(LOG_INFO,"UDP ");
            //  }
        }
    }
    catch (const std::exception &e)
    {
        LOG(LOG_ERROR, "exception: %s", e.what());
    }
    close(sock);
    LOG(LOG_INFO, "UDP thread exit");
}

void udpAck(int sock, struct sockaddr_in *cliaddr, uint16_t msg_id)
{
    udp_ack[10] = msg_id & 0xff;
    udp_ack[11] = msg_id >> 8;
    sendto(sock, udp_ack, 12, 0, (struct sockaddr *)cliaddr,
           sizeof(struct sockaddr_in));
}

uint32_t extractFrameInfo(char *data)
{
    // uint16_t width = *(uint16_t *)data;//传输时候是小端，低字节在前
    // uint16_t height = *(uint16_t *)(data + 2);
    // uint32_t bitrate = *(uint32_t *)(data + 4);
    // uint8_t frame_rate = *(uint8_t *)(data + 8);
    // uint8_t sta_cnt = *(uint8_t *)(data + 9);
    // uint32_t trans_q = *(uint32_t *)(data + 10);
    // uint32_t id = *(uint32_t *)(data + 14);
    // uint8_t type = *(uint8_t *)(data + 18);
    uint32_t payload_len = *(uint32_t *)(data + 19);
    // uint8_t nalu_type = *(uint8_t *)(data + 23 +4);
    // spdlog::debug("w:{}, h:{}, fps:{}, nalu:{:02X}",
    // width, height, frame_rate, nalu_type);

    return payload_len;
}
