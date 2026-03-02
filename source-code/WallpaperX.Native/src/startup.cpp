#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

std::wstring GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR rawPath = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &rawPath)) && rawPath) {
        result = rawPath;
    }
    CoTaskMemFree(rawPath);
    return result;
}

std::wstring GetCurrentExePath() {
    std::wstring path(1024, L'\0');
    DWORD copied = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    while (copied == path.size()) {
        path.resize(path.size() * 2);
        copied = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }
    path.resize(copied);
    return path;
}

std::wstring ResolveEditorPath() {
    const fs::path selfPath = GetCurrentExePath();
    const fs::path localEditor = selfPath.parent_path() / L"WallpaperXEditor.exe";
    if (fs::exists(localEditor)) {
        return localEditor.wstring();
    }

    const auto programFiles = GetKnownFolderPath(FOLDERID_ProgramFiles);
    if (!programFiles.empty()) {
        const fs::path installed = fs::path(programFiles) / L"WallpaperXNative" / L"WallpaperXEditor.exe";
        if (fs::exists(installed)) {
            return installed.wstring();
        }
    }

    return L"";
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    const auto editorPath = ResolveEditorPath();
    if (!editorPath.empty()) {
        const std::wstring args = L"\""+ editorPath + L"\" --background";
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::wstring command = args;
        if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }

    CoUninitialize();
    return 0;
}
