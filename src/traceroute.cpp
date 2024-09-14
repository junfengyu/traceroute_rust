#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <vector>
#include <optional>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

constexpr int MAX_TTL = 30;
constexpr int TIMEOUT = 3; // seconds
constexpr int PACKET_SIZE = 64;

struct ICMPPacket {
    struct icmphdr header;
    char msg[PACKET_SIZE - sizeof(struct icmphdr)];
};

uint16_t compute_checksum(const void* buff, size_t length) {
    uint32_t sum = 0;
    const uint16_t* ptr = static_cast<const uint16_t*>(buff);

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length == 1) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

std::optional<std::string> resolve_hostname(const std::string& hostname) {
    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;

    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &res) != 0) {
        return std::nullopt;
    }

    char ip_str[INET_ADDRSTRLEN];
    auto* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);

    freeaddrinfo(res);
    return std::string(ip_str);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <destination>\n";
        return 1;
    }

    std::string destination = argv[1];
    std::optional<std::string> dest_ip_opt = resolve_hostname(destination);
    if (!dest_ip_opt) {
        std::cerr << "Could not resolve hostname: " << destination << "\n";
        return 1;
    }
    std::string dest_ip = dest_ip_opt.value();

    std::cout << "Traceroute to " << destination << " (" << dest_ip << ")\n";

    int send_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (send_sock < 0) {
        perror("Failed to create send socket");
        return 1;
    }

    int recv_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recv_sock < 0) {
        perror("Failed to create receive socket");
        close(send_sock);
        return 1;
    }

    timeval timeout{};
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    if (setsockopt(recv_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Failed to set socket options");
        close(send_sock);
        close(recv_sock);
        return 1;
    }

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, dest_ip.c_str(), &(dest_addr.sin_addr));

    for (int ttl = 1; ttl <= MAX_TTL; ++ttl) {
        if (setsockopt(send_sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
            perror("Failed to set TTL");
            continue;
        }

        ICMPPacket packet{};
        packet.header.type = ICMP_ECHO;
        packet.header.code = 0;
        packet.header.un.echo.id = getpid();
        packet.header.un.echo.sequence = ttl;
        memset(packet.msg, 0, sizeof(packet.msg));
        packet.header.checksum = 0;
        packet.header.checksum = compute_checksum(&packet, sizeof(packet));

        auto start_time = std::chrono::high_resolution_clock::now();
        ssize_t bytes_sent = sendto(send_sock, &packet, sizeof(packet), 0,
                                    reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr));
        if (bytes_sent < 0) {
            perror("Failed to send packet");
            continue;
        }

        sockaddr_in recv_addr{};
        socklen_t addr_len = sizeof(recv_addr);
        char recv_buffer[1024];

        ssize_t bytes_received = recvfrom(recv_sock, recv_buffer, sizeof(recv_buffer), 0,
                                          reinterpret_cast<sockaddr*>(&recv_addr), &addr_len);
        auto end_time = std::chrono::high_resolution_clock::now();

        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cout << ttl << "\t*\tRequest timed out\n";
            } else {
                perror("Failed to receive packet");
            }
            continue;
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(recv_addr.sin_addr), addr_str, sizeof(addr_str));

        struct iphdr* ip_hdr = reinterpret_cast<struct iphdr*>(recv_buffer);
        size_t ip_header_len = ip_hdr->ihl * 4;
        struct icmphdr* icmp_hdr = reinterpret_cast<struct icmphdr*>(recv_buffer + ip_header_len);

        if (icmp_hdr->type == ICMP_TIME_EXCEEDED) {
            std::cout << ttl << "\t" << addr_str << "\t" << duration.count() << "ms\n";
        } else if (icmp_hdr->type == ICMP_ECHOREPLY) {
            std::cout << ttl << "\t" << addr_str << "\t" << duration.count() << "ms\n";
            std::cout << "Destination reached.\n";
            break;
        } else {
            std::cout << ttl << "\t" << addr_str << "\t" << duration.count() << "ms (ICMP type " << static_cast<int>(icmp_hdr->type) << ")\n";
        }
    }

    close(send_sock);
    close(recv_sock);
    return 0;
}
