#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mfapi.h>
#include <mfplay.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

constexpr int IDC_PATH_EDIT = 1001;
constexpr int IDC_BROWSE = 1002;
constexpr int IDC_APPLY = 1003;
constexpr int IDC_LOOP = 1004;
constexpr int IDC_AUTOSTART = 1005;
constexpr int IDC_STATUS = 1006;

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"WallpaperXNative";
constexpr wchar_t kAppName[] = L"WallpaperX Native";
constexpr wchar_t kInstallFolderName[] = L"WallpaperXNative";
constexpr wchar_t kExeName[] = L"WallpaperXNative.exe";
constexpr wchar_t kControllerClass[] = L"WallpaperXNative.Controller";
constexpr wchar_t kWallpaperClass[] = L"WallpaperXNative.Wallpaper";
constexpr UINT kCreateWorkerMessage = 0x052C;

template <typename T>
void SafeRelease(T** value) {
    if (value && *value) {
        (*value)->Release();
        *value = nullptr;
    }
}

struct AppSettings {
    std::wstring lastVideoPath;
    bool autoStart = false;
    bool loop = true;
};

struct AppState;

class MediaPlayerCallback final : public IMFPMediaPlayerCallback {
public:
    explicit MediaPlayerCallback(AppState* app) : app_(app) {}

    STDMETHODIMP QueryInterface(REFIID iid, void** object) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* eventHeader) override;

private:
    volatile long refCount_ = 1;
    AppState* app_ = nullptr;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND controller = nullptr;
    HWND wallpaper = nullptr;
    HWND pathEdit = nullptr;
    HWND loopCheck = nullptr;
    HWND autoStartCheck = nullptr;
    HWND statusLabel = nullptr;

    IMFPMediaPlayer* player = nullptr;
    MediaPlayerCallback* callback = nullptr;

    AppSettings settings;
    std::wstring settingsPath;
    std::wstring videoLibraryPath;
    std::wstring currentVideo;

    bool workerAttached = false;
    bool backgroundMode = false;
};

static AppState g_app;

std::wstring Trim(const std::wstring& input) {
    auto begin = input.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) {
        return L"";
    }
    auto end = input.find_last_not_of(L" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::wstring ToLower(std::wstring value) {
    std::ranges::transform(value, value.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return value;
}

bool IsSupportedVideo(const std::wstring& path) {
    static const std::vector<std::wstring> kExtensions = {
        L".mp4", L".mkv", L".webm", L".mov", L".avi", L".m4v"
    };

    auto ext = ToLower(fs::path(path).extension().wstring());
    return std::ranges::find(kExtensions, ext) != kExtensions.end();
}

void SetStatus(const std::wstring& message) {
    if (g_app.statusLabel) {
        SetWindowTextW(g_app.statusLabel, message.c_str());
    }
}

std::wstring GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR rawPath = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &rawPath)) && rawPath) {
        result = rawPath;
    }
    CoTaskMemFree(rawPath);
    return result;
}

void EnsureAppDirectories() {
    const auto appData = GetKnownFolderPath(FOLDERID_LocalAppData);
    const fs::path root = fs::path(appData) / kInstallFolderName;
    const fs::path videos = root / L"Videos4K";
    const fs::path config = root / L"Config";

    fs::create_directories(videos);
    fs::create_directories(config);

    g_app.settingsPath = (config / L"settings.ini").wstring();
    g_app.videoLibraryPath = videos.wstring();
}

AppSettings LoadSettings(const std::wstring& path) {
    AppSettings result;
    std::wifstream input{fs::path(path)};
    if (!input.is_open()) {
        return result;
    }

    std::wstring line;
    while (std::getline(input, line)) {
        auto split = line.find(L'=');
        if (split == std::wstring::npos) {
            continue;
        }
        auto key = Trim(line.substr(0, split));
        auto value = Trim(line.substr(split + 1));
        if (key == L"last_video") {
            result.lastVideoPath = value;
        } else if (key == L"autostart") {
            result.autoStart = (value == L"1");
        } else if (key == L"loop") {
            result.loop = (value != L"0");
        }
    }

    return result;
}

