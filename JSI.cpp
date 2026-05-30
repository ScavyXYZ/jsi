#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <conio.h>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <set>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <map>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <malloc.h>
#include <string.h>
#include <IPTypes.h>
#include <shobjidl.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <utility>
#include <iomanip>   // for std::setw

#pragma warning(disable : 4996)
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "mpr.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// Helper: convert UTF-8 std::string to std::wstring
static std::wstring to_wstring(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 1) return L"";
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    return w;
}

class Config {
public:
    std::map<std::string, std::string> data;

    bool Load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;

        std::string section;
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t s = line.find_first_not_of(" \t");
            if (s == std::string::npos) continue;
            line = line.substr(s);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line[0] == '[') {
                size_t e = line.find(']');
                section = (e != std::string::npos) ? line.substr(1, e - 1) : "";
                continue;
            }
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));
            size_t ic = value.find(" ;");
            if (ic == std::string::npos) ic = value.find(" #");
            if (ic != std::string::npos) value = Trim(value.substr(0, ic));
            data[(section.empty() ? key : section + "." + key)] = value;
        }
        return true;
    }

    std::string GetStr(const std::string& k, const std::string& def = "") const {
        auto it = data.find(k);
        return (it != data.end()) ? it->second : def;
    }
    std::wstring GetWStr(const std::string& k, const std::wstring& def = L"") const {
        auto it = data.find(k);
        if (it == data.end()) return def;
        const std::string& s = it->second;
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 1) return def;
        std::wstring w(len - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
        return w;
    }
    int GetInt(const std::string& k, int def = 0) const {
        auto it = data.find(k);
        if (it == data.end()) return def;
        try { return std::stoi(it->second); }
        catch (...) { return def; }
    }
    bool GetBool(const std::string& k, bool def = false) const {
        auto it = data.find(k);
        if (it == data.end()) return def;
        const std::string& v = it->second;
        if (v == "1" || v == "true" || v == "yes" || v == "on")  return true;
        if (v == "0" || v == "false" || v == "no" || v == "off") return false;
        return def;
    }

private:
    static std::string Trim(const std::string& s) {
        size_t b = s.find_first_not_of(" \t\"");
        size_t e = s.find_last_not_of(" \t\"");
        return (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
    }
};

Config g_cfg;

namespace NetworkConfig {
    int         MaxScanThreads() { return g_cfg.GetInt("network.max_scan_threads", 64); }
    int         ScanTimeoutMs() { return g_cfg.GetInt("network.scan_timeout_ms", 2000); }
    std::string DefaultIP() { return g_cfg.GetStr("network.default_ip", "192.168.1.1"); }
    std::string SubnetMask() { return g_cfg.GetStr("network.subnet_mask", "24"); }
    int         MaxIPRange() { return g_cfg.GetInt("network.max_ip_range", 254); }
}

namespace DeployConfig {
    std::wstring AdminShare() { return g_cfg.GetWStr("deploy.admin_share", L"C$"); }
    int          CopyTimeoutMs() { return g_cfg.GetInt("deploy.copy_timeout_ms", 120000); }
    bool         OverwriteExist() { return g_cfg.GetBool("deploy.overwrite_existing", true); }
    bool         VerboseOutput() { return g_cfg.GetBool("deploy.verbose_output", true); }
}

namespace LogConfig {
    bool         EnableLogging() { return g_cfg.GetBool("log.enable", true); }
    std::wstring LogFile() { return g_cfg.GetWStr("log.file", L"deploy_log.txt"); }
    std::wstring LogPath() { return g_cfg.GetWStr("log.path", L".\\"); }
    bool         LogSuccess() { return g_cfg.GetBool("log.log_success", true); }
    bool         LogFailures() { return g_cfg.GetBool("log.log_failures", true); }
}

namespace ResultsConfig {
    bool         SortResults() { return g_cfg.GetBool("results.sort", true); }
    bool         ShowMAC() { return g_cfg.GetBool("results.show_mac_addresses", true); }
    bool         ExportResults() { return g_cfg.GetBool("results.export", true); }
    std::wstring ExportFile() { return g_cfg.GetWStr("results.export_file", L"scan_results.txt"); }
    std::wstring ExportPath() { return g_cfg.GetWStr("results.export_path", L".\\"); }
}

