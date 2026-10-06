#include "DirectP2P.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cctype>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <natupnp.h>
#include <comdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define closesocket close
#define INVALID_SOCKET -1
#define SOCKET int
#endif

namespace DirectP2P
{

static const char* ALPHABET = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

std::string EncodeRoomCode(const std::string& ipStr, uint16_t port)
{
    unsigned int b1 = 0, b2 = 0, b3 = 0, b4 = 0;
    if (sscanf(ipStr.c_str(), "%u.%u.%u.%u", &b1, &b2, &b3, &b4) != 4)
        return "";

    uint64_t val = ((uint64_t)(b1 & 0xFF) << 40) |
                   ((uint64_t)(b2 & 0xFF) << 32) |
                   ((uint64_t)(b3 & 0xFF) << 24) |
                   ((uint64_t)(b4 & 0xFF) << 16) |
                   ((uint64_t)port);

    char chars[11] = {0};
    for (int i = 9; i >= 0; i--) {
        chars[i] = ALPHABET[val % 32];
        val /= 32;
    }

    std::string res = "SL-";
    res.append(chars, 5);
    res += "-";
    res.append(chars + 5, 5);
    return res;
}

bool DecodeRoomCode(const std::string& input, std::string& outIp, uint16_t& outPort)
{
    if (input.empty()) return false;

    // Direct IP format check (e.g. 192.168.1.50 or 25.1.2.3:7820)
    if (input.find('.') != std::string::npos) {
        size_t colon = input.find(':');
        if (colon != std::string::npos) {
            outIp = input.substr(0, colon);
            outPort = (uint16_t)atoi(input.substr(colon + 1).c_str());
            if (outPort == 0) outPort = DEFAULT_PORT;
        } else {
            outIp = input;
            outPort = DEFAULT_PORT;
        }
        return true;
    }

    std::string clean;
    for (char c : input) {
        if (c == '-' || c == ' ' || c == '_' || c == ':') continue;
        clean += (char)toupper((unsigned char)c);
    }
    if (clean.rfind("SL", 0) == 0) {
        clean = clean.substr(2);
    }

    if (clean.length() != 10) return false;

    uint64_t val = 0;
    for (char c : clean) {
        const char* p = strchr(ALPHABET, c);
        if (!p) return false;
        val = val * 32 + (p - ALPHABET);
    }

    uint8_t b1 = (uint8_t)(val >> 40);
    uint8_t b2 = (uint8_t)(val >> 32);
    uint8_t b3 = (uint8_t)(val >> 24);
    uint8_t b4 = (uint8_t)(val >> 16);
    outPort = (uint16_t)(val & 0xFFFF);

    char ipBuf[64];
    snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", b1, b2, b3, b4);
    outIp = ipBuf;
    return true;
}

std::string GetLocalIP()
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) return "127.0.0.1";
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(80);
    target.sin_addr.s_addr = inet_addr("8.8.8.8");
    connect(s, (sockaddr*)&target, sizeof(target));
    sockaddr_in local{};
    int len = sizeof(local);
    getsockname(s, (sockaddr*)&local, &len);
    closesocket(s);
    char buf[64] = "127.0.0.1";
    inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

std::string GetPublicIP(int timeoutMs)
{
    const char* services[] = { "api.ipify.org", "icanhazip.com" };
    for (const char* host : services)
    {
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, "80", &hints, &res) != 0 || !res)
            continue;

        SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s == INVALID_SOCKET) {
            freeaddrinfo(res);
            continue;
        }

#ifdef _WIN32
        DWORD tv = (DWORD)timeoutMs;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif

        if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
            closesocket(s);
            freeaddrinfo(res);
            continue;
        }
        freeaddrinfo(res);

        char req[256];
        snprintf(req, sizeof(req), "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: melonDS\r\nConnection: close\r\n\r\n", host);
        send(s, req, (int)strlen(req), 0);

        char buf[1024];
        int n = recv(s, buf, sizeof(buf) - 1, 0);
        closesocket(s);
        if (n <= 0) continue;
        buf[n] = '\0';

        const char* body = strstr(buf, "\r\n\r\n");
        if (!body) continue;
        body += 4;

        std::string ip;
        while (*body && (*body == ' ' || *body == '\r' || *body == '\n')) body++;
        while (*body && (*body == '.' || (*body >= '0' && *body <= '9'))) {
            ip += *body++;
        }
        if (!ip.empty()) return ip;
    }
    return "";
}

bool UPnPOpenPort(uint16_t port, const std::string& localIp, std::string& outMsg)
{
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IUPnPNAT* nat = nullptr;
    hr = CoCreateInstance(CLSID_UPnPNAT, NULL, CLSCTX_INPROC_SERVER, IID_IUPnPNAT, (void**)&nat);
    if (FAILED(hr) || !nat) {
        outMsg = "Service UPnP Windows indisponible";
        if (SUCCEEDED(hr)) CoUninitialize();
        return false;
    }

    IStaticPortMappingCollection* col = nullptr;
    hr = nat->get_StaticPortMappingCollection(&col);
    if (FAILED(hr) || !col) {
        outMsg = "Aucun routeur UPnP détecté sur votre réseau local";
        nat->Release();
        CoUninitialize();
        return false;
    }

    BSTR bstrProto = SysAllocString(L"TCP");
    std::wstring wLocal(localIp.begin(), localIp.end());
    BSTR bstrClient = SysAllocString(wLocal.c_str());
    BSTR bstrDesc = SysAllocString(L"melonDS Soullocke Direct P2P");
    IStaticPortMapping* map = nullptr;

    hr = col->Add(port, bstrProto, port, bstrClient, VARIANT_TRUE, bstrDesc, &map);
    SysFreeString(bstrProto);
    SysFreeString(bstrClient);
    SysFreeString(bstrDesc);

    if (SUCCEEDED(hr)) {
        outMsg = "Port ouvert avec succès sur votre Box (UPnP) !";
        if (map) map->Release();
        col->Release();
        nat->Release();
        CoUninitialize();
        return true;
    } else {
        outMsg = "La Box a rejeté l'ouverture automatique de port (UPnP désactivé ?)";
        col->Release();
        nat->Release();
        CoUninitialize();
        return false;
    }
#else
    outMsg = "UPnP non disponible sur cette plateforme";
    return false;
#endif
}

void UPnPClosePort(uint16_t port)
{
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IUPnPNAT* nat = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_UPnPNAT, NULL, CLSCTX_INPROC_SERVER, IID_IUPnPNAT, (void**)&nat)) && nat) {
        IStaticPortMappingCollection* col = nullptr;
        if (SUCCEEDED(nat->get_StaticPortMappingCollection(&col)) && col) {
            BSTR bstrProto = SysAllocString(L"TCP");
            col->Remove(port, bstrProto);
            SysFreeString(bstrProto);
            col->Release();
        }
        nat->Release();
    }
    if (SUCCEEDED(hr)) CoUninitialize();
#endif
}

} // namespace DirectP2P