void SaveSettings(const AppSettings& settings, const std::wstring& path) {
    std::wofstream output{fs::path(path), std::ios::trunc};
    if (!output.is_open()) {
        return;
    }

    output << L"last_video=" << settings.lastVideoPath << L"\n";
    output << L"autostart=" << (settings.autoStart ? L"1" : L"0") << L"\n";
    output << L"loop=" << (settings.loop ? L"1" : L"0") << L"\n";
}

std::wstring GetExecutablePath() {
    std::wstring result(1024, L'\0');
    DWORD copied = GetModuleFileNameW(nullptr, result.data(), static_cast<DWORD>(result.size()));
    while (copied == result.size()) {
        result.resize(result.size() * 2);
        copied = GetModuleFileNameW(nullptr, result.data(), static_cast<DWORD>(result.size()));
    }
    result.resize(copied);
    return result;
}

std::wstring GetRegisteredExecutablePath() {
    const auto programFiles = GetKnownFolderPath(FOLDERID_ProgramFiles);
    if (!programFiles.empty()) {
        const auto installed = fs::path(programFiles) / kInstallFolderName / kExeName;
        if (fs::exists(installed)) {
            return installed.wstring();
        }
    }

    return GetExecutablePath();
}

void SetAutoStart(bool enabled) {
    HKEY runKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &runKey, nullptr) != ERROR_SUCCESS) {
        SetStatus(L"Cannot update startup setting (registry blocked).");
        return;
    }

    if (enabled) {
        const auto exe = GetRegisteredExecutablePath();
        std::wstring value = L"\"" + exe + L"\" --background";
        RegSetValueExW(runKey, kRunValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                       static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(runKey, kRunValueName);
    }

    RegCloseKey(runKey);
}

void RegisterVideoContextMenu() {
    const auto exePath = GetRegisteredExecutablePath();
    const std::wstring command = L"\"" + exePath + L"\" --import \"%1\"";
    const std::vector<std::wstring> extensions = {L".mp4", L".mkv", L".webm", L".mov", L".avi", L".m4v"};

    for (const auto& ext : extensions) {
        const std::wstring keyPath = L"Software\\Classes\\SystemFileAssociations\\" + ext + L"\\shell\\WallpaperXImport";
        HKEY key = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
            continue;
        }

        const std::wstring title = L"Set as WallpaperX wallpaper";
        RegSetValueExW(key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(title.c_str()),
                       static_cast<DWORD>((title.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(key, L"Icon", 0, REG_SZ, reinterpret_cast<const BYTE*>(exePath.c_str()),
                       static_cast<DWORD>((exePath.size() + 1) * sizeof(wchar_t)));

        HKEY commandKey = nullptr;
        if (RegCreateKeyExW(key, L"command", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &commandKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(commandKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                           static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(commandKey);
        }

        RegCloseKey(key);
    }
}

void EnsureDesktopShortcut() {
    const auto desktop = GetKnownFolderPath(FOLDERID_Desktop);
    if (desktop.empty()) {
        return;
    }

    const auto linkPath = (fs::path(desktop) / L"WallpaperX Editor.lnk").wstring();
    const auto exePath = GetRegisteredExecutablePath();

    IShellLinkW* shellLink = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)))) {
        return;
    }

    shellLink->SetPath(exePath.c_str());
    shellLink->SetDescription(L"WallpaperX MP4 editor");
    shellLink->SetArguments(L"");

    IPersistFile* persist = nullptr;
    if (SUCCEEDED(shellLink->QueryInterface(IID_PPV_ARGS(&persist)))) {
        persist->Save(linkPath.c_str(), TRUE);
        persist->Release();
    }

    shellLink->Release();
}

std::wstring TimestampSuffix() {
    SYSTEMTIME now {};
    GetLocalTime(&now);
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    return buffer;
}

std::wstring ImportVideoToLibrary(const std::wstring& inputPath) {
    try {
        fs::path source = fs::absolute(fs::path(inputPath));
        if (!fs::exists(source) || !fs::is_regular_file(source)) {
            SetStatus(L"Video file not found.");
            return L"";
        }
        if (!IsSupportedVideo(source.wstring())) {
            SetStatus(L"Unsupported format. Use MP4/MKV/WEBM/MOV/AVI/M4V.");
            return L"";
        }

        fs::path destinationDir = fs::path(g_app.videoLibraryPath);
        fs::create_directories(destinationDir);

        auto stem = source.stem().wstring();
        if (stem.empty()) {
            stem = L"video";
        }

        const fs::path destination = destinationDir / (stem + L"_" + TimestampSuffix() + source.extension().wstring());
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);

        return destination.wstring();
    } catch (...) {
        SetStatus(L"Import failed. File may be locked.");
        return L"";
    }
}

HWND FindWallpaperHostWindow() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (progman) {
        DWORD_PTR result = 0;
        SendMessageTimeoutW(progman, kCreateWorkerMessage, 0, 0, SMTO_NORMAL, 1000, &result);
    }

    HWND worker = nullptr;
    EnumWindows(
        [](HWND top, LPARAM param) -> BOOL {
            HWND shellView = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
            if (!shellView) {
                return TRUE;
            }

            HWND* out = reinterpret_cast<HWND*>(param);
            *out = FindWindowExW(nullptr, top, L"WorkerW", nullptr);
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&worker));

    if (worker) {
        return worker;
    }
    return progman;
}

void AttachWallpaperToDesktop(HWND wallpaperWindow) {
    const HWND host = FindWallpaperHostWindow();
    if (!host || !wallpaperWindow) {
        SetStatus(L"Cannot attach wallpaper to WorkerW.");
        return;
    }

    LONG_PTR style = GetWindowLongPtrW(wallpaperWindow, GWL_STYLE);
    style &= ~WS_POPUP;
    style |= WS_CHILD | WS_VISIBLE;
    SetWindowLongPtrW(wallpaperWindow, GWL_STYLE, style);

    SetParent(wallpaperWindow, host);

    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    SetWindowPos(wallpaperWindow, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    g_app.workerAttached = true;
}

void StopPlayback() {
    if (g_app.player) {
        g_app.player->Shutdown();
        SafeRelease(&g_app.player);
    }
}

bool StartPlayback(const std::wstring& videoPath) {
    if (videoPath.empty() || !fs::exists(videoPath)) {
        SetStatus(L"Selected video is missing.");
        return false;
    }

    if (!g_app.workerAttached) {
        AttachWallpaperToDesktop(g_app.wallpaper);
    }

    StopPlayback();

    HRESULT hr = MFPCreateMediaPlayer(
        videoPath.c_str(),
        FALSE,
        0,
        g_app.callback,
        g_app.wallpaper,
        &g_app.player);
    if (FAILED(hr) || !g_app.player) {
        SetStatus(L"Cannot initialize video playback (MFPlay).");
        return false;
    }

    hr = g_app.player->Play();
    if (FAILED(hr)) {
        SetStatus(L"Playback failed.");
        return false;
    }

    g_app.currentVideo = videoPath;
    g_app.settings.lastVideoPath = videoPath;
    SaveSettings(g_app.settings, g_app.settingsPath);

    const auto fileName = fs::path(videoPath).filename().wstring();
    SetStatus(L"Playing: " + fileName);
    return true;
}

void ApplyLoopSettingToUi() {
    SendMessageW(g_app.loopCheck, BM_SETCHECK, g_app.settings.loop ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_app.autoStartCheck, BM_SETCHECK, g_app.settings.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);
}

void SelectAndImportVideo(HWND owner) {
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.webm;*.mov;*.avi;*.m4v\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    const auto imported = ImportVideoToLibrary(filePath);
    if (imported.empty()) {
        return;
    }

    SetWindowTextW(g_app.pathEdit, imported.c_str());
    SetStatus(L"Imported to local library. Click Apply.");
}

void ApplyFromPathEdit() {
    int length = GetWindowTextLengthW(g_app.pathEdit);
    if (length <= 0) {
        SetStatus(L"Select a video first.");
        return;
    }

    std::wstring path(length + 1, L'\0');
    GetWindowTextW(g_app.pathEdit, path.data(), static_cast<int>(path.size()));
    path.resize(length);
    path = Trim(path);
    if (path.empty()) {
        SetStatus(L"Path is empty.");
        return;
    }

    // If user typed an external file path, still copy into managed library.
    std::wstring sourcePath = path;
    if (!fs::exists(path) || fs::path(path).parent_path() != fs::path(g_app.videoLibraryPath)) {
        sourcePath = ImportVideoToLibrary(path);
        if (sourcePath.empty()) {
            return;
        }
        SetWindowTextW(g_app.pathEdit, sourcePath.c_str());
    }

    StartPlayback(sourcePath);
}

LRESULT CALLBACK WallpaperWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (g_app.workerAttached) {
                const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
                const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
                const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                SetWindowPos(window, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

void LayoutController(HWND window) {
    RECT rc {};
    GetClientRect(window, &rc);

    const int margin = 14;
    const int buttonW = 96;
    const int rowH = 28;
    const int width = rc.right - rc.left;

    MoveWindow(g_app.pathEdit, margin, margin, width - margin * 3 - buttonW, rowH, TRUE);
    MoveWindow(GetDlgItem(window, IDC_BROWSE), width - margin - buttonW, margin, buttonW, rowH, TRUE);
    MoveWindow(GetDlgItem(window, IDC_APPLY), margin, margin + rowH + 10, buttonW, rowH, TRUE);
    MoveWindow(g_app.loopCheck, margin + buttonW + 16, margin + rowH + 12, 160, rowH, TRUE);
    MoveWindow(g_app.autoStartCheck, margin + buttonW + 190, margin + rowH + 12, 220, rowH, TRUE);
    MoveWindow(g_app.statusLabel, margin, margin + rowH * 2 + 24, width - margin * 2, rowH, TRUE);
}

LRESULT CALLBACK ControllerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

            g_app.pathEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                             0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PATH_EDIT)),
                                             g_app.instance, nullptr);
            SendMessageW(g_app.pathEdit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            HWND browseBtn = CreateWindowExW(0, L"BUTTON", L"Browse",
                                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                             0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BROWSE)),
                                             g_app.instance, nullptr);
            SendMessageW(browseBtn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            HWND applyBtn = CreateWindowExW(0, L"BUTTON", L"Apply",
                                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_APPLY)),
                                            g_app.instance, nullptr);
            SendMessageW(applyBtn, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            g_app.loopCheck = CreateWindowExW(0, L"BUTTON", L"Loop protection",
                                              WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                              0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOOP)),
                                              g_app.instance, nullptr);
            SendMessageW(g_app.loopCheck, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            g_app.autoStartCheck = CreateWindowExW(0, L"BUTTON", L"Auto start with Windows",
                                                   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                   0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_AUTOSTART)),
                                                   g_app.instance, nullptr);
            SendMessageW(g_app.autoStartCheck, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            g_app.statusLabel = CreateWindowExW(0, L"STATIC", L"Ready.",
                                                WS_CHILD | WS_VISIBLE | SS_LEFT,
                                                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)),
                                                g_app.instance, nullptr);
            SendMessageW(g_app.statusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            ApplyLoopSettingToUi();

            if (!g_app.settings.lastVideoPath.empty()) {
                SetWindowTextW(g_app.pathEdit, g_app.settings.lastVideoPath.c_str());
            }

            LayoutController(window);
            return 0;
        }
        case WM_SIZE:
            LayoutController(window);
            return 0;
        case WM_COMMAND: {
            const int controlId = LOWORD(wParam);
            const int notifyCode = HIWORD(wParam);
            if (controlId == IDC_BROWSE && notifyCode == BN_CLICKED) {
                SelectAndImportVideo(window);
            } else if (controlId == IDC_APPLY && notifyCode == BN_CLICKED) {
                ApplyFromPathEdit();
            } else if (controlId == IDC_LOOP && notifyCode == BN_CLICKED) {
                g_app.settings.loop = (SendMessageW(g_app.loopCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveSettings(g_app.settings, g_app.settingsPath);
            } else if (controlId == IDC_AUTOSTART && notifyCode == BN_CLICKED) {
                g_app.settings.autoStart = (SendMessageW(g_app.autoStartCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetAutoStart(g_app.settings.autoStart);
                SaveSettings(g_app.settings, g_app.settingsPath);
                SetStatus(g_app.settings.autoStart ? L"Auto start enabled." : L"Auto start disabled.");
            }
            return 0;
        }
        case WM_DESTROY:
            StopPlayback();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

STDMETHODIMP MediaPlayerCallback::QueryInterface(REFIID iid, void** object) {
    if (!object) {
        return E_POINTER;
    }
    if (iid == __uuidof(IUnknown) || iid == __uuidof(IMFPMediaPlayerCallback)) {
        *object = static_cast<IMFPMediaPlayerCallback*>(this);
        AddRef();
        return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) MediaPlayerCallback::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&refCount_));
}

STDMETHODIMP_(ULONG) MediaPlayerCallback::Release() {
    const ULONG value = static_cast<ULONG>(InterlockedDecrement(&refCount_));
    if (value == 0) {
        delete this;
    }
    return value;
}

void STDMETHODCALLTYPE MediaPlayerCallback::OnMediaPlayerEvent(MFP_EVENT_HEADER* eventHeader) {
    if (!eventHeader || !app_) {
        return;
    }
    if (eventHeader->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED && app_->settings.loop && app_->player) {
        PROPVARIANT position {};
        position.vt = VT_I8;
        position.hVal.QuadPart = 0;
        app_->player->SetPosition(GUID_NULL, &position);
        app_->player->Play();
    }
}

bool RegisterWindowClasses(HINSTANCE instance) {
    WNDCLASSEXW controllerClass {};
    controllerClass.cbSize = sizeof(controllerClass);
    controllerClass.hInstance = instance;
    controllerClass.lpfnWndProc = ControllerWindowProc;
    controllerClass.lpszClassName = kControllerClass;
    controllerClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    controllerClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&controllerClass)) {
        return false;
    }

    WNDCLASSEXW wallpaperClass {};
    wallpaperClass.cbSize = sizeof(wallpaperClass);
    wallpaperClass.hInstance = instance;
    wallpaperClass.lpfnWndProc = WallpaperWindowProc;
    wallpaperClass.lpszClassName = kWallpaperClass;
    wallpaperClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wallpaperClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    return RegisterClassExW(&wallpaperClass) != 0;
}

void ParseCommandLine(bool& background, std::wstring& importPath) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return;
    }

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--background") {
            background = true;
        } else if (arg == L"--import" && i + 1 < argc) {
            importPath = argv[++i];
        } else if (!arg.empty() && arg[0] != L'-') {
            importPath = arg;
        }
    }

    LocalFree(argv);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    g_app.instance = instance;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION);

    INITCOMMONCONTROLSEX controls {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    bool background = false;
    std::wstring importPath;
    ParseCommandLine(background, importPath);
    g_app.backgroundMode = background;

    EnsureAppDirectories();
    g_app.settings = LoadSettings(g_app.settingsPath);

    RegisterVideoContextMenu();
    EnsureDesktopShortcut();

    if (!RegisterWindowClasses(instance)) {
        MessageBoxW(nullptr, L"Window class registration failed.", kAppName, MB_ICONERROR);
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    g_app.wallpaper = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kWallpaperClass,
        L"WallpaperRenderer",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        100,
        100,
        nullptr,
        nullptr,
        instance,
        nullptr);

    g_app.controller = CreateWindowExW(
        0,
        kControllerClass,
        kAppName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        680,
        200,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_app.wallpaper || !g_app.controller) {
        MessageBoxW(nullptr, L"Cannot create app windows.", kAppName, MB_ICONERROR);
        StopPlayback();
        MFShutdown();
        CoUninitialize();
        return 1;
    }

    g_app.callback = new MediaPlayerCallback(&g_app);
    SetAutoStart(g_app.settings.autoStart);

    ShowWindow(g_app.controller, background ? SW_SHOWMINNOACTIVE : showCommand);
    UpdateWindow(g_app.controller);

    if (!importPath.empty()) {
        auto imported = ImportVideoToLibrary(importPath);
        if (!imported.empty()) {
            SetWindowTextW(g_app.pathEdit, imported.c_str());
            StartPlayback(imported);
        }
    } else if (!g_app.settings.lastVideoPath.empty() && fs::exists(g_app.settings.lastVideoPath)) {
        StartPlayback(g_app.settings.lastVideoPath);
    }

    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_app.callback) {
        g_app.callback->Release();
        g_app.callback = nullptr;
    }

    MFShutdown();
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