namespace ARPCacheConfig {
    bool         UseARPFallback() { return g_cfg.GetBool("arp.use_cache_fallback", true); }
    std::wstring ARPTempFile() { return g_cfg.GetWStr("arp.temp_file", L"_arp_tmp_.txt"); }
    std::wstring ARPCacheFilter() { return g_cfg.GetWStr("arp.cache_filter", L"dynamic"); }
}

namespace UIConfig {
    const std::wstring APP_TITLE = LR"(       _______ ____
      / / ___//  _/
 __  / /\__ \ / /  
/ /_/ /___/ // /   
\____//____/___/)";
    const std::wstring APP_SEPARATOR = L"========================================";
    const std::wstring APP_SUBSEPARATOR = L"----------------------------------------";
    const int          PROGRESS_BAR_WIDTH = 50;
}

namespace CredentialConfig {
    bool         UseCurrentUser() { return g_cfg.GetBool("credentials.use_current_user", false); }
    int          MaxPwdAttempts() { return g_cfg.GetInt("credentials.max_password_attempts", 3); }
    std::wstring DefaultUsername() { return g_cfg.GetWStr("credentials.default_username", L""); }
    std::wstring DefaultPassword() { return g_cfg.GetWStr("credentials.default_password", L""); }
    std::wstring DefaultDestDir() { return g_cfg.GetWStr("credentials.default_dest_dir", L"C:\\"); }
}

void WriteDefaultConfig(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "; JSI Deploy - configuration file\n"
        "; Lines starting with ; or # are comments\n"
        "; Boolean values: true/false, yes/no, 1/0\n"
        "\n"
        "[network]\n"
        "max_scan_threads  = 64\n"
        "scan_timeout_ms   = 2000\n"
        "default_ip        = 192.168.1.1\n"
        "subnet_mask       = 24\n"
        "max_ip_range      = 254\n"
        "\n"
        "[deploy]\n"
        "admin_share        = C$\n"
        "copy_timeout_ms    = 120000\n"
        "overwrite_existing = true\n"
        "verbose_output     = false\n"
        "\n"
        "[log]\n"
        "enable        = true\n"
        "file          = deploy_log.txt\n"
        "path          = .\\\n"
        "log_success   = true\n"
        "log_failures  = true\n"
        "\n"
        "[results]\n"
        "sort               = true\n"
        "show_mac_addresses = true\n"
        "export             = true\n"
        "export_file        = scan_results.txt\n"
        "export_path        = .\\\n"
        "\n"
        "[arp]\n"
        "use_cache_fallback = true\n"
        "temp_file          = _arp_tmp_.txt\n"
        "cache_filter       = dynamic\n"
        "\n"
        "[credentials]\n"
        "; Leave blank to prompt at runtime\n"
        "use_current_user       = false\n"
        "max_password_attempts  = 3\n"
        "default_username       = \n"
        "default_password       = \n"
        "default_dest_dir       = \n";
    f.close();
    std::wcout << L"[*] Default config written: " << to_wstring(path) << L"\n";
}

std::mutex       g_mutex;
std::atomic<int> g_scanned(0);

struct HostInfo {
    std::wstring ip;
    std::wstring mac;
};

void WriteLog(const std::wstring& message, bool isSuccess = true) {
    if (!LogConfig::EnableLogging())            return;
    if (!isSuccess && !LogConfig::LogFailures()) return;
    if (isSuccess && !LogConfig::LogSuccess())  return;

    std::wofstream log((LogConfig::LogPath() + LogConfig::LogFile()).c_str(), std::ios::app);
    if (!log.is_open()) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &time);
    timeStr[24] = '\0';

    std::wstring tw(timeStr, timeStr + strlen(timeStr));
    log << L"[" << tw << L"] " << message << std::endl;
    log.close();
}

std::string GetLocalIPAddress() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return "";

    struct sockaddr_in dest {};
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, "8.8.8.8", &dest.sin_addr);

    DWORD ifIdx = 0;
    if (GetBestInterfaceEx((sockaddr*)&dest, &ifIdx) != NO_ERROR) {
        WSACleanup(); return "";
    }

    ULONG bufLen = 0;
    GetAdaptersAddresses(AF_INET, 0, NULL, NULL, &bufLen);
    IP_ADAPTER_ADDRESSES* pAddr = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);

    std::string result;
    if (GetAdaptersAddresses(AF_INET, 0, NULL, pAddr, &bufLen) == NO_ERROR) {
        for (auto* p = pAddr; p; p = p->Next) {
            if (p->IfIndex != ifIdx && p->Ipv6IfIndex != ifIdx) continue;
            for (auto* u = p->FirstUnicastAddress; u; u = u->Next) {
                if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
                char buf[INET_ADDRSTRLEN];
                auto* sa = (sockaddr_in*)u->Address.lpSockaddr;
                inet_ntop(AF_INET, &sa->sin_addr, buf, INET_ADDRSTRLEN);
                result = buf;
            }
        }
    }
    free(pAddr);
    WSACleanup();
    return result;
}

