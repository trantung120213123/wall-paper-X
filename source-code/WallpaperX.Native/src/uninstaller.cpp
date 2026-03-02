#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

constexpr wchar_t kInstallFolderName[] = L"WallpaperXNative";
constexpr wchar_t kRunValueName[] = L"WallpaperXNative";
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr int IDI_APP_ICON = 101;

bool IsRunningAsAdministrator() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

bool RelaunchSelfAsAdministrator() {
    std::wstring exePath(1024, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    while (len == exePath.size()) {
        exePath.resize(exePath.size() * 2);
        len = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    }
    exePath.resize(len);

    const HINSTANCE result = ShellExecuteW(nullptr, L"runas", exePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

std::wstring GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR raw = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &raw)) && raw) {
        out = raw;
    }
    CoTaskMemFree(raw);
    return out;
}

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return value;
}

bool StartsWithPathCaseInsensitive(const std::wstring& path, const std::wstring& prefix) {
    const std::wstring lhs = ToLower(path);
    std::wstring rhs = ToLower(prefix);
    if (!rhs.empty() && rhs.back() != L'\\') {
        rhs.push_back(L'\\');
    }
    if (lhs == ToLower(prefix)) {
        return true;
    }
    if (lhs.size() < rhs.size()) {
        return false;
    }
    return lhs.compare(0, rhs.size(), rhs) == 0;
}

std::wstring GetDefaultInstallDir() {
    auto base = GetKnownFolderPath(FOLDERID_ProgramFiles);
    if (base.empty()) {
        base = GetKnownFolderPath(FOLDERID_ProgramFilesX86);
    }
    return (fs::path(base) / kInstallFolderName).wstring();
}

std::wstring GetModulePath() {
    std::wstring exePath(1024, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    while (len == exePath.size()) {
        exePath.resize(exePath.size() * 2);
        len = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    }
    exePath.resize(len);
    return exePath;
}

std::wstring GetInstallDirFromSelf() {
    std::error_code ec;
    const fs::path self = fs::path(GetModulePath());
    const fs::path parent = self.parent_path();
    if (!parent.empty() && parent.filename() == kInstallFolderName) {
        return parent.wstring();
    }
    return GetDefaultInstallDir();
}

std::wstring GetProcessImagePath(HANDLE process) {
    std::wstring path(1024, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    while (true) {
        if (QueryFullProcessImageNameW(process, 0, path.data(), &size) != 0) {
            path.resize(size);
            return path;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return L"";
        }
        path.resize(path.size() * 2);
        size = static_cast<DWORD>(path.size());
    }
}

void TerminateProcessesInInstallDir(const std::wstring& installDir) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    const DWORD selfPid = GetCurrentProcessId();
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == 0 || entry.th32ProcessID == selfPid) {
                continue;
            }

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE,
                                         FALSE,
                                         entry.th32ProcessID);
            if (!process) {
                continue;
            }

            const std::wstring image = GetProcessImagePath(process);
            if (!image.empty() && StartsWithPathCaseInsensitive(image, installDir)) {
                TerminateProcess(process, 0);
                WaitForSingleObject(process, 5000);
            }
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

void RemoveDesktopShortcut() {
    const auto desktop = GetKnownFolderPath(FOLDERID_Desktop);
    if (desktop.empty()) {
        return;
    }

    std::error_code ec;
    fs::remove(fs::path(desktop) / L"WallpaperX Editor.lnk", ec);
}

void RemoveContextMenu() {
    const std::vector<std::wstring> extensions = {L".mp4", L".mkv", L".webm", L".mov", L".avi", L".m4v"};

    for (const auto& ext : extensions) {
        const std::wstring keyPath = L"Software\\Classes\\SystemFileAssociations\\" + ext +
                                     L"\\shell\\WallpaperXImport";
        RegDeleteTreeW(HKEY_CURRENT_USER, keyPath.c_str());
    }
}

void RemoveRunAtStartup() {
    HKEY runKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        kRunKeyPath,
                        0,
                        nullptr,
                        0,
                        KEY_SET_VALUE,
                        nullptr,
                        &runKey,
                        nullptr) == ERROR_SUCCESS) {
        RegDeleteValueW(runKey, kRunValueName);
        RegCloseKey(runKey);
    }
}

void RemoveInstallDirNow(const fs::path& installDir) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(installDir, ec);
         !ec && it != fs::recursive_directory_iterator();
         ++it) {
        SetFileAttributesW(it->path().wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
    }
    SetFileAttributesW(installDir.wstring().c_str(), FILE_ATTRIBUTE_DIRECTORY);

    ec.clear();
    fs::remove_all(installDir, ec);
}

void ScheduleDeferredDelete(const std::wstring& installDir) {
    std::wstring command =
        L"/c ping 127.0.0.1 -n 3 >nul & attrib -r -s -h \"" + installDir +
        L"\" /s /d >nul 2>nul & rmdir /s /q \"" + installDir + L"\"";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring cmdExe = L"C:\\Windows\\System32\\cmd.exe";
    if (CreateProcessW(cmdExe.data(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

void MaybeRemoveUserData(int argc, wchar_t** argv) {
    bool removeUserData = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--remove-user-data") == 0 || _wcsicmp(argv[i], L"/remove-user-data") == 0) {
            removeUserData = true;
        }
    }

    if (!removeUserData) {
        return;
    }

    const auto localData = GetKnownFolderPath(FOLDERID_LocalAppData);
    if (localData.empty()) {
        return;
    }

    std::error_code ec;
    fs::remove_all(fs::path(localData) / kInstallFolderName, ec);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    if (!IsRunningAsAdministrator()) {
        if (RelaunchSelfAsAdministrator()) {
            return 0;
        }
        MessageBoxW(nullptr,
                    L"Uninstaller needs Administrator permission to remove Program Files content.",
                    L"WallpaperX Uninstaller",
                    MB_ICONERROR);
        return 1;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    const std::wstring installDir = GetInstallDirFromSelf();
    if (installDir.empty()) {
        MessageBoxW(nullptr, L"Cannot determine install directory.", L"WallpaperX Uninstaller", MB_ICONERROR);
        if (argv) {
            LocalFree(argv);
        }
        return 1;
    }

    const int confirm = MessageBoxW(nullptr,
                                    L"This will close WallpaperX processes and uninstall WallpaperX Native. Continue?",
                                    L"WallpaperX Uninstaller",
                                    MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
    if (confirm != IDYES) {
        if (argv) {
            LocalFree(argv);
        }
        return 0;
    }

    TerminateProcessesInInstallDir(installDir);
    RemoveDesktopShortcut();
    RemoveContextMenu();
    RemoveRunAtStartup();
    MaybeRemoveUserData(argc, argv ? argv : nullptr);

    const fs::path installPath = installDir;
    if (fs::exists(installPath)) {
        RemoveInstallDirNow(installPath);
    }

    if (fs::exists(installPath)) {
        ScheduleDeferredDelete(installDir);
        MessageBoxW(nullptr,
                    L"Uninstall is finishing in the background. The install folder will be removed in a moment.",
                    L"WallpaperX Uninstaller",
                    MB_ICONINFORMATION);
    } else {
        MessageBoxW(nullptr, L"WallpaperX Native removed.", L"WallpaperX Uninstaller", MB_ICONINFORMATION);
    }

    if (argv) {
        LocalFree(argv);
    }
    return 0;
}
