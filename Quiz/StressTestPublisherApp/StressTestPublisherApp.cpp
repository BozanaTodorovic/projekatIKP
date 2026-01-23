// StressTestPublisherApp.cpp
// Stress test 2: "Publisher spam / multiple quizzes in parallel"
// Goal: start many PublisherApp instances quickly and verify server handles concurrent CREATE_QUIZ correctly.
//
// Usage examples (set in Project -> Properties -> Debugging -> Command Arguments):
//   --n 8 --delay 20
//   --n 10 --delay 5
//
// Notes:
// - This app does NOT modify your existing project code.
// - It simply spawns multiple PublisherApp.exe processes (new consoles).
// - Make sure ServerApp and ServiceApp are running before starting this test,
//   unless you run with --start-system (optional).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>

struct Config {
    int publishers = 60;        // N instances
    int startDelayMs = 50;     // delay between spawns
    bool startSystem = false;  // optionally start Server/Service first

    // Paths relative to StressTestPublisherApp.exe working directory (usually .\x64\Debug\)
    std::wstring publisherExe;
    std::wstring serverExe;
    std::wstring serviceExe;
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
        if (a == "--n" && i + 1 < argc) cfg.publishers = std::atoi(argv[++i]);
        else if (a == "--delay" && i + 1 < argc) cfg.startDelayMs = std::atoi(argv[++i]);
        else if (a == "--start-system") cfg.startSystem = true;
    }
    return cfg;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);

    Config cfg = parseArgs(argc, argv);

    wchar_t wd[MAX_PATH]{};
    GetCurrentDirectoryW(MAX_PATH, wd);
    cfg.publisherExe = getFullPath(L"x64\\Debug\\PublisherApp.exe");
    cfg.serviceExe = getFullPath(L"x64\\Debug\\ServiceApp.exe");
    cfg.serverExe = getFullPath(L"x64\\Debug\\ServerApp.exe");
    wlog(L"StressTest 2: Publisher spam / multiple quizzes");
    wlog(L"WorkingDir: " + std::wstring(wd));
    wlog(L"PublisherExe(fullpath): " + cfg.publisherExe);
    wlog(L"N=" + std::to_wstring(cfg.publishers) + L" delayMs=" + std::to_wstring(cfg.startDelayMs));

    if (cfg.startSystem) {
        wlog(L"Starting Server + Service...");
        if (!startProcessNewConsole(cfg.serverExe, L""))  wlog(L"WARN: failed to start Server.");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!startProcessNewConsole(cfg.serviceExe, L"")) wlog(L"WARN: failed to start Service.");
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
    else {
        wlog(L"NOTE: Start ServerApp and ServiceApp manually (recommended).");
    }

    int okCount = 0;
    std::vector<PROCESS_INFORMATION> procs;
    procs.reserve(cfg.publishers);

    // Spawn many publishers fast. If PublisherApp supports args, you can pass an id, but it's optional.
    for (int i = 1; i <= cfg.publishers; i++) {
        // If your PublisherApp ignores args, that's fine—each instance still sends CREATE_QUIZ.
        std::wstring args = std::to_wstring(i);

        PROCESS_INFORMATION pi{};
        if (startProcessNewConsole(cfg.publisherExe, args, &pi)) {
            okCount++;
            procs.push_back(pi);
        }

        if (cfg.startDelayMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg.startDelayMs));

        Sleep(1000);
    }
    wlog(L"Spawned publishers: " + std::to_wstring(okCount) + L"/" + std::to_wstring(cfg.publishers));
    wlog(L"Now check Server console: it should print multiple 'Quiz X created'.");
    wlog(L"Take screenshots: Server + 2-3 Publisher consoles.");
    wlog(L"Press ENTER to exit (children keep running).");
    //Sleep(5000);
    //for (int i = 1; i <=100; i++) {
    //    // If your PublisherApp ignores args, that's fine—each instance still sends CREATE_QUIZ.

    //    TerminateProcess(array[i].hProcess, 0);
    //}

    /*std::wstring dummy;
    std::getline(std::wcin, dummy);*/

    // Close only handles (do not terminate publisher processes)
    for (auto& pi : procs) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    return 0;
}
