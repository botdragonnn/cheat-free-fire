#pragma once
#include <string>
#include <vector>
#include <sddl.h>
#include "crypto.h"
#include "obf_key.h"
#include <XorStr.hpp>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")

/*
 * Protocolo v2
 *
 * Request:  [magic(1)] [total_len(2)] [client_nonce(16) | payload]
 * Response: [server_nonce(16)] [enc_len(2)] [encrypted_data] [hmac(32)]
 *
 * session_key  = HMAC-SHA256(MASTER_KEY, client_nonce || server_nonce)
 * encrypted    = XOR(plaintext, keystream(session_key, server_nonce))
 * hmac         = HMAC-SHA256(session_key, server_nonce || encrypted)
 */

class LuxClient {
public:
    // Key ofuscada em compile-time. No binário só aparece key^pad.
    // Decifrada no stack só quando usada, zerada automaticamente.
    static constexpr uint8_t MASTER_KEY_ENC[32] = OBFUSCATE_KEY_32(
        0xA3, 0x1F, 0x7C, 0xE2, 0x5B, 0x09, 0xD4, 0x88,
        0x6E, 0xF1, 0x2A, 0xC7, 0x93, 0x40, 0xBD, 0x56,
        0x0D, 0xE8, 0x74, 0x3B, 0xAF, 0x62, 0x19, 0xC5,
        0x81, 0xFE, 0x47, 0x0A, 0xD6, 0x33, 0x9F, 0x58
    );

    LuxClient(const char* targetIp, int targetPort)
        : ip(targetIp), port(targetPort), sock(INVALID_SOCKET) {
    }

    ~LuxClient() { Close(); }

    bool Init() {
        if (sock != INVALID_SOCKET) { closesocket(sock); sock = INVALID_SOCKET; }
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { message = XorStr("WinSock init failed."); return false; }
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) { message = XorStr("Socket creation failed."); WSACleanup(); return false; }

        DWORD timeout = 15000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        if (addr.sin_addr.s_addr == INADDR_NONE) { message = XorStr("Invalid IP."); Close(); return false; }
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) { message = XorStr("Connection failed."); Close(); return false; }
        return true;
    }

    bool Authenticate(const std::string& user, const std::string& pass) {
        if (!Init()) return false;
        std::string payload = std::string(XorStr("luxinternal01")) + "|" + user + "|" + pass + "|" + GetHwid();

        uint8_t cn[16];
        if (!LuxCrypto::gen_nonce(cn, 16)) { message = XorStr("Nonce gen failed."); return false; }

        SendSecure(2, cn, payload);

        std::string plain;
        if (!RecvSecure(cn, plain)) return false;
        return ParseResponse(plain);
    }

    bool RegisterAccount(const std::string& user, const std::string& pass, const std::string& key) {
        if (!Init()) return false;
        std::string payload = std::string(XorStr("luxinternal01")) + "|" + user + "|" + pass + "|" + GetHwid() + "|" + key;

        uint8_t cn[16];
        if (!LuxCrypto::gen_nonce(cn, 16)) { message = XorStr("Nonce gen failed."); return false; }

        SendSecure(3, cn, payload);

        std::string plain;
        if (!RecvSecure(cn, plain)) return false;
        return ParseResponse(plain);
    }

    std::vector<unsigned char> GetAvatar(const std::string& avatar) {
        if (!Init()) return {};
        if (sock == INVALID_SOCKET) return {};
        uint8_t magic = 4;
        send(sock, (const char*)&magic, 1, 0);
        uint16_t len = (uint16_t)avatar.size();
        uint8_t lb[2] = { (uint8_t)(len & 0xFF),(uint8_t)(len >> 8) };
        send(sock, (const char*)lb, 2, 0);
        send(sock, avatar.c_str(), (int)avatar.size(), 0);

        std::string hdr = RecvRaw(2);
        if (hdr.size() < 2) { message = XorStr("Avatar header failed."); return {}; }
        uint16_t alen = (unsigned char)hdr[0] | ((unsigned char)hdr[1] << 8);
        if (!alen) { message = XorStr("Empty avatar."); return {}; }
        std::string data = RecvRaw(alen);
        if (data.size() != alen) { message = XorStr("Avatar data failed."); return {}; }
        return std::vector<unsigned char>(data.begin(), data.end());
    }

    std::string GetResponse() const { return message; }
    std::string GetDays() const {
        if (!days.empty()) { try { if (std::stoi(days) > 365) return XorStr("Lifetime"); } catch (...) {} }
        return days;
    }

    void Close() {
        if (sock != INVALID_SOCKET) { closesocket(sock); sock = INVALID_SOCKET; }
        WSACleanup();
    }

