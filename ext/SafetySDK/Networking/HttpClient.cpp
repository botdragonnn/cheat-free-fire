#include "HttpClient.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <iostream>

#pragma comment(lib, "winhttp.lib")

namespace SafetySDK {

    // Helper conversion functions
    static std::wstring ToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    static std::string ToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    HttpClient::HttpClient(const std::wstring& domain, int prt, bool https)
        : baseDomain(domain), port(prt), useHttps(https) {}

    HttpClient::~HttpClient() {}

    void HttpClient::SetToken(const std::string& tkn) {
        token = tkn;
    }

    std::string HttpClient::GetToken() const {
        return token;
    }

    void HttpClient::SetAuthDetails(const std::string& dId, const std::string& lKey, const std::string& pHash) {
        discordId = dId;
        licenseKey = lKey;
        productHash = pHash;
    }

    HttpResponse HttpClient::Request(
        const std::string& method,
        const std::string& path,
        const std::string& body,
        const std::map<std::string, std::string>& headers
    ) {
        HttpResponse response;
        HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

        hSession = WinHttpOpen(
            userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );

        if (!hSession) return response;

        hConnect = WinHttpConnect(
            hSession,
            baseDomain.c_str(),
            (INTERNET_PORT)port,
            0
        );

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return response;
        }

        std::wstring wMethod = ToWString(method);
        std::wstring wPath = ToWString(path);

        DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;

        hRequest = WinHttpOpenRequest(
            hConnect,
            wMethod.c_str(),
            wPath.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags
        );

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Add default/custom headers
        std::wstring requestHeaders;
        if (!token.empty()) {
            requestHeaders += L"Authorization: Bearer " + ToWString(token) + L"\r\n";
        }
        if (!discordId.empty()) {
            requestHeaders += L"x-discord-id: " + ToWString(discordId) + L"\r\n";
        }
        if (!licenseKey.empty()) {
            requestHeaders += L"x-license-key: " + ToWString(licenseKey) + L"\r\n";
        }
        if (!productHash.empty()) {
            requestHeaders += L"x-product-hash: " + ToWString(productHash) + L"\r\n";
        }
        requestHeaders += L"Content-Type: application/json\r\n";

        for (const auto& [key, value] : headers) {
            requestHeaders += ToWString(key) + L": " + ToWString(value) + L"\r\n";
        }

        if (!requestHeaders.empty()) {
            WinHttpAddRequestHeaders(
                hRequest,
                requestHeaders.c_str(),
                (ULONG)-1L,
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE
            );
        }

        // Send request
        BOOL bResults = WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            (LPVOID)(body.empty() ? NULL : body.c_str()),
            (DWORD)(body.size()),
            (DWORD)(body.size()),
            0
        );

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        if (bResults) {
            // Get status code
            DWORD dwStatusCode = 0;
            DWORD dwSize = sizeof(dwStatusCode);
            WinHttpQueryHeaders(
                hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &dwStatusCode,
                &dwSize,
                WINHTTP_NO_HEADER_INDEX
            );
            response.statusCode = (int)dwStatusCode;

            // Read response body
            DWORD dwSizeAvailable = 0;
            do {
                dwSizeAvailable = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvailable)) {
                    break;
                }

                if (dwSizeAvailable == 0) break;

                std::vector<char> tempBuffer(dwSizeAvailable + 1, 0);
                DWORD dwBytesRead = 0;

                if (WinHttpReadData(hRequest, tempBuffer.data(), dwSizeAvailable, &dwBytesRead)) {
                    response.body.append(tempBuffer.data(), dwBytesRead);
                }
            } while (dwSizeAvailable > 0);
        }

        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);

        return response;
    }

}