void ArpWorker(const std::string& ipStr,
    const std::string& localIP,
    std::vector<HostInfo>& results)
{
    if (ipStr == localIP) { ++g_scanned; return; }

    IN_ADDR addr{};
    if (InetPtonA(AF_INET, ipStr.c_str(), &addr) != 1) { ++g_scanned; return; }

    unsigned char mac[6]{};
    unsigned long macLen = 6;
    DWORD ret = SendARP(addr.S_un.S_addr, 0, mac, &macLen);
    ++g_scanned;

    if (ret == NO_ERROR && macLen == 6) {
        wchar_t macStr[32];
        swprintf_s(macStr, L"%02X-%02X-%02X-%02X-%02X-%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        int len = MultiByteToWideChar(CP_ACP, 0, ipStr.c_str(), -1, nullptr, 0);
        std::wstring wip(len - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, ipStr.c_str(), -1, &wip[0], len);

        std::lock_guard<std::mutex> lk(g_mutex);
        results.push_back({ wip, macStr });
    }
}

static SHORT g_progressRow = -1;

void InitProgressLine(const std::string&) {
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    GetConsoleScreenBufferInfo(hCon, &csbi);
    g_progressRow = csbi.dwCursorPosition.Y;
    std::wcout << L"    Progress: 0/" << NetworkConfig::MaxIPRange() << L" (0%)\n";
    std::wcout.flush();
}

void UpdateProgressLine(int done, int total) {
    if (g_progressRow < 0) return;
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    GetConsoleScreenBufferInfo(hCon, &csbi);
    COORD cur = csbi.dwCursorPosition;

    COORD go{ 0, g_progressRow };
    SetConsoleCursorPosition(hCon, go);
    std::wcout << L"    Progress: " << done << L"/" << total
        << L" (" << (done * 100 / total) << L"%)    ";
    std::wcout.flush();
    SetConsoleCursorPosition(hCon, cur);
}

std::vector<HostInfo> ScanNetworkArp() {
    std::vector<HostInfo> results;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::wcout << L"[!] WSAStartup failed\n";
        return results;
    }

    std::string localIP = GetLocalIPAddress();
    size_t lastDot = localIP.find_last_of('.');
    if (lastDot == std::string::npos) {
        std::wcout << L"[!] Could not determine network base\n";
        WSACleanup();
        return results;
    }

    std::string baseIP = localIP.substr(0, lastDot + 1);
    g_scanned = 0;

    std::wcout << L"\n[*] Local IP  : " << to_wstring(localIP) << L"\n";
    std::wcout << L"[*] Scanning  : " << to_wstring(baseIP) << L"0/"
        << to_wstring(NetworkConfig::SubnetMask()) << L"  ("
        << NetworkConfig::MaxScanThreads() << L" threads)\n";

    InitProgressLine(baseIP);

    auto startTime = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    pool.reserve(NetworkConfig::MaxScanThreads());

    for (int i = 1; i <= NetworkConfig::MaxIPRange(); ++i) {
        std::string ip = baseIP + std::to_string(i);
        pool.emplace_back(ArpWorker, ip, localIP, std::ref(results));

        if ((int)pool.size() >= NetworkConfig::MaxScanThreads() ||
            i == NetworkConfig::MaxIPRange())
        {
            for (auto& t : pool) t.join();
            pool.clear();
            UpdateProgressLine(g_scanned.load(), NetworkConfig::MaxIPRange());
        }
    }

    UpdateProgressLine(NetworkConfig::MaxIPRange(), NetworkConfig::MaxIPRange());

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    std::wcout << L"\n[+] Scan complete in " << (elapsed / 1000.0) << L" s.  Found "
        << results.size() << L" host(s).\n";

    if (DeployConfig::VerboseOutput() && !results.empty()) {
        std::wcout << L"\n";
        for (const auto& h : results)
            std::wcout << L"    [+] " << std::setw(16) << std::left << h.ip << L" " << h.mac << L"\n";
    }

    if (ResultsConfig::SortResults()) {
        std::sort(results.begin(), results.end(),
            [](const HostInfo& a, const HostInfo& b) { return a.ip < b.ip; });
    }

    WSACleanup();
    return results;
}

