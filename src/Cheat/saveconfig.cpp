#pragma once
#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstring>
#include "Globals.hpp"

#pragma comment(lib, "Shell32.lib")

extern Cheat::Globals g_Globals;

namespace Cheat {

    class Manager {
    public:
        static bool Save() {
            return SaveAs(DefaultPath());
        }
        static bool Load() {
            return LoadFrom(DefaultPath());
        }

        // Alternativamente, use um caminho customizado
        static bool SaveAs(const std::wstring& path) {
            try {
                ensureParent(path);
                std::ofstream ofs{ std::filesystem::path(path), std::ios::trunc };
                if (!ofs.is_open()) return false;

                ofs << std::fixed << std::setprecision(6);

                // ---------- [AimBot] ----------
                ofs << "[AimBot]\n";
                wBool(ofs, "Enabled", g_Globals.AimBot.Enabled);
                //wInt(ofs, "aimtype", g_Globals.AimBot.aimtype);
                wBool(ofs, "IgnoreKnocked", g_Globals.AimBot.IgnoreKnocked);
                wBool(ofs, "IgnoreBots", g_Globals.AimBot.IgnoreBots);
                wInt(ofs, "KeyBind", g_Globals.AimBot.KeyBind);
                wInt(ofs, "KeyBind1", g_Globals.AimBot.MagKey);
                wInt(ofs, "GhostKey", g_Globals.AimBot.ghostkey);
                wBool(ofs, "Magnect", g_Globals.AimBot.aimmagnect);
                wBool(ofs, "Ghost", g_Globals.AimBot.ghost);
                wInt(ofs, "Fov", g_Globals.AimBot.Fov);
                wInt(ofs, "MaxDistance", g_Globals.AimBot.MaxDistance);
                wInt(ofs, "Sleep", g_Globals.AimBot.Sleep);
                wInt(ofs, "PeitosIndex", g_Globals.AimBot.PeitosIndex);
                wInt(ofs, "Target", g_Globals.AimBot.Target);
                ofs << "\n";

                // ---------- [Visuals.ESP] ----------
                ofs << "[Visuals.ESP]\n";
                wBool(ofs, "Enabled", g_Globals.Visuals.ESP.Enabled);
                wBool(ofs, "ShowTeam", g_Globals.Visuals.ESP.ShowTeam);
                wInt(ofs, "RenderDistance", g_Globals.Visuals.ESP.RenderDistance);
                wFloat(ofs, "Thickness", g_Globals.Visuals.ESP.Thickness);
                wFloat(ofs, "TextSize", g_Globals.Visuals.ESP.TextSize);
                wBool(ofs, "Watermark", g_Globals.Visuals.ESP.Watermark);
                wFloat4(ofs, "WatermarkColor", g_Globals.Visuals.ESP.WatermarkColor);
                wBool(ofs, "Enemy", g_Globals.Visuals.ESP.Enemy);
                wFloat4(ofs, "EnemyColor", g_Globals.Visuals.ESP.EnemyColor);
                wBool(ofs, "Weapon", g_Globals.Visuals.ESP.Weapon);
                wBool(ofs, "ShowIcons", g_Globals.Visuals.ESP.ShowIcons);
                wInt(ofs, "WeaponStyle", g_Globals.Visuals.ESP.WeaponStyle);
                wFloat4(ofs, "WeaponColor", g_Globals.Visuals.ESP.WeaponColor);
                wBool(ofs, "SnapLines", g_Globals.Visuals.ESP.SnapLines);
                wInt(ofs, "SnapLinesPos", g_Globals.Visuals.ESP.SnapLinesPos);
                wFloat4(ofs, "SnapLinesColor", g_Globals.Visuals.ESP.SnapLinesColor);
                wBool(ofs, "HealthBar", g_Globals.Visuals.ESP.HealthBar);
                wInt(ofs, "HealthBarStyle", g_Globals.Visuals.ESP.HealthBarStyle);
                wBool(ofs, "Box", g_Globals.Visuals.ESP.Box);
                wBool(ofs, "BoxFilled", g_Globals.Visuals.ESP.BoxFilled);
                wInt(ofs, "BoxStyle", g_Globals.Visuals.ESP.BoxStyle);
                wFloat4(ofs, "BoxColor", g_Globals.Visuals.ESP.BoxColor);
                wFloat4(ofs, "FilledBoxColor", g_Globals.Visuals.ESP.FilledBoxColor);
                wBool(ofs, "ShowName", g_Globals.Visuals.ESP.ShowName);
                wFloat4(ofs, "NameColor", g_Globals.Visuals.ESP.NameColor);
                wBool(ofs, "Distance", g_Globals.Visuals.ESP.Distance);
                wFloat4(ofs, "DistanceColor", g_Globals.Visuals.ESP.DistanceColor);
                wBool(ofs, "Skeleton", g_Globals.Visuals.ESP.Skeleton);
                wFloat4(ofs, "SkeletonColor", g_Globals.Visuals.ESP.SkeletonColor);
                ofs << "\n";

                // ---------- [Visuals.Chams] ----------
                ofs << "[Visuals.Chams]\n";
                wBool(ofs, "Enabled", g_Globals.Visuals.Chams.Enabled);
                wFloat4(ofs, "NearColor", g_Globals.Visuals.Chams.NearColor);
                wFloat4(ofs, "FarColor", g_Globals.Visuals.Chams.FarColor);
                ofs << "\n";

                // ---------- [Misc.Screen] ----------
                ofs << "[Misc.Screen]\n";
                wBool(ofs, "ShowAimbotFov", g_Globals.Misc.Screen.ShowAimbotFov);
                wFloat4(ofs, "AimbotFovColor", g_Globals.Misc.Screen.AimbotFovColor);
                wFloat4(ofs, "FilledFovColor", g_Globals.Misc.Screen.FilledFovColor);
                ofs << "\n";
                // ---------- [Silent] ----------
                ofs << "[Silent]\n";
                wBool(ofs, "Enabled", g_Globals.Silent.Enabled);
                wInt(ofs, "KeyBind", g_Globals.Silent.KeyBind);
                wInt(ofs, "Fov", g_Globals.Silent.Fov);
                wInt(ofs, "MaxDistance", g_Globals.Silent.MaxDistance);
                wBool(ofs, "ShowFov", g_Globals.Misc.Screen.ShowSilentFov);
                wFloat4(ofs, "FovColor", g_Globals.Misc.Screen.SilentFovColor);
                wFloat4(ofs, "FilledFovColor", g_Globals.Misc.Screen.SilentFilledFovColor);
                ofs << "\n";

                // ---------- [Misc.Exploits.LocalPlayer] ----------
                ofs << "[Misc.Exploits.LocalPlayer]\n";
                wBool(ofs, "AimLock2x", g_Globals.Misc.Exploits.LocalPlayer.AimLock2x);
                wBool(ofs, "AimbotSniper", g_Globals.Misc.Exploits.LocalPlayer.AimbotAwm);
                wBool(ofs, "AimlockGeral", g_Globals.Misc.Exploits.LocalPlayer.Aimlock);
                wBool(ofs, "BugarPixel", g_Globals.Misc.Exploits.LocalPlayer.BugarPixel);
                wBool(ofs, "BackJump", g_Globals.Misc.Exploits.LocalPlayer.BackJump);
                wBool(ofs, "SocoLonge", g_Globals.Misc.Exploits.LocalPlayer.SocoLonge);
                wBool(ofs, "NoRecoil", g_Globals.Misc.Exploits.LocalPlayer.NoRecoil);
                wBool(ofs, "Precision", g_Globals.Misc.Exploits.LocalPlayer.Precision);
                wInt(ofs, "RecoilControl", g_Globals.Misc.Exploits.LocalPlayer.RecoilControl);
                wBool(ofs, "FastMedkit", g_Globals.Misc.Exploits.LocalPlayer.FastMedkit);
                wBool(ofs, "TelaParada", g_Globals.Misc.Exploits.LocalPlayer.telaparada);
                wBool(ofs, "AtributarArma", g_Globals.Misc.Exploits.LocalPlayer.AtributarArma);
                wInt(ofs, "AtributarArmaLevel", g_Globals.Misc.Exploits.LocalPlayer.AtributarArmaLevel);
                wBool(ofs, "MaxDamage", g_Globals.Misc.Exploits.LocalPlayer.MoreDamage);
                wBool(ofs, "SpinBot", g_Globals.Misc.Exploits.LocalPlayer.SpinBot);
                wBool(ofs, "NoFireDelay", g_Globals.Misc.Exploits.LocalPlayer.FireDelay);
                ofs << "\n";

                // ---------- [General] ----------
                ofs << "[General]\n";
                wInt(ofs, "MenuKey", g_Globals.General.MenuKey);
                wInt(ofs, "ThreadDelay", g_Globals.General.ThreadDelay);
                wBool(ofs, "CaptureBypass", g_Globals.General.CaptureBypass);
                ofs << "\n";

                return true;
            }
            catch (...) {
                return false;
            }
        }

