// StressTestApp - Stress test 1: "Many subscribers in short time"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>

struct Config {
    int subscribers = 60;
    int startDelayMs = 30;
    int quizId = 1;

    std::wstring subscriberExe ;
    bool startSystem = false;
    std::wstring serverExe ;
    std::wstring serviceExe ;
    std::wstring publisherExe;
};

static void wlog(const std::wstring& s) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::wcout
        << L"[" << st.wHour << L":" << st.wMinute << L":" << st.wSecond << L"." << st.wMilliseconds << L"] "
        << s << L"\n";
}

static std::wstring makeCmd(const std::wstring& exe, const std::wstring& args) {
    std::wstringstream ss;
    ss << L"\"" << exe << L"\"";
    if (!args.empty()) ss << L" " << args;
    return ss.str();
}

static std::wstring getFullPath(const std::wstring& path) {
    wchar_t wd[MAX_PATH]{};
    GetCurrentDirectoryW(MAX_PATH, wd);  // npr. ...\quiz\StressTestApp
    std::wstring parentDir = std::wstring(wd) + L"\\.."; // ide jedan folder unazad (…\quiz)

    wchar_t out[MAX_PATH]{};
    DWORD n = GetFullPathNameW((parentDir + L"\\" + path).c_str(),
        MAX_PATH, out, nullptr);
    if (n == 0 || n >= MAX_PATH) return path;
    return std::wstring(out);
}

static std::wstring winErrMsg(DWORD err) {
    wchar_t buf[512]{};
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

    DWORD n = FormatMessageW(flags, nullptr, err, langId, buf, (DWORD)std::size(buf), nullptr);
    std::wstring msg = (n > 0) ? std::wstring(buf, n) : L"Unknown error";

    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n' || msg.back() == L' ')) msg.pop_back();
    return msg;
}

//static bool startProcessWithInjectedStdin(
//    const std::wstring& exePath,
//    const std::wstring& args,
//    const std::string& stdinText,
//    PROCESS_INFORMATION& outPi
//) {
//    SECURITY_ATTRIBUTES sa{};
//    sa.nLength = sizeof(sa);
//    sa.bInheritHandle = TRUE;
//    sa.lpSecurityDescriptor = NULL;
//
//    HANDLE hRead = NULL, hWrite = NULL;
//    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
//        DWORD err = GetLastError();
//        wlog(L"CreatePipe failed. err=" + std::to_wstring(err) + L" msg=" + winErrMsg(err));
//        return false;
//    }
//
//    SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);
//
//    STARTUPINFOW si{};
//    si.cb = sizeof(si);
//    si.dwFlags = STARTF_USESTDHANDLES;
//    si.hStdInput = hRead;
//    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
//    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
//
//    PROCESS_INFORMATION pi{};
//
//    std::wstring cmd = makeCmd(exePath, args);
//    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
//    cmdBuf.push_back(L'\0');
//
//    BOOL ok = CreateProcessW(
//        NULL,
//        cmdBuf.data(),      // mutable LPWSTR
//        NULL,
//        NULL,
//        TRUE,
//        CREATE_NEW_CONSOLE,
//        NULL,
//        NULL,
//        &si,
//        &pi
//    );
//
//    CloseHandle(hRead);
//
//    if (!ok) {
//        DWORD err = GetLastError();
//        wlog(L"CreateProcess(subscriber) failed. err=" + std::to_wstring(err) + L" msg=" + winErrMsg(err));
//        wlog(L"Tried exe(fullpath): " + getFullPath(exePath));
//        CloseHandle(hWrite);
//        return false;
//    }
//
//    DWORD written = 0;
//    if (!WriteFile(hWrite, stdinText.c_str(), (DWORD)stdinText.size(), &written, NULL)) {
//        DWORD err = GetLastError();
//        wlog(L"WriteFile(stdin) failed. err=" + std::to_wstring(err) + L" msg=" + winErrMsg(err));
//    }
//
//    CloseHandle(hWrite);
//
//    outPi = pi;
//    return true;
//}