std::vector<HostInfo> GetLocalIPsFromCache() {
    std::vector<HostInfo> ips;
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring tmpFile = std::wstring(tempPath) + ARPCacheConfig::ARPTempFile();

    _wsystem((L"arp -a > \"" + tmpFile + L"\"").c_str());

    std::wifstream file(tmpFile.c_str());
    std::wstring line;
    while (std::getline(file, line)) {
        if (line.find(ARPCacheConfig::ARPCacheFilter()) == std::wstring::npos) continue;
        std::wistringstream iss(line);
        std::wstring ip;
        if (iss >> ip && !ip.empty() && iswdigit(ip[0]))
            ips.push_back({ ip, L"(from cache)" });
    }
    file.close();
    DeleteFileW(tmpFile.c_str());
    return ips;
}

struct PathParts {
    std::wstring shareName;
    std::wstring remainder;
};

PathParts ParseDestPath(const std::wstring& destDir) {
    PathParts pp;
    if (destDir.size() >= 2 && iswalpha(destDir[0]) && destDir[1] == L':') {
        wchar_t dl = towupper(destDir[0]);
        pp.shareName = std::wstring(1, dl) + L"$";
        pp.remainder = destDir.substr(2);
        if (!pp.remainder.empty() && pp.remainder.back() != L'\\')
            pp.remainder += L'\\';
        if (pp.remainder.empty()) pp.remainder = L"\\";
    }
    else {
        pp.shareName = L"C$";
        pp.remainder = L"\\";
    }
    return pp;
}

std::wstring ReadLine() {
    std::wstring s;
    wchar_t ch;
    while (true) {
        ch = _getwch();
        if (ch == L'\r' || ch == L'\n') break;
        if (ch == 3) exit(0);
        if (ch == L'\b') {
            if (!s.empty()) { s.pop_back(); std::wcout << L"\b \b"; }
        }
        else { s += ch; std::wcout << ch; }
    }
    std::wcout << L"\n";
    return s;
}

std::wstring ReadPassword() {
    std::wstring pw;
    wchar_t ch;
    while (true) {
        ch = _getwch();
        if (ch == L'\r' || ch == L'\n') break;
        if (ch == 3) exit(0);
        if (ch == L'\b') {
            if (!pw.empty()) { pw.pop_back(); std::wcout << L"\b \b"; }
        }
        else { pw += ch; std::wcout << L'*'; }
    }
    std::wcout << L"\n";
    return pw;
}