        static bool LoadFrom(const std::wstring& path) {
            std::ifstream ifs{ std::filesystem::path(path) };
            if (!ifs.is_open()) return false;

            // Parse INI simples -> mapa "Section.key" -> "value"
            std::unordered_map<std::string, std::string> kv;
            std::string line, section;
            while (std::getline(ifs, line)) {
                stripCR(line);
                trim(line);
                if (line.empty() || line[0] == ';' || line[0] == '#')
                    continue;
                if (line.front() == '[' && line.back() == ']') {
                    section = line.substr(1, line.size() - 2);
                    trim(section);
                    continue;
                }
                auto pos = line.find('=');
                if (pos == std::string::npos) continue;
                std::string key = line.substr(0, pos);
                std::string val = line.substr(pos + 1);
                trim(key); trim(val);
                kv[section + "." + key] = val;
            }

            // Helpers
            auto gBool = [&](const char* sec, const char* key, bool& out) { return readBool(kv, sec, key, out); };
            auto gInt = [&](const char* sec, const char* key, int& out) {  return readInt(kv, sec, key, out);  };
            auto gFloat = [&](const char* sec, const char* key, float& out) {return readFloat(kv, sec, key, out); };
            auto gF4 = [&](const char* sec, const char* key, float out[4]) { readFloatN(kv, sec, key, out, 4); };
            // auto gStr = ...  (n�o usado mais, pois strings foram removidas do carregamento)

            // ---------- AimBot ----------
            gBool("AimBot", "Enabled", g_Globals.AimBot.Enabled);
            //gInt("AimBot", "aimtype", g_Globals.AimBot.aimtype);
            gBool("AimBot", "IgnoreKnocked", g_Globals.AimBot.IgnoreKnocked);
            gBool("AimBot", "IgnoreBots", g_Globals.AimBot.IgnoreBots);
            gInt("AimBot", "KeyBind", g_Globals.AimBot.KeyBind);
            gInt("AimBot", "KeyBind1", g_Globals.AimBot.MagKey);
            gInt("AimBot", "GhostKey", g_Globals.AimBot.ghostkey);
            gBool("AimBot", "Magnect", g_Globals.AimBot.aimmagnect);
            gBool("AimBot", "Ghost", g_Globals.AimBot.ghost);

            gInt("AimBot", "Fov", g_Globals.AimBot.Fov);

            gInt("AimBot", "MaxDistance", g_Globals.AimBot.MaxDistance);
            gInt("AimBot", "Sleep", g_Globals.AimBot.Sleep);
            gInt("AimBot", "PeitosIndex", g_Globals.AimBot.PeitosIndex);
            gInt("AimBot", "Target", g_Globals.AimBot.Target);

            // ---------- Visuals.ESP ----------
            gBool("Visuals.ESP", "Enabled", g_Globals.Visuals.ESP.Enabled);
            gBool("Visuals.ESP", "ShowTeam", g_Globals.Visuals.ESP.ShowTeam);
            gInt("Visuals.ESP", "RenderDistance", g_Globals.Visuals.ESP.RenderDistance);
            gFloat("Visuals.ESP", "Thickness", g_Globals.Visuals.ESP.Thickness);
            gFloat("Visuals.ESP", "TextSize", g_Globals.Visuals.ESP.TextSize);
            gBool("Visuals.ESP", "Watermark", g_Globals.Visuals.ESP.Watermark);
            gF4("Visuals.ESP", "WatermarkColor", g_Globals.Visuals.ESP.WatermarkColor);
            gBool("Visuals.ESP", "Enemy", g_Globals.Visuals.ESP.Enemy);
            gF4("Visuals.ESP", "EnemyColor", g_Globals.Visuals.ESP.EnemyColor);
            gBool("Visuals.ESP", "Weapon", g_Globals.Visuals.ESP.Weapon);
            gBool("Visuals.ESP", "ShowIcons", g_Globals.Visuals.ESP.ShowIcons);
            gInt("Visuals.ESP", "WeaponStyle", g_Globals.Visuals.ESP.WeaponStyle);
            gF4("Visuals.ESP", "WeaponColor", g_Globals.Visuals.ESP.WeaponColor);
            gBool("Visuals.ESP", "SnapLines", g_Globals.Visuals.ESP.SnapLines);
            gInt("Visuals.ESP", "SnapLinesPos", g_Globals.Visuals.ESP.SnapLinesPos);
            gF4("Visuals.ESP", "SnapLinesColor", g_Globals.Visuals.ESP.SnapLinesColor);
            gBool("Visuals.ESP", "HealthBar", g_Globals.Visuals.ESP.HealthBar);
            gInt("Visuals.ESP", "HealthBarStyle", g_Globals.Visuals.ESP.HealthBarStyle);
            gBool("Visuals.ESP", "Box", g_Globals.Visuals.ESP.Box);
            gBool("Visuals.ESP", "BoxFilled", g_Globals.Visuals.ESP.BoxFilled);
            gInt("Visuals.ESP", "BoxStyle", g_Globals.Visuals.ESP.BoxStyle);
            gF4("Visuals.ESP", "BoxColor", g_Globals.Visuals.ESP.BoxColor);
            gF4("Visuals.ESP", "FilledBoxColor", g_Globals.Visuals.ESP.FilledBoxColor);
            gBool("Visuals.ESP", "ShowName", g_Globals.Visuals.ESP.ShowName);
            gF4("Visuals.ESP", "NameColor", g_Globals.Visuals.ESP.NameColor);
            gBool("Visuals.ESP", "Distance", g_Globals.Visuals.ESP.Distance);
            gF4("Visuals.ESP", "DistanceColor", g_Globals.Visuals.ESP.DistanceColor);
            gBool("Visuals.ESP", "Skeleton", g_Globals.Visuals.ESP.Skeleton);
            gF4("Visuals.ESP", "SkeletonColor", g_Globals.Visuals.ESP.SkeletonColor);

            // ---------- Visuals.Chams ----------
            gBool("Visuals.Chams", "Enabled", g_Globals.Visuals.Chams.Enabled);
            gF4("Visuals.Chams", "NearColor", g_Globals.Visuals.Chams.NearColor);
            gF4("Visuals.Chams", "FarColor", g_Globals.Visuals.Chams.FarColor);

            // ---------- Misc.Screen ----------
            gBool("Misc.Screen", "ShowAimbotFov", g_Globals.Misc.Screen.ShowAimbotFov);
            gF4("Misc.Screen", "AimbotFovColor", g_Globals.Misc.Screen.AimbotFovColor);
            gF4("Misc.Screen", "FilledFovColor", g_Globals.Misc.Screen.FilledFovColor);

            gBool("Silent", "Enabled", g_Globals.Silent.Enabled);
            gInt("Silent", "KeyBind", g_Globals.Silent.KeyBind);
            gInt("Silent", "Fov", g_Globals.Silent.Fov);
            gInt("Silent", "MaxDistance", g_Globals.Silent.MaxDistance);
            gBool("Silent", "ShowFov", g_Globals.Misc.Screen.ShowSilentFov);
            gF4("Silent", "FovColor", g_Globals.Misc.Screen.SilentFovColor);
            gF4("Silent", "FilledFovColor", g_Globals.Misc.Screen.SilentFilledFovColor);

            // ---------- Misc.Exploits.LocalPlayer ----------
            gBool("Misc.Exploits.LocalPlayer", "AimLock2x", g_Globals.Misc.Exploits.LocalPlayer.AimLock2x);
            gBool("Misc.Exploits.LocalPlayer", "AimbotSniper", g_Globals.Misc.Exploits.LocalPlayer.AimbotAwm);
            gBool("Misc.Exploits.LocalPlayer", "AimlockGeral", g_Globals.Misc.Exploits.LocalPlayer.Aimlock);
            gBool("Misc.Exploits.LocalPlayer", "BugarPixel", g_Globals.Misc.Exploits.LocalPlayer.BugarPixel);
            gBool("Misc.Exploits.LocalPlayer", "BackJump", g_Globals.Misc.Exploits.LocalPlayer.BackJump);
            gBool("Misc.Exploits.LocalPlayer", "SocoLonge", g_Globals.Misc.Exploits.LocalPlayer.SocoLonge);
            gBool("Misc.Exploits.LocalPlayer", "NoRecoil", g_Globals.Misc.Exploits.LocalPlayer.NoRecoil);
            gBool("Misc.Exploits.LocalPlayer", "Precision", g_Globals.Misc.Exploits.LocalPlayer.Precision);
            gInt("Misc.Exploits.LocalPlayer", "RecoilControl", g_Globals.Misc.Exploits.LocalPlayer.RecoilControl);
            gBool("Misc.Exploits.LocalPlayer", "FastMedkit", g_Globals.Misc.Exploits.LocalPlayer.FastMedkit);
            gBool("Misc.Exploits.LocalPlayer", "TelaParada", g_Globals.Misc.Exploits.LocalPlayer.telaparada);
            gBool("Misc.Exploits.LocalPlayer", "AtributarArma", g_Globals.Misc.Exploits.LocalPlayer.AtributarArma);
            gInt("Misc.Exploits.LocalPlayer", "AtributarArmaLevel", g_Globals.Misc.Exploits.LocalPlayer.AtributarArmaLevel);
            gBool("Misc.Exploits.LocalPlayer", "MaxDamage", g_Globals.Misc.Exploits.LocalPlayer.MoreDamage);
            gBool("Misc.Exploits.LocalPlayer", "SpinBot", g_Globals.Misc.Exploits.LocalPlayer.SpinBot);
            gBool("Misc.Exploits.LocalPlayer", "NoFireDelay", g_Globals.Misc.Exploits.LocalPlayer.FireDelay);

            // ---------- General ----------
            gInt("General", "MenuKey", g_Globals.General.MenuKey);
            gInt("General", "ThreadDelay", g_Globals.General.ThreadDelay);
            gBool("General", "CaptureBypass", g_Globals.General.CaptureBypass);


            return true;
        }