static bool startProcessNewConsole(const std::wstring& exePath, const std::wstring& args, PROCESS_INFORMATION* outPiOpt = nullptr) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring cmd = makeCmd(exePath, args);
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        NULL,
        cmdBuf.data(),
        NULL, NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL, NULL,
        &si, &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        wlog(L"CreateProcess failed. err=" + std::to_wstring(err) + L" msg=" + winErrMsg(err));
        wlog(L"Tried exe(fullpath): " + getFullPath(exePath));
        return false;
    }

    if (outPiOpt) {
        *outPiOpt = pi; // caller will close handles
    }
    else {
        // We don't need the handles here; close immediately
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return true;
}
static Config parseArgs(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--n" && i + 1 < argc) cfg.subscribers = std::atoi(argv[++i]);
        else if (a == "--delay" && i + 1 < argc) cfg.startDelayMs = std::atoi(argv[++i]);
        else if (a == "--quiz" && i + 1 < argc) cfg.quizId = std::atoi(argv[++i]);
        else if (a == "--start-system") cfg.startSystem = true;
        // (optional) leave out path overriding for now to keep it simple/clean
    }
    return cfg;
}

int main(int argc, char** argv) {
    // make sure wide output works nicely
    std::ios::sync_with_stdio(false);

    Config cfg = parseArgs(argc, argv);
    cfg.subscriberExe = getFullPath(L"x64\\Debug\\SubscriberApp.exe");
    cfg.serverExe = getFullPath(L"x64\\Debug\\ServerApp.exe");
    cfg.serviceExe = getFullPath(L"x64\\Debug\\ServiceApp.exe");
    cfg.publisherExe = getFullPath(L"x64\\Debug\\PublisherApp.exe");

    wchar_t wd[MAX_PATH]{};
    GetCurrentDirectoryW(MAX_PATH, wd);
    wlog(L"WorkingDir: " + std::wstring(wd));
    wlog(L"SubscriberExe(fullpath): " + cfg.subscriberExe);
    wlog(L"SubscriberExe(fullpath): " + cfg.serverExe);
    wlog(L"SubscriberExe(fullpath): " + cfg.serviceExe);

    wlog(L"StressTest 1: Many subscribers in short time");
    wlog(L"N=" + std::to_wstring(cfg.subscribers) +
        L" delayMs=" + std::to_wstring(cfg.startDelayMs) +
        L" quizId=" + std::to_wstring(cfg.quizId));

    if (cfg.startSystem) {
        wlog(L"Starting Server/Service/Publisher...");
        if (!startProcessNewConsole(cfg.serverExe, L""))    wlog(L"WARN: failed to start Server.");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!startProcessNewConsole(cfg.serviceExe, L""))   wlog(L"WARN: failed to start Service.");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!startProcessNewConsole(cfg.publisherExe, L"")) wlog(L"WARN: failed to start Publisher.");
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
    else {
        wlog(L"NOTE: Start Server/Service/Publisher manually (or run with --start-system).");
    }

    int okCount = 0;
    std::vector<PROCESS_INFORMATION> procs;
    procs.reserve(cfg.subscribers);

    std::string stdinText = std::to_string(cfg.quizId) + "\n";

    for (int i = 1; i <= cfg.subscribers; i++) {
        std::wstring args = std::to_wstring(i);

        PROCESS_INFORMATION pi{};
        if (startProcessNewConsole(cfg.subscriberExe, args, &pi)) {
            okCount++;
            procs.push_back(pi);
        }

        if (cfg.startDelayMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.startDelayMs));

        Sleep(1000);
    }

    wlog(L"Spawned subscribers: " + std::to_wstring(okCount) + L"/" + std::to_wstring(cfg.subscribers));
    wlog(L"Take screenshots now: Server + (optional) Service + 1-2 Subscribers.");
    wlog(L"Press ENTER to exit (children keep running).");

    /*std::wstring dummy;
    std::getline(std::wcin, dummy);*/

    for (auto& pi : procs) {
        TerminateProcess(pi.hProcess,0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return 0;
}