std::vector<HostInfo> SelectTargetHosts(const std::vector<HostInfo>& hosts) {
    std::wcout << L"\n" << UIConfig::APP_SEPARATOR << L"\n";
    std::wcout << L"  Select targets for deployment\n";
    std::wcout << UIConfig::APP_SEPARATOR << L"\n";
    std::wcout << L"  [A] All hosts (" << hosts.size() << L")\n";
    std::wcout << L"  [S] Select manually\n";
    std::wcout << L"  [R] Select range\n";
    std::wcout << L"Choice: ";

    wchar_t mode = towupper(_getwch());
    std::wcout << mode << L"\n\n";

    if (mode == L'A') {
        std::wcout << L"[*] Selected all " << hosts.size() << L" hosts.\n";
        return hosts;
    }

    auto printList = [&]() {
        for (size_t i = 0; i < hosts.size(); ++i)
            std::wcout << L"  [" << (i + 1) << L"] " << std::setw(16) << std::left
            << hosts[i].ip << L" "
            << (ResultsConfig::ShowMAC() ? hosts[i].mac : L"") << L"\n";
        };

    if (mode == L'S') {
        std::wcout << L"Available hosts:\n";
        printList();
        std::wcout << L"\nEnter numbers (e.g. 1 3 5 or 1,3,5):\n> ";

        std::wstring line = ReadLine();
        for (auto& c : line) if (c == L',') c = L' ';

        std::wistringstream iss(line);
        std::set<size_t> idx;
        size_t n;
        while (iss >> n) {
            if (n >= 1 && n <= hosts.size()) idx.insert(n - 1);
            else std::wcout << L"[!] Skipped invalid: " << n << L"\n";
        }

        std::vector<HostInfo> sel;
        for (size_t i : idx) sel.push_back(hosts[i]);
        if (sel.empty()) { std::wcout << L"[!] Nothing selected.\n"; return {}; }

        std::wcout << L"\n[*] Selected " << sel.size() << L" host(s):\n";
        for (const auto& h : sel)
            std::wcout << L"    " << std::setw(16) << std::left << h.ip << L" "
            << (ResultsConfig::ShowMAC() ? h.mac : L"") << L"\n";
        return sel;
    }

    if (mode == L'R') {
        std::wcout << L"Available hosts:\n";
        printList();
        std::wcout << L"\nEnter range (e.g. 2 8):\n> ";

        std::wstring line = ReadLine();
        for (auto& c : line) if (c == L',') c = L' ';

        std::wistringstream rss(line);
        size_t from = 0, to = 0;
        if (!(rss >> from >> to)) { std::wcout << L"[!] Invalid input.\n"; return {}; }

        if (from < 1) from = 1;
        if (to > hosts.size()) to = hosts.size();
        if (from > to) std::swap(from, to);

        std::vector<HostInfo> sel(hosts.begin() + (from - 1), hosts.begin() + to);
        std::wcout << L"\n[*] Selected " << sel.size() << L" host(s) [" << from << L".." << to << L"]:\n";
        for (const auto& h : sel)
            std::wcout << L"    " << std::setw(16) << std::left << h.ip << L" "
            << (ResultsConfig::ShowMAC() ? h.mac : L"") << L"\n";
        return sel;
    }

    std::wcout << L"[!] Unknown choice. Deploying to all hosts.\n";
    return hosts;
}

std::wstring BrowseForFileOrFolder(bool folderOnly = false) {
    IFileDialog* pDlg = nullptr;
    std::wstring result;

    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
        IID_IFileDialog, reinterpret_cast<void**>(&pDlg))))
        return result;

    DWORD opts = 0;
    pDlg->GetOptions(&opts);
    pDlg->SetOptions(opts | (folderOnly ? FOS_PICKFOLDERS : 0) | FOS_FORCEFILESYSTEM);
    pDlg->SetTitle(L"Select file or folder to deploy");

    if (SUCCEEDED(pDlg->Show(nullptr))) {
        IShellItem* pItem = nullptr;
        if (SUCCEEDED(pDlg->GetResult(&pItem))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = path;
                CoTaskMemFree(path);
            }
            pItem->Release();
        }
    }
    pDlg->Release();
    return result;
}

bool CopyFolderRemote(const std::wstring& src,
    const std::wstring& dstBase,
    const std::wstring& folderName)
{
    std::wstring params = L"/C xcopy \"" + src + L"\" \""
        + dstBase + folderName + L"\" /E /I /H /Y /Q";

    if (!DeployConfig::OverwriteExist()) {
        size_t pos = params.find(L" /Y");
        if (pos != std::wstring::npos) params.erase(pos, 3);
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) return false;

    DWORD code = 0;
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, DeployConfig::CopyTimeoutMs());
        GetExitCodeProcess(sei.hProcess, &code);
        CloseHandle(sei.hProcess);
    }
    return code == 0 || code == 1;
}

DWORD ConnectToShare(const std::wstring& unc,
    const std::wstring& user,
    const std::wstring& pass)
{
    WNetCancelConnection2W(unc.c_str(), 0, TRUE);

    NETRESOURCEW nr{};
    nr.dwType = RESOURCETYPE_DISK;
    nr.lpRemoteName = const_cast<LPWSTR>(unc.c_str());

    return WNetAddConnection2W(&nr, pass.c_str(), user.c_str(), CONNECT_TEMPORARY);
}

DWORD RunCommand(const std::wstring& cmd, DWORD timeoutMs = 30000) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};

    std::wstring fullCmd = L"cmd.exe /C " + cmd;
    std::vector<wchar_t> buf(fullCmd.begin(), fullCmd.end());
    buf.push_back(0);

    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr,
        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return 0xFFFFFFFF;

    WaitForSingleObject(pi.hProcess, timeoutMs);

    DWORD code = 0xFFFFFFFF;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

