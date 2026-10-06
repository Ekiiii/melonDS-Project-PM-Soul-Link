#include "DirectP2P.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cctype>

#ifdef _WIN32
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
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
    inet_pton(AF_INET, "8.8.8.8", &target.sin_addr);
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

static std::string sLastRouterHost;
static uint16_t sLastRouterPort = 0;
static std::string sLastControlPath;
static std::string sLastServiceType;

static bool DirectSSDPDiscover(const std::string& localIp, std::string& outLocation)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in local{};
    local.sin_family = AF_INET;
    inet_pton(AF_INET, localIp.c_str(), &local.sin_addr);
    local.sin_port = 0;
    bind(s, (sockaddr*)&local, sizeof(local));

    DWORD tv = 1500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    sockaddr_in mcast{};
    mcast.sin_family = AF_INET;
    mcast.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &mcast.sin_addr);

    const char* req =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";

    sendto(s, req, (int)strlen(req), 0, (sockaddr*)&mcast, sizeof(mcast));

    char buf[4096];
    sockaddr_in from{};
    int fromLen = sizeof(from);
    int n = recvfrom(s, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromLen);
    closesocket(s);

    if (n <= 0) return false;
    buf[n] = '\0';

    const char* loc = nullptr;
    const char* candidates[] = {"LOCATION:", "Location:", "location:", nullptr};
    for (int i = 0; candidates[i]; i++) {
        loc = strstr(buf, candidates[i]);
        if (loc) { loc += strlen(candidates[i]); break; }
    }
    if (!loc) return false;

    while (*loc == ' ' || *loc == '\t') loc++;
    const char* end = strpbrk(loc, "\r\n");
    if (end) {
        outLocation.assign(loc, end - loc);
    } else {
        outLocation = loc;
    }
    return !outLocation.empty();
}

static bool ParseHttpUrl(const std::string& url, std::string& host, uint16_t& port, std::string& path)
{
    if (url.rfind("http://", 0) != 0) return false;
    std::string rem = url.substr(7);
    size_t slash = rem.find('/');
    std::string hostPort = (slash == std::string::npos) ? rem : rem.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : rem.substr(slash);

    size_t colon = hostPort.find(':');
    if (colon == std::string::npos) {
        host = hostPort;
        port = 80;
    } else {
        host = hostPort.substr(0, colon);
        port = (uint16_t)atoi(hostPort.substr(colon + 1).c_str());
    }
    return !host.empty() && port > 0;
}

static std::string SimpleHttpGet(const std::string& host, uint16_t port, const std::string& path, int timeoutMs = 2500)
{
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16]; snprintf(portStr, sizeof(portStr), "%u", port);
    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) return "";

    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); return ""; }

    DWORD tv = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        closesocket(s);
        freeaddrinfo(res);
        return "";
    }
    freeaddrinfo(res);

    char req[512];
    snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s:%u\r\nUser-Agent: melonDS\r\nConnection: close\r\n\r\n", path.c_str(), host.c_str(), port);
    send(s, req, (int)strlen(req), 0);

    std::string resp;
    char buf[4096];
    int n;
    while ((n = recv(s, buf, sizeof(buf), 0)) > 0) {
        resp.append(buf, n);
    }
    closesocket(s);
    return resp;
}

static bool SimpleHttpSoap(const std::string& host, uint16_t port, const std::string& path, const std::string& action, const std::string& body, int timeoutMs = 2500)
{
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16]; snprintf(portStr, sizeof(portStr), "%u", port);
    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) return false;

    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); return false; }

    DWORD tv = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        closesocket(s);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    char hdr[1024];
    snprintf(hdr, sizeof(hdr),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "User-Agent: melonDS\r\n"
        "Content-Type: text/xml; charset=\"utf-8\"\r\n"
        "SOAPAction: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        path.c_str(), host.c_str(), port, action.c_str(), (int)body.size());

    send(s, hdr, (int)strlen(hdr), 0);
    send(s, body.c_str(), (int)body.size(), 0);

    char buf[1024];
    int n = recv(s, buf, sizeof(buf) - 1, 0);
    closesocket(s);

    if (n > 0) {
        buf[n] = '\0';
        return (strstr(buf, "200 OK") != nullptr);
    }
    return false;
}

