#ifndef DIRECT_P2P_H
#define DIRECT_P2P_H

#include <string>
#include <cstdint>

namespace DirectP2P
{
    const uint16_t DEFAULT_PORT = 7820;

    // Room code encoding & decoding
    // Uses 32 clean alphanumeric chars (no 0/O, 1/I confusion).
    // Encodes 4 bytes IPv4 + 2 bytes port into a 10-char token: "SL-XXXXX-XXXXX"
    std::string EncodeRoomCode(const std::string& ipStr, uint16_t port);
    bool DecodeRoomCode(const std::string& input, std::string& outIp, uint16_t& outPort);

    // IP helpers
    std::string GetLocalIP();
    std::string GetPublicIP(int timeoutMs = 3500);

    // UPnP automatic port mapping
    bool UPnPOpenPort(uint16_t port, const std::string& localIp, std::string& outMsg);
    void UPnPClosePort(uint16_t port);
}

#endif // DIRECT_P2P_H