bool ExecuteRemoteViaSc(const std::wstring& ip,
    const std::wstring& remoteExePath)
{
    std::wstring svcName = L"_jsi_tmp_";

    RunCommand(L"sc \\\\" + ip + L" delete " + svcName, 5000);
    Sleep(800);

    std::wstring createCmd = L"sc \\\\" + ip
        + L" create " + svcName
        + L" binPath= \"\\\"" + remoteExePath + L"\\\"\""
        + L" start= demand type= interact type= own";

    DWORD cr = RunCommand(createCmd, 15000);
    if (cr != 0) {
        std::wcout << L"\n   sc create exit: " << cr << L"\n    ";
        return false;
    }
    Sleep(500);

    DWORD sr = RunCommand(L"sc \\\\" + ip + L" start " + svcName, 15000);
    Sleep(2000);

    RunCommand(L"sc \\\\" + ip + L" delete " + svcName, 5000);

    return sr == 0;
}

void ExportResults(const std::vector<HostInfo>& hosts,
    const std::vector<HostInfo>& targets,
    int success, int failed)
{
    if (!ResultsConfig::ExportResults()) return;

    std::wofstream file((ResultsConfig::ExportPath() + ResultsConfig::ExportFile()).c_str());
    if (!file.is_open()) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char ts[26]; ctime_s(ts, sizeof(ts), &time); ts[24] = '\0';

    file << L"=== Deployment Results ===" << std::endl;
    file << L"Date: " << std::wstring(ts, ts + strlen(ts)) << std::endl;
    file << L"Total hosts discovered: " << hosts.size() << std::endl;
    file << L"Targets selected: " << targets.size() << std::endl;
    file << L"Successful: " << success << std::endl;
    file << L"Failed: " << failed << std::endl;
    file << std::endl;
    file << L"=== Discovered Hosts ===" << std::endl;
    for (const auto& h : hosts)
        file << h.ip << L"\t" << h.mac << std::endl;
    file.close();
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::wcout << UIConfig::APP_TITLE << L"\n";

    std::string cfgPath = "deploy.ini";
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            cfgPath = argv[i + 1];
            break;
        }
    }

    bool cfgLoaded = g_cfg.Load(cfgPath);

    {
        std::ifstream probe(cfgPath);
        if (!probe.is_open()) {
            std::wcout << L"[*] Config file not found: " << to_wstring(cfgPath) << L"\n";
            WriteDefaultConfig(cfgPath);
        }
        else if (cfgLoaded) {
            std::wcout << L"[*] Config loaded: " << to_wstring(cfgPath) << L"\n\n";
        }
        else {
            std::wcout << L"[!] Could not load config: " << to_wstring(cfgPath)
                << L" — using built-in defaults.\n\n";
        }
    }

    WriteLog(L"Application started", true);

    std::vector<HostInfo> hosts = ScanNetworkArp();

    if (hosts.empty() && ARPCacheConfig::UseARPFallback()) {
        std::wcout << L"[*] Active scan found nothing. Trying ARP cache...\n";
        hosts = GetLocalIPsFromCache();
    }

    if (hosts.empty()) {
        std::wcout << L"[!] No hosts found. Exiting.\n";
        WriteLog(L"No hosts found, exiting", false);
        CoUninitialize(); return 1;
    }

    std::wcout << L"\n[+] Hosts found: " << hosts.size() << L"\n";
    for (size_t i = 0; i < hosts.size(); ++i)
        std::wcout << L"    [" << (i + 1) << L"] " << std::setw(16) << std::left
        << hosts[i].ip << L" "
        << (ResultsConfig::ShowMAC() ? hosts[i].mac : L"") << L"\n";

    std::vector<HostInfo> targets = SelectTargetHosts(hosts);
    if (targets.empty()) {
        WriteLog(L"No targets selected, exiting", false);
        CoUninitialize(); return 1;
    }

    std::wcout << L"\n[*] Deploy what?\n    [1] File\n    [2] Folder\nChoice: ";
    wchar_t choice = _getwch();
    std::wcout << choice << L"\n\n";
    bool isFolder = (choice == L'2');

    std::wstring selectedPath = BrowseForFileOrFolder(isFolder);
    if (selectedPath.empty()) {
        std::wcout << L"[!] Nothing selected. Exiting.\n";
        WriteLog(L"No file/folder selected, exiting", false);
        CoUninitialize(); return 1;
    }
    std::wcout << L"[+] Selected: " << selectedPath << L"\n\n";

    size_t sep = selectedPath.find_last_of(L"\\/");
    std::wstring itemName = (sep != std::wstring::npos)
        ? selectedPath.substr(sep + 1)
        : selectedPath;

    bool doLaunch = false;
    if (!isFolder) {
        std::wcout << L"[*] Launch after deploy?\n    [1] Yes\n    [2] No\nChoice: ";
        wchar_t lc = _getwch();
        std::wcout << lc << L"\n\n";
        doLaunch = (lc == L'1');
    }

    std::wstring username = CredentialConfig::DefaultUsername();
    std::wstring password = CredentialConfig::DefaultPassword();

    if (username.empty()) {
        std::wcout << L"[*] Credentials\n    Username: ";
        username = ReadLine();
    }
    else {
        std::wcout << L"[*] Username from config: " << username << L"\n";
    }

    if (password.empty()) {
        std::wcout << L"    Password: ";
        password = ReadPassword();
        std::wcout << L"\n";
    }
    else {
        std::wcout << L"    Password: (from config)\n\n";
    }

    std::wstring destDir = CredentialConfig::DefaultDestDir();
    std::wcout << L"[*] Destination directory on target (default: " << destDir << L"): ";
    std::wstring userDestDir = ReadLine();
    if (!userDestDir.empty()) destDir = userDestDir;
    if (destDir.back() != L'\\') destDir += L'\\';

    PathParts pp = ParseDestPath(destDir);
    std::wcout << L"[+] Destination : " << destDir << L"  (share: " << pp.shareName << L")\n\n";
    WriteLog(L"Destination: " + destDir, true);

    std::wcout << L"[*] Deploying to " << targets.size() << L" host(s)...\n\n";
    int ok = 0, fail = 0;

    for (const auto& host : targets) {
        std::wstring unc = L"\\\\" + host.ip + L"\\" + pp.shareName;
        std::wstring dstBase = unc + pp.remainder;
        std::wstring dest = dstBase + itemName;

        std::wcout << L"[>] " << std::setw(16) << std::left << host.ip << L" ";

        if (ConnectToShare(unc, username, password) != NO_ERROR) {
            DWORD err = GetLastError();
            std::wcout << L"[X] Connection failed (error " << err << L")\n";
            WriteLog(L"Failed to connect to " + host.ip
                + L" error: " + std::to_wstring(err), false);
            ++fail; continue;
        }

        if (GetFileAttributesW(unc.c_str()) == INVALID_FILE_ATTRIBUTES) {
            std::wcout << L"[X] Share inaccessible\n";
            WriteLog(L"Share inaccessible on " + host.ip, false);
            WNetCancelConnection2W(unc.c_str(), 0, TRUE);
            ++fail; continue;
        }

        bool copied = isFolder
            ? CopyFolderRemote(selectedPath, dstBase, itemName)
            : (CopyFileW(selectedPath.c_str(), dest.c_str(),
                !DeployConfig::OverwriteExist()) == TRUE);

        if (!copied) {
            DWORD err = GetLastError();
            std::wcout << L"[X] Copy failed (error " << err << L")\n";
            WriteLog(L"Copy failed on " + host.ip
                + L" error: " + std::to_wstring(err), false);
            WNetCancelConnection2W(unc.c_str(), 0, TRUE);
            ++fail; continue;
        }

        std::wcout << L"[V] OK\n";
        WriteLog(L"Successfully deployed to " + host.ip, true);

        if (doLaunch) {
            std::wstring remoteExe = destDir + itemName;
            std::wcout << L"    [~] Launching " << remoteExe << L" ... ";

            if (ExecuteRemoteViaSc(host.ip, remoteExe)) {
                std::wcout << L"[V] Started\n";
                WriteLog(L"Successfully launched on " + host.ip, true);
            }
            else {
                std::wcout << L"[X] Launch failed\n";
                WriteLog(L"Launch failed on " + host.ip, false);
            }
        }

        WNetCancelConnection2W(unc.c_str(), 0, TRUE);
        ++ok;
    }

    ExportResults(hosts, targets, ok, fail);

    std::wcout << L"\n" << UIConfig::APP_SEPARATOR << L"\n";
    std::wcout << L"  Done!  OK: " << ok << L"  |  Failed: " << fail << L"\n";
    std::wcout << UIConfig::APP_SEPARATOR << L"\n";

    WriteLog(L"Deployment completed. Success: " + std::to_wstring(ok)
        + L" Failed: " + std::to_wstring(fail), true);

    CoUninitialize();
    return 0;
}
