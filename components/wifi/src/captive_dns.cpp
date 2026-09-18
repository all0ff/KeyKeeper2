#include "wifi/captive_dns.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include <cstring>

namespace wifi::captive_dns {

namespace {

constexpr char TAG[] = "wifi.captive_dns";
constexpr uint16_t DNS_PORT = 53;
constexpr size_t MAX_PACKET = 512; // generous for a single-question query; anything bigger is dropped
constexpr size_t DNS_HEADER_LEN = 12;
constexpr uint32_t ANSWER_TTL_SECONDS = 60; // short on purpose -- this isn't real DNS data worth caching

TaskHandle_t task_handle = nullptr;
int sock_fd = -1;
uint8_t ap_ip[4] = {0, 0, 0, 0};
volatile bool should_run = false;

/**
 * @brief Find the byte length of the QUESTION section starting at
 *        DNS_HEADER_LEN in a query packet -- the QNAME's label
 *        sequence (each a length byte + that many bytes, ending in a
 *        zero-length label) plus the 4-byte QTYPE+QCLASS that follows
 *        it.
 *
 * @return 0 if the packet is too short/malformed to contain a
 *         complete question (caller drops the packet).
 */
size_t question_length(const uint8_t* buf, size_t len)
{
    size_t pos = DNS_HEADER_LEN;
    while (true) {
        if (pos >= len) {
            return 0;
        }
        const uint8_t label_len = buf[pos];
        if (label_len == 0) {
            ++pos; // the terminating zero-length label itself
            break;
        }
        if ((label_len & 0xC0) != 0) {
            // A compression pointer in a QUESTION section would be
            // unusual for a real resolver's outgoing query -- not
            // worth supporting, just reject.
            return 0;
        }
        pos += 1 + label_len;
    }
    pos += 4; // QTYPE + QCLASS
    if (pos > len) {
        return 0;
    }
    return pos - DNS_HEADER_LEN;
}

void dns_task(void* /*arg*/)
{
    uint8_t buf[MAX_PACKET];
    uint8_t response[MAX_PACKET];

    ESP_LOGI(TAG, "Captive DNS started, answering every query with %u.%u.%u.%u", ap_ip[0], ap_ip[1], ap_ip[2],
             ap_ip[3]);

    while (should_run) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        const int received = recvfrom(sock_fd, buf, sizeof(buf), 0, reinterpret_cast<struct sockaddr*>(&client_addr),
                                       &addr_len);
        if (received < 0) {
            // Expected once when stop() closes the socket out from
            // under this recvfrom() -- should_run is already false by
            // then, so the loop exits on its own right after.
            continue;
        }
        if (static_cast<size_t>(received) < DNS_HEADER_LEN) {
            continue; // too short to even be a DNS header
        }

        const size_t q_len = question_length(buf, static_cast<size_t>(received));
        if (q_len == 0) {
            continue; // malformed/unsupported question -- silently drop, a real resolver will just retry or time out
        }

        // Header: copy the transaction ID (bytes 0-1) as-is.
        std::memcpy(response, buf, 2);
        // Flags: QR=1 (response), Opcode=0000, AA=1 (authoritative for
        // this made-up zone), TC=0, RD=copied from the query's own
        // bit -- see this file's own comment on the bit layout.
        response[2] = static_cast<uint8_t>(0x84 | (buf[2] & 0x01));
        response[3] = 0x00; // RA=0, Z=0, RCODE=0 (no error)
        response[4] = 0x00;
        response[5] = 0x01; // QDCOUNT=1 -- echoing the one question back
        response[6] = 0x00;
        response[7] = 0x01; // ANCOUNT=1 -- the one answer we're giving
        response[8] = 0x00;
        response[9] = 0x00; // NSCOUNT=0
        response[10] = 0x00;
        response[11] = 0x00; // ARCOUNT=0

        // Echo the question section verbatim.
        std::memcpy(response + DNS_HEADER_LEN, buf + DNS_HEADER_LEN, q_len);
        size_t pos = DNS_HEADER_LEN + q_len;

        if (pos + 16 > sizeof(response)) {
            continue; // shouldn't happen given MAX_PACKET headroom, but don't overrun if it somehow does
        }

        // Answer: NAME as a compression pointer back to the question's
        // own name at offset 12 (0xC0 0x0C), rather than repeating it.
        response[pos++] = 0xC0;
        response[pos++] = 0x0C;
        response[pos++] = 0x00;
        response[pos++] = 0x01; // TYPE=A
        response[pos++] = 0x00;
        response[pos++] = 0x01; // CLASS=IN
        response[pos++] = static_cast<uint8_t>((ANSWER_TTL_SECONDS >> 24) & 0xFF);
        response[pos++] = static_cast<uint8_t>((ANSWER_TTL_SECONDS >> 16) & 0xFF);
        response[pos++] = static_cast<uint8_t>((ANSWER_TTL_SECONDS >> 8) & 0xFF);
        response[pos++] = static_cast<uint8_t>(ANSWER_TTL_SECONDS & 0xFF);
        response[pos++] = 0x00;
        response[pos++] = 0x04; // RDLENGTH=4
        response[pos++] = ap_ip[0];
        response[pos++] = ap_ip[1];
        response[pos++] = ap_ip[2];
        response[pos++] = ap_ip[3];

        sendto(sock_fd, response, pos, 0, reinterpret_cast<struct sockaddr*>(&client_addr), addr_len);
    }

    ESP_LOGI(TAG, "Captive DNS task exiting");
    task_handle = nullptr;
    vTaskDelete(nullptr);
}

} // namespace

bool start(esp_netif_t* ap_netif)
{
    if (should_run) {
        stop();
    }

    if (ap_netif == nullptr) {
        ESP_LOGE(TAG, "start(): null netif");
        return false;
    }

    esp_netif_ip_info_t ip_info{};
    if (esp_netif_get_ip_info(ap_netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
        ESP_LOGE(TAG, "start(): AP netif has no IP yet");
        return false;
    }
    // esp_ip4_addr_t stores the address in network byte order already
    // -- these four bytes are the dotted-decimal octets in order.
    std::memcpy(ap_ip, &ip_info.ip.addr, 4);

    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        ESP_LOGE(TAG, "socket() failed");
        return false;
    }

    struct sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(DNS_PORT);
    if (bind(sock_fd, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) != 0) {
        ESP_LOGE(TAG, "bind() to UDP port %u failed", DNS_PORT);
        close(sock_fd);
        sock_fd = -1;
        return false;
    }

    should_run = true;
    const BaseType_t created = xTaskCreate(&dns_task, "captive_dns", 3072, nullptr, tskIDLE_PRIORITY + 2,
                                            &task_handle);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate() failed");
        should_run = false;
        close(sock_fd);
        sock_fd = -1;
        return false;
    }

    return true;
}

void stop()
{
    if (!should_run) {
        return;
    }
    should_run = false;
    if (sock_fd >= 0) {
        // Unblocks the task's own recvfrom() so it can see should_run
        // is now false and exit cleanly, instead of staying blocked
        // forever on a socket nothing will ever write to again.
        close(sock_fd);
        sock_fd = -1;
    }
    ESP_LOGI(TAG, "Captive DNS stopping");
}

bool is_running()
{
    return should_run;
}

} // namespace wifi::captive_dns