        // Caminho padr�o do arquivo
        static std::wstring DefaultPath() {
            std::wstring base = knownFolder(FOLDERID_LocalAppData);
            if (base.empty()) base = L".";
            std::filesystem::path p = std::filesystem::path(base) / L"HwMon" / L"config.dat";
            return p.wstring();
        }

    private:
        // ---------- Helpers de caminho ----------
        static std::wstring knownFolder(REFKNOWNFOLDERID id) {
            PWSTR w = nullptr;
            std::wstring out;
            if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, NULL, &w))) {
                out = w;
                CoTaskMemFree(w);
            }
            return out;
        }
        static void ensureParent(const std::wstring& file) {
            std::filesystem::path p(file);
            std::filesystem::create_directories(p.parent_path());
        }

        // ---------- Helpers de escrita ----------
        static size_t bounded_len(const char* s, size_t cap) {
            size_t i = 0;
            if (!s) return 0;
            while (i < cap && s[i] != '\0') ++i;
            return i;
        }

        static void wBool(std::ostream& os, const char* key, bool v) { os << key << '=' << (v ? 1 : 0) << '\n'; }
        static void wInt(std::ostream& os, const char* key, int v) { os << key << '=' << v << '\n'; }
        static void wFloat(std::ostream& os, const char* key, float v) { os << key << '=' << v << '\n'; }
        static void wFloat4(std::ostream& os, const char* key, const float c[4]) {
            os << key << '=' << c[0] << ',' << c[1] << ',' << c[2] << ',' << c[3] << '\n';
        }
        static void wStr(std::ostream& os, const char* key, const char* buf, size_t cap) {
            const size_t len = bounded_len(buf, cap);
            std::string s(buf, buf + (len < cap ? len : cap));
            os << key << '=' << s << '\n';
        }

        // ---------- Helpers de leitura ----------
        static inline void stripCR(std::string& s) { if (!s.empty() && s.back() == '\r') s.pop_back(); }
        static inline void trim(std::string& s) {
            auto issp = [](unsigned char c) { return std::isspace(c) != 0; };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c) { return !issp(c); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c) { return !issp(c); }).base(), s.end());
        }
        static std::string keyOf(const char* sec, const char* k) {
            std::string out; out.reserve(strlen(sec) + 1 + strlen(k));
            out.append(sec).append(".").append(k);
            return out;
        }
        static bool readBool(const std::unordered_map<std::string, std::string>& kv, const char* sec, const char* k, bool& out) {
            auto it = kv.find(keyOf(sec, k)); if (it == kv.end()) return false;
            const std::string& v = it->second;
            if (v == "1" || v == "true" || v == "True" || v == "TRUE") { out = true;  return true; }
            if (v == "0" || v == "false" || v == "False" || v == "FALSE") { out = false; return true; }
            out = (std::stoi(v) != 0); return true;
        }
        static bool readInt(const std::unordered_map<std::string, std::string>& kv, const char* sec, const char* k, int& out) {
            auto it = kv.find(keyOf(sec, k)); if (it == kv.end()) return false;
            out = std::stoi(it->second); return true;
        }
        static bool readFloat(const std::unordered_map<std::string, std::string>& kv, const char* sec, const char* k, float& out) {
            auto it = kv.find(keyOf(sec, k)); if (it == kv.end()) return false;
            out = std::strtof(it->second.c_str(), nullptr); return true;
        }
        static void readFloatN(const std::unordered_map<std::string, std::string>& kv, const char* sec, const char* k, float* out, int n) {
            auto it = kv.find(keyOf(sec, k)); if (it == kv.end()) return;
            std::string s = it->second;
            std::vector<float> vals; vals.reserve(n);
            size_t start = 0;
            while (start < s.size() && (int)vals.size() < n) {
                size_t comma = s.find(',', start);
                std::string tok = (comma == std::string::npos) ? s.substr(start) : s.substr(start, comma - start);
                trim(tok);
                if (!tok.empty()) vals.push_back(std::strtof(tok.c_str(), nullptr));
                start = (comma == std::string::npos) ? s.size() : comma + 1;
            }
            for (int i = 0; i < n && i < (int)vals.size(); ++i) out[i] = vals[i];
        }
        static void readStr(const std::unordered_map<std::string, std::string>& kv, const char* sec, const char* k, char* buf, size_t cap) {
            if (cap == 0) return;
            auto it = kv.find(keyOf(sec, k)); if (it == kv.end()) return;
            const std::string& v = it->second;
            size_t cap1 = cap - 1;
            size_t len = (std::min)(cap1, v.size());
            memcpy(buf, v.data(), len);
            buf[len] = '\0';
        }
    };

} // namespace Cheat