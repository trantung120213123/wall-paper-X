#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "generated_payload.h"

namespace fs = std::filesystem;

constexpr wchar_t kInstallFolderName[] = L"WallpaperXNative";
constexpr wchar_t kEditorExeName[] = L"WallpaperXEditor.exe";
constexpr wchar_t kStartupExeName[] = L"WallpaperXStartup.exe";
constexpr wchar_t kRunValueName[] = L"WallpaperXNative";
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kWindowClassName[] = L"WallpaperXNativeInstaller.Window";
constexpr int IDI_APP_ICON = 101;

constexpr int IDC_PATH = 2001;
constexpr int IDC_BROWSE = 2002;
constexpr int IDC_AUTOSTART = 2003;
constexpr int IDC_INSTALL = 2004;
constexpr int IDC_CANCEL = 2005;
constexpr int IDC_PROGRESS = 2006;
constexpr int IDC_STATUS = 2007;

struct InstallerState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND pathEdit = nullptr;
    HWND browseButton = nullptr;
    HWND autoStartCheckbox = nullptr;
    HWND installButton = nullptr;
    HWND cancelButton = nullptr;
    HWND progressBar = nullptr;
    HWND statusLabel = nullptr;
    bool installing = false;
};

static InstallerState g_state;

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

std::wstring GetDefaultInstallRoot() {
    auto base = GetKnownFolderPath(FOLDERID_ProgramFiles);
    if (base.empty()) {
        base = GetKnownFolderPath(FOLDERID_ProgramFilesX86);
    }
    return (fs::path(base) / kInstallFolderName).wstring();
}

void SetStatus(const std::wstring& text) {
    if (g_state.statusLabel) {
        SetWindowTextW(g_state.statusLabel, text.c_str());
        UpdateWindow(g_state.statusLabel);
    }
}

void SetInstallProgress(int position) {
    if (g_state.progressBar) {
        SendMessageW(g_state.progressBar, PBM_SETPOS, static_cast<WPARAM>(position), 0);
    }
}

std::wstring GetInstallPathFromUi() {
    const int length = GetWindowTextLengthW(g_state.pathEdit);
    if (length <= 0) {
        return L"";
    }
    std::wstring path(length + 1, L'\0');
    GetWindowTextW(g_state.pathEdit, path.data(), static_cast<int>(path.size()));
    path.resize(length);
    return path;
}

bool SelectInstallFolder(std::wstring& folderPath) {
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return false;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Choose install folder");

    if (!folderPath.empty()) {
        IShellItem* currentFolder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(folderPath.c_str(), nullptr, IID_PPV_ARGS(&currentFolder)))) {
            dialog->SetFolder(currentFolder);
            currentFolder->Release();
        }
    }

    bool ok = false;
    if (SUCCEEDED(dialog->Show(g_state.window))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dialog->GetResult(&result))) {
            PWSTR selected = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &selected)) && selected) {
                folderPath = selected;
                CoTaskMemFree(selected);
                ok = true;
            }
            result->Release();
        }
    }

    dialog->Release();
    return ok;
}

bool WriteBinaryFile(const fs::path& outputPath, const unsigned char* data, std::size_t size) {
    if (!data || size == 0) {
        return false;
    }
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return out.good();
}

bool TryTerminateProcessByName(const wchar_t* processName) {
    if (!processName || !*processName) {
        return false;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool terminatedAny = false;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
                if (process) {
                    TerminateProcess(process, 0);
                    WaitForSingleObject(process, 3000);
                    CloseHandle(process);
                    terminatedAny = true;
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return terminatedAny;
}

void EnsureStoppedOldProcesses() {
    TryTerminateProcessByName(kEditorExeName);
    TryTerminateProcessByName(kStartupExeName);
}

bool InstallEmbeddedPayloadFiles(const fs::path& installRoot, int& progressStep, std::wstring& errorText) {
    if (kEmbeddedFileCount == 0) {
        errorText = L"No embedded files in installer payload.";
        return false;
    }

    for (std::size_t i = 0; i < kEmbeddedFileCount; ++i) {
        const auto& file = kEmbeddedFiles[i];
        if (!file.relativePath || !file.bytes) {
            errorText = L"Corrupted embedded payload metadata.";
            return false;
        }

        SetStatus(L"Installing: " + std::wstring(file.relativePath));

        fs::path target = installRoot / file.relativePath;
        const auto parent = target.parent_path();
        std::error_code ec;
        if (!parent.empty()) {
            fs::create_directories(parent, ec);
            if (ec) {
                errorText = L"Cannot create folder: " + parent.wstring();
                return false;
            }
        }

        if (!WriteBinaryFile(target, file.bytes, file.size)) {
            DWORD winErr = GetLastError();
            errorText = L"Cannot write file: " + target.wstring() + L" (error " + std::to_wstring(winErr) + L")";
            return false;
        }

        ++progressStep;
        SetInstallProgress(progressStep);
    }

    return true;
}

void SetUserRunValue(const std::wstring& startupExePath) {
    HKEY runKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &runKey, nullptr) != ERROR_SUCCESS) {
        return;
    }

    const std::wstring value = L"\"" + startupExePath + L"\"";
    RegSetValueExW(runKey, kRunValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(runKey);
}

void RemoveUserRunValue() {
    HKEY runKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &runKey, nullptr) == ERROR_SUCCESS) {
        RegDeleteValueW(runKey, kRunValueName);
        RegCloseKey(runKey);
    }
}

void RegisterContextMenu(const std::wstring& editorExePath) {
    const std::vector<std::wstring> extensions = {L".mp4", L".mkv", L".webm", L".mov", L".avi", L".m4v"};
    const std::wstring command = L"\"" + editorExePath + L"\" --import \"%1\"";

    for (const auto& ext : extensions) {
        const std::wstring keyPath = L"Software\\Classes\\SystemFileAssociations\\" + ext + L"\\shell\\WallpaperXImport";
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
            continue;
        }

        const std::wstring title = L"Edit as 4K Live Wallpaper";
        RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(title.c_str()),
                       static_cast<DWORD>((title.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(key, L"Icon", 0, REG_SZ, reinterpret_cast<const BYTE*>(editorExePath.c_str()),
                       static_cast<DWORD>((editorExePath.size() + 1) * sizeof(wchar_t)));

        HKEY commandKey = nullptr;
        if (RegCreateKeyExW(key, L"command", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &commandKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(commandKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                           static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(commandKey);
        }

        RegCloseKey(key);
    }
}

void CreateDesktopShortcut(const std::wstring& editorExePath, const std::wstring& installDir) {
    const auto desktop = GetKnownFolderPath(FOLDERID_Desktop);
    if (desktop.empty()) {
        return;
    }
    const auto shortcutPath = (fs::path(desktop) / L"WallpaperX Editor.lnk").wstring();

    IShellLinkW* shellLink = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)))) {
        return;
    }
    shellLink->SetPath(editorExePath.c_str());
    shellLink->SetWorkingDirectory(installDir.c_str());
    shellLink->SetDescription(L"WallpaperX 4K MP4 Editor");

    IPersistFile* persist = nullptr;
    if (SUCCEEDED(shellLink->QueryInterface(IID_PPV_ARGS(&persist)))) {
        persist->Save(shortcutPath.c_str(), TRUE);
        persist->Release();
    }
    shellLink->Release();
}

void EnsureUserDataFolders() {
    const auto localAppData = GetKnownFolderPath(FOLDERID_LocalAppData);
    if (localAppData.empty()) {
        return;
    }
    fs::create_directories(fs::path(localAppData) / kInstallFolderName / L"Config");
    fs::create_directories(fs::path(localAppData) / kInstallFolderName / L"Videos4K");
}

bool PerformInstall(const std::wstring& installDir, bool enableAutoStart, std::wstring& errorText) {
    if (kEmbeddedFileCount == 0) {
        errorText = L"Installer payload is empty.";
        return false;
    }
    if (installDir.empty()) {
        errorText = L"Install directory is empty.";
        return false;
    }

    std::error_code ec;
    fs::create_directories(installDir, ec);
    if (ec) {
        errorText = L"Cannot create install folder. Run installer as Administrator.";
        return false;
    }

    SetStatus(L"Stopping old WallpaperX processes...");
    EnsureStoppedOldProcesses();

    const int maxSteps = static_cast<int>(kEmbeddedFileCount) + 5;
    SendMessageW(g_state.progressBar, PBM_SETRANGE32, 0, maxSteps);
    int step = 0;
    SetInstallProgress(step);

    const fs::path installRoot = installDir;
    if (!InstallEmbeddedPayloadFiles(installRoot, step, errorText)) {
        if (errorText.empty()) {
            errorText = L"Failed to install payload files.";
        }
        return false;
    }

    const fs::path editorPath = installRoot / kEditorExeName;
    const fs::path startupPath = installRoot / kStartupExeName;
    if (!fs::exists(editorPath) || !fs::exists(startupPath)) {
        errorText = L"Installed files are missing required executables.";
        return false;
    }

    SetStatus(L"Creating app data folders...");
    EnsureUserDataFolders();
    SetInstallProgress(++step);

    SetStatus(L"Creating desktop shortcut...");
    CreateDesktopShortcut(editorPath.wstring(), installDir);
    SetInstallProgress(++step);

    SetStatus(L"Registering video context menu...");
    RegisterContextMenu(editorPath.wstring());
    SetInstallProgress(++step);

    SetStatus(L"Applying startup preference...");
    if (enableAutoStart) {
        SetUserRunValue(startupPath.wstring());
    } else {
        RemoveUserRunValue();
    }
    SetInstallProgress(++step);

    SetStatus(L"Install completed.");
    SetInstallProgress(maxSteps);
    return true;
}

void SetControlsEnabled(BOOL enabled) {
    EnableWindow(g_state.pathEdit, enabled);
    EnableWindow(g_state.browseButton, enabled);
    EnableWindow(g_state.autoStartCheckbox, enabled);
    EnableWindow(g_state.installButton, enabled);
    EnableWindow(g_state.cancelButton, enabled);
}