static bool DirectSSDPUPnPOpen(uint16_t port, const std::string& localIp, std::string& outMsg)
{
    std::string location;
    if (!DirectSSDPDiscover(localIp, location)) return false;

    std::string host, path;
    uint16_t rport = 80;
    if (!ParseHttpUrl(location, host, rport, path)) return false;

    std::string xml = SimpleHttpGet(host, rport, path);
    if (xml.empty()) return false;

    std::string svcType;
    size_t posSvc = xml.find("urn:schemas-upnp-org:service:WANIPConnection:");
    if (posSvc == std::string::npos) {
        posSvc = xml.find("urn:schemas-upnp-org:service:WANPPPConnection:");
    }
    if (posSvc == std::string::npos) return false;

    size_t endSvc = xml.find('<', posSvc);
    if (endSvc == std::string::npos) return false;
    svcType = xml.substr(posSvc, endSvc - posSvc);

    size_t posCtrl = xml.find("<controlURL>", posSvc);
    if (posCtrl == std::string::npos) return false;
    posCtrl += 12;
    size_t endCtrl = xml.find("</controlURL>", posCtrl);
    if (endCtrl == std::string::npos) return false;
    std::string ctrlPath = xml.substr(posCtrl, endCtrl - posCtrl);
    while (!ctrlPath.empty() && (ctrlPath.front() == ' ' || ctrlPath.front() == '\t' || ctrlPath.front() == '\r' || ctrlPath.front() == '\n')) {
        ctrlPath.erase(ctrlPath.begin());
    }
    while (!ctrlPath.empty() && (ctrlPath.back() == ' ' || ctrlPath.back() == '\t' || ctrlPath.back() == '\r' || ctrlPath.back() == '\n')) {
        ctrlPath.pop_back();
    }

    std::string ctrlHost = host;
    uint16_t ctrlPort = rport;
    if (ctrlPath.rfind("http://", 0) == 0) {
        ParseHttpUrl(ctrlPath, ctrlHost, ctrlPort, ctrlPath);
    } else if (!ctrlPath.empty() && ctrlPath[0] != '/') {
        ctrlPath = "/" + ctrlPath;
    }

    char soap[1024];
    snprintf(soap, sizeof(soap),
        "<?xml version=\"1.0\"?>\r\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
        "<s:Body>\r\n"
        "<u:AddPortMapping xmlns:u=\"%s\">\r\n"
        "  <NewRemoteHost></NewRemoteHost>\r\n"
        "  <NewExternalPort>%u</NewExternalPort>\r\n"
        "  <NewProtocol>TCP</NewProtocol>\r\n"
        "  <NewInternalPort>%u</NewInternalPort>\r\n"
        "  <NewInternalClient>%s</NewInternalClient>\r\n"
        "  <NewEnabled>1</NewEnabled>\r\n"
        "  <NewPortMappingDescription>melonDS Soul Link Direct P2P</NewPortMappingDescription>\r\n"
        "  <NewLeaseDuration>0</NewLeaseDuration>\r\n"
        "</u:AddPortMapping>\r\n"
        "</s:Body>\r\n"
        "</s:Envelope>",
        svcType.c_str(), port, port, localIp.c_str());

    std::string soapAction = "\"" + svcType + "#AddPortMapping\"";
    if (SimpleHttpSoap(ctrlHost, ctrlPort, ctrlPath, soapAction, soap)) {
        sLastRouterHost = ctrlHost;
        sLastRouterPort = ctrlPort;
        sLastControlPath = ctrlPath;
        sLastServiceType = svcType;
        outMsg = "Port 7820 ouvert avec succès sur votre Box (UPnP) !";
        return true;
    }
    return false;
}

bool UPnPOpenPort(uint16_t port, const std::string& localIp, std::string& outMsg)
{
    // 1. Direct SSDP + HTTP/SOAP (bypasses Windows Public Network restrictions)
    if (DirectSSDPUPnPOpen(port, localIp, outMsg)) {
        return true;
    }

#ifdef _WIN32
    // 2. Fallback to Windows COM IUPnPNAT
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
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
    BSTR bstrDesc = SysAllocString(L"melonDS Soul Link Direct P2P");
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
    if (!sLastControlPath.empty() && !sLastServiceType.empty()) {
        char soap[512];
        snprintf(soap, sizeof(soap),
            "<?xml version=\"1.0\"?>\r\n"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
            "<s:Body>\r\n"
            "<u:DeletePortMapping xmlns:u=\"%s\">\r\n"
            "  <NewRemoteHost></NewRemoteHost>\r\n"
            "  <NewExternalPort>%u</NewExternalPort>\r\n"
            "  <NewProtocol>TCP</NewProtocol>\r\n"
            "</u:DeletePortMapping>\r\n"
            "</s:Body>\r\n"
            "</s:Envelope>",
            sLastServiceType.c_str(), port);

        std::string soapAction = "\"" + sLastServiceType + "#DeletePortMapping\"";
        SimpleHttpSoap(sLastRouterHost, sLastRouterPort, sLastControlPath, soapAction, soap);
        sLastControlPath.clear();
    }

#ifdef _WIN32
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
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
