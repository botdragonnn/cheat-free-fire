#include "KeyAuth.hpp"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <rpc.h>
#include <sstream>
#include <iomanip>
#include <vector>

#include <ext/Auth/json.hpp>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "rpcrt4.lib")

using json = nlohmann::json;

namespace KeyAuth {

	namespace {

		static std::string ToNarrow(const std::wstring& wstr) {
			if (wstr.empty()) return "";
			int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
			std::string out(size, 0);
			WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &out[0], size, NULL, NULL);
			return out;
		}

		static std::string UrlEncode(const std::string& str) {
			static const char* hex = "0123456789ABCDEF";
			std::string out;
			for (unsigned char c : str) {
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
					c == '-' || c == '_' || c == '.' || c == '~') {
					out += (char)c;
				}
				else {
					out += '%';
					out += hex[c >> 4];
					out += hex[c & 0xF];
				}
			}
			return out;
		}

		static bool HttpPost(const std::string& host, const std::string& path, const std::string& body, std::string& outResponse, std::string& outSignature) {
			outResponse.clear();
			outSignature.clear();

			std::wstring wHost(host.begin(), host.end());
			std::wstring wPath(path.begin(), path.end());

			HINTERNET hSession = WinHttpOpen(L"ZimonClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!hSession) return false;

			HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
			if (!hConnect) {
				WinHttpCloseHandle(hSession);
				return false;
			}

			HINTERNET hRequest = WinHttpOpenRequest(
				hConnect, L"POST", wPath.c_str(), NULL, WINHTTP_NO_REFERER,
				WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
			if (!hRequest) {
				WinHttpCloseHandle(hConnect);
				WinHttpCloseHandle(hSession);
				return false;
			}

			LPCWSTR headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
			BOOL ok = WinHttpSendRequest(
				hRequest, headers, (DWORD)-1L,
				(LPVOID)(body.empty() ? NULL : body.c_str()), (DWORD)body.size(), (DWORD)body.size(), 0);
			if (ok) ok = WinHttpReceiveResponse(hRequest, NULL);

			if (ok) {
				DWORD sigLen = 0;
				WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, L"signature", WINHTTP_NO_OUTPUT_BUFFER, &sigLen, WINHTTP_NO_HEADER_INDEX);
				if (sigLen > 0) {
					std::vector<wchar_t> sigBuf(sigLen);
					if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, L"signature", sigBuf.data(), &sigLen, WINHTTP_NO_HEADER_INDEX)) {
						std::wstring wsig(sigBuf.data());
						outSignature = ToNarrow(wsig);
					}
				}

				DWORD available = 0;
				do {
					available = 0;
					if (!WinHttpQueryDataAvailable(hRequest, &available)) break;
					if (available == 0) break;
					std::vector<char> buf(available + 1, 0);
					DWORD read = 0;
					if (WinHttpReadData(hRequest, buf.data(), available, &read)) {
						outResponse.append(buf.data(), read);
					}
				} while (available > 0);
			}

			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return ok;
		}

		static std::string HmacSha256Hex(const std::string& key, const std::string& data) {
			BCRYPT_ALG_HANDLE hAlg = NULL;
			BCRYPT_HASH_HANDLE hHash = NULL;
			DWORD objLen = 0, hashLen = 0, bytesDone = 0;

			if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
				return "";

			BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&objLen, sizeof(DWORD), &bytesDone, 0);
			BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&hashLen, sizeof(DWORD), &bytesDone, 0);

			std::vector<BYTE> obj(objLen);
			std::vector<BYTE> hash(hashLen);

			if (BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, (PBYTE)key.data(), (ULONG)key.size(), 0) != 0) {
				BCryptCloseAlgorithmProvider(hAlg, 0);
				return "";
			}
			BCryptHashData(hHash, (PBYTE)data.data(), (ULONG)data.size(), 0);
			BCryptFinishHash(hHash, hash.data(), hashLen, 0);
			BCryptDestroyHash(hHash);
			BCryptCloseAlgorithmProvider(hAlg, 0);

			static const char* hexdigits = "0123456789abcdef";
			std::string out;
			out.reserve(hashLen * 2);
			for (BYTE b : hash) {
				out += hexdigits[b >> 4];
				out += hexdigits[b & 0xF];
			}
			return out;
		}

		static std::string GetHwid() {
			std::string machineGuid;
			HKEY hKey = NULL;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
				DWORD type = 0, size = 0;
				if (RegQueryValueExW(hKey, L"MachineGuid", NULL, &type, NULL, &size) == ERROR_SUCCESS && size > 1) {
					std::vector<wchar_t> buf((size / sizeof(wchar_t)) + 1, 0);
					DWORD actual = size;
					if (RegQueryValueExW(hKey, L"MachineGuid", NULL, &type, (LPBYTE)buf.data(), &actual) == ERROR_SUCCESS) {
						machineGuid = ToNarrow(std::wstring(buf.data()));
					}
				}
				RegCloseKey(hKey);
			}

			DWORD serial = 0;
			GetVolumeInformationW(L"C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);

			char serialStr[32] = { 0 };
			sprintf_s(serialStr, "%08X", serial);

			char user[256] = { 0 };
			DWORD userLen = sizeof(user);
			GetUserNameA(user, &userLen);

			char comp[256] = { 0 };
			DWORD compLen = sizeof(comp);
			GetComputerNameA(comp, &compLen);

			return std::string(serialStr) + "|" + machineGuid + "|" + user + "|" + comp;
		}

		static std::string ParseJsonString(const json& j, const std::string& key, const std::string& fallback) {
			if (j.is_object() && j.contains(key) && !j[key].is_null())
				return j[key].get<std::string>();
			return fallback;
		}

	}

	api::api(const std::string& appName, const std::string& owner, const std::string& sec, const std::string& ver, const std::string& apiUrl)
		: name(appName), ownerid(owner), secret(sec), version(ver), url(apiUrl) {}

	bool api::init() {
		UUID uuid = { 0 };
		RPC_CSTR szUuid = NULL;
		if (UuidCreate(&uuid) != RPC_S_OK)
			return false;
		if (UuidToStringA(&uuid, &szUuid) != RPC_S_OK)
			return false;
		std::string guid = (char*)szUuid;
		RpcStringFreeA(&szUuid);

		std::string sentKey = guid.substr(0, 16);
		enckey = sentKey + "-" + secret;

		std::string body =
			"type=init&ver=" + UrlEncode(version) +
			"&enckey=" + sentKey +
			"&name=" + UrlEncode(name) +
			"&ownerid=" + ownerid;

		std::string response, signature;
		if (!HttpPost("keyauth.win", "/api/1.2/", body, response, signature))
		{
			message = "Falha de conexao com o servidor";
			return false;
		}

		std::string expected = HmacSha256Hex(secret, response);
		if (expected.empty() || expected != signature) {
			message = "Assinatura invalida na resposta";
			return false;
		}

		json j = json::parse(response, nullptr, false);
		if (j.is_discarded()) {
			message = "Resposta invalida do servidor";
			return false;
		}

		success = j.value("success", false);
		message = ParseJsonString(j, "message", "Erro desconhecido");

		if (success) {
			sessionid = ParseJsonString(j, "sessionid", "");
			initalized = true;
		}
		return success;
	}

	bool api::license(const std::string& key) {
		if (!initalized) {
			message = "Inicialize a API primeiro";
			return false;
		}

		std::string body =
			"type=license&key=" + UrlEncode(key) +
			"&hwid=" + UrlEncode(GetHwid()) +
			"&sessionid=" + sessionid +
			"&name=" + UrlEncode(name) +
			"&ownerid=" + ownerid;

		std::string response, signature;
		if (!HttpPost("keyauth.win", "/api/1.2/", body, response, signature))
		{
			message = "Falha de conexao com o servidor";
			return false;
		}

		std::string expected = HmacSha256Hex(enckey, response);
		if (expected.empty() || expected != signature) {
			message = "Assinatura invalida na resposta";
			return false;
		}

		json j = json::parse(response, nullptr, false);
		if (j.is_discarded()) {
			message = "Resposta invalida do servidor";
			return false;
		}

		success = j.value("success", false);
		message = ParseJsonString(j, "message", "Erro desconhecido");

		if (success && j.contains("info") && j["info"].is_object()) {
			auto& info = j["info"];
			username = ParseJsonString(info, "username", "");
			subscriptions.clear();
			if (info.contains("subscriptions") && info["subscriptions"].is_array()) {
				for (auto& sub : info["subscriptions"]) {
					if (!sub.is_object()) continue;
					Subscription s;
					s.name = ParseJsonString(sub, "subscription", "");
					s.expiry = ParseJsonString(sub, "expiry", "");
					subscriptions.push_back(s);
				}
			}
		}
		return success;
	}

	std::string api::getExpiry() const {
		if (subscriptions.empty()) return "Lifetime";
		long long t = 0;
		try {
			t = std::stoll(subscriptions[0].expiry);
		}
		catch (...) {
			return "Lifetime";
		}
		if (t <= 0) return "Lifetime";
		time_t raw = (time_t)t;
		struct tm tmv;
		localtime_s(&tmv, &raw);
		char buf[32] = { 0 };
		strftime(buf, sizeof(buf), "%d/%m/%Y", &tmv);
		return std::string(buf);
	}

}