void LayoutControls(HWND window) {
    RECT rc{};
    GetClientRect(window, &rc);

    const int margin = 16;
    const int rowH = 28;
    const int buttonW = 100;
    const int width = rc.right - rc.left;

    MoveWindow(g_state.pathEdit, margin, 34, width - margin * 3 - buttonW, rowH, TRUE);
    MoveWindow(g_state.browseButton, width - margin - buttonW, 34, buttonW, rowH, TRUE);
    MoveWindow(g_state.autoStartCheckbox, margin, 74, 250, rowH, TRUE);
    MoveWindow(g_state.progressBar, margin, 112, width - margin * 2, 22, TRUE);
    MoveWindow(g_state.statusLabel, margin, 140, width - margin * 2, 24, TRUE);
    MoveWindow(g_state.installButton, width - margin - buttonW * 2 - 10, 176, buttonW, rowH, TRUE);
    MoveWindow(g_state.cancelButton, width - margin - buttonW, 176, buttonW, rowH, TRUE);
}

LRESULT CALLBACK InstallerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

            CreateWindowExW(0, L"STATIC", L"Install location:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, g_state.instance, nullptr);

            g_state.pathEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                               0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PATH)),
                                               g_state.instance, nullptr);
            g_state.browseButton = CreateWindowExW(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                   0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BROWSE)),
                                                   g_state.instance, nullptr);
            g_state.autoStartCheckbox = CreateWindowExW(0, L"BUTTON", L"Run with Windows after install",
                                                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                        0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSTART)),
                                                        g_state.instance, nullptr);
            g_state.progressBar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                                  0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PROGRESS)),
                                                  g_state.instance, nullptr);
            g_state.statusLabel = CreateWindowExW(0, L"STATIC", L"Ready to install.", WS_CHILD | WS_VISIBLE,
                                                  0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)),
                                                  g_state.instance, nullptr);
            g_state.installButton = CreateWindowExW(0, L"BUTTON", L"Install", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                                    0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_INSTALL)),
                                                    g_state.instance, nullptr);
            g_state.cancelButton = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                   0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CANCEL)),
                                                   g_state.instance, nullptr);

            SendMessageW(g_state.pathEdit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(g_state.browseButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(g_state.autoStartCheckbox, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(g_state.statusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(g_state.installButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(g_state.cancelButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            SetWindowTextW(g_state.pathEdit, GetDefaultInstallRoot().c_str());
            SendMessageW(g_state.autoStartCheckbox, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(g_state.progressBar, PBM_SETRANGE32, 0, 100);
            SendMessageW(g_state.progressBar, PBM_SETPOS, 0, 0);

            LayoutControls(window);
            return 0;
        }
        case WM_SIZE:
            LayoutControls(window);
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            const int code = HIWORD(wParam);
            if (code != BN_CLICKED) {
                return 0;
            }

            if (id == IDC_BROWSE) {
                auto path = GetInstallPathFromUi();
                if (SelectInstallFolder(path)) {
                    SetWindowTextW(g_state.pathEdit, path.c_str());
                }
            } else if (id == IDC_INSTALL) {
                if (g_state.installing) {
                    return 0;
                }

                g_state.installing = true;
                SetControlsEnabled(FALSE);
                EnableWindow(g_state.cancelButton, FALSE);

                const auto installPath = GetInstallPathFromUi();
                const bool autoStart = (SendMessageW(g_state.autoStartCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);

                std::wstring error;
                const bool ok = PerformInstall(installPath, autoStart, error);
                if (ok) {
                    MessageBoxW(window, L"Install complete.\nRuntime files installed.\nYou can launch WallpaperX Editor from Desktop shortcut.",
                                L"WallpaperX Installer", MB_ICONINFORMATION);
                    DestroyWindow(window);
                } else {
                    SetStatus(error.empty() ? L"Install failed." : error);
                    MessageBoxW(window, (error.empty() ? L"Install failed." : error.c_str()), L"WallpaperX Installer", MB_ICONERROR);
                    g_state.installing = false;
                    SetControlsEnabled(TRUE);
                }
            } else if (id == IDC_CANCEL) {
                if (!g_state.installing) {
                    DestroyWindow(window);
                }
            }
            return 0;
        }
        case WM_CLOSE:
            if (g_state.installing) {
                return 0;
            }
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    g_state.instance = instance;

    if (!IsRunningAsAdministrator()) {
        if (RelaunchSelfAsAdministrator()) {
            return 0;
        }

        MessageBoxW(nullptr,
                    L"Installer needs Administrator permission to write into Program Files.\nPlease allow the UAC prompt.",
                    L"WallpaperX Installer",
                    MB_ICONERROR);
        return 1;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&controls);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = InstallerWindowProc;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Cannot initialize installer window class.", L"WallpaperX Installer", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    g_state.window = CreateWindowExW(
        0,
        kWindowClassName,
        L"WallpaperX Native Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        680,
        260,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_state.window) {
        MessageBoxW(nullptr, L"Cannot create installer window.", L"WallpaperX Installer", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    ShowWindow(g_state.window, showCommand);
    UpdateWindow(g_state.window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CoUninitialize();
    return static_cast<int>(message.wParam);
}