private:
    std::string ip;
    int port;
    SOCKET sock;
    std::string message, days;

    void SendSecure(uint8_t magic, const uint8_t cn[16], const std::string& payload) {
        if (sock == INVALID_SOCKET) return;
        uint16_t totalLen = (uint16_t)(16 + payload.size());
        send(sock, (const char*)&magic, 1, 0);
        uint8_t lb[2] = { (uint8_t)(totalLen & 0xFF),(uint8_t)(totalLen >> 8) };
        send(sock, (const char*)lb, 2, 0);
        send(sock, (const char*)cn, 16, 0);
        send(sock, payload.c_str(), (int)payload.size(), 0);
    }

    bool RecvSecure(const uint8_t cn[16], std::string& out) {
        if (sock == INVALID_SOCKET) { message = XorStr("Not connected."); return false; }

        uint8_t sn[16];
        if (!RecvExact((char*)sn, 16)) { message = XorStr("Server nonce failed."); return false; }

        uint8_t lb[2];
        if (!RecvExact((char*)lb, 2)) { message = XorStr("Length failed."); return false; }
        uint16_t encLen = (uint16_t)lb[0] | ((uint16_t)lb[1] << 8);
        if (encLen == 0 || encLen > 4096) { message = XorStr("Bad length."); return false; }

        std::vector<uint8_t> enc(encLen);
        if (!RecvExact((char*)enc.data(), encLen)) { message = XorStr("Data failed."); return false; }

        uint8_t recvMac[32];
        if (!RecvExact((char*)recvMac, 32)) { message = XorStr("HMAC failed."); return false; }

        auto mk = ObfuscatedKey::decrypt(MASTER_KEY_ENC);
        uint8_t sk[32];
        LuxCrypto::derive_session_key(mk.data(), 32, cn, sn, sk);

        uint8_t computedMac[32];
        LuxCrypto::hmac_sha256_2(sk, 32, sn, 16, enc.data(), encLen, computedMac);

        if (!LuxCrypto::secure_cmp(recvMac, computedMac, 32)) {
            message = XorStr("Integrity check failed. Possible tampering.");
            memset(sk, 0, 32);
            return false;
        }

        LuxCrypto::stream_cipher(sk, 32, sn, 16, enc.data(), encLen);
        out.assign((char*)enc.data(), encLen);

        memset(sk, 0, 32);
        return true;
    }

    bool ParseResponse(const std::string& r) {
        auto parts = Split(r, '|');
        if (parts.empty()) { message = XorStr("Invalid response."); return false; }
        bool ok = (parts[0] == XorStr("true"));
        message = parts.size() > 1 ? parts[1] : (ok ? XorStr("OK") : XorStr("Failed"));
        days = parts.size() > 2 ? parts[2] : "";
        return ok;
    }

    bool RecvExact(char* buf, size_t sz) {
        size_t total = 0;
        while (total < sz) { int r = recv(sock, buf + total, (int)(sz - total), 0); if (r <= 0)return false; total += r; }
        return true;
    }
    std::string RecvRaw(size_t sz) {
        std::string o(sz, '\0'); if (!RecvExact(&o[0], sz)) return ""; return o;
    }
    std::vector<std::string> Split(const std::string& s, char d) {
        std::vector<std::string> p; std::string t;
        for (char c : s) { if (c == d) { p.push_back(t); t.clear(); } else t += c; } p.push_back(t); return p;
    }
    std::string GetHwid() {
        HANDLE hToken = NULL; PTOKEN_USER pTU = NULL; DWORD sz = 0; std::string r = XorStr("None");
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hToken)) return r;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &sz);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) { CloseHandle(hToken); return r; }
        pTU = (PTOKEN_USER)LocalAlloc(LPTR, sz);
        if (!pTU) { CloseHandle(hToken); return r; }
        if (GetTokenInformation(hToken, TokenUser, pTU, sz, &sz)) {
            LPSTR sid = NULL;
            if (ConvertSidToStringSidA(pTU->User.Sid, &sid)) { r = sid; LocalFree(sid); }
        }
        if (pTU)LocalFree(pTU); if (hToken)CloseHandle(hToken); return r;
    }
};