#include "OFS_Util.h"
#include "OFS_GL.h"
#include "OFS_EventSystem.h"

#include <filesystem>
#include "SDL_rwops.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(WIN32)
#define STBI_WINDOWS_UTF8
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "imgui.h"

#include "tinyfiledialogs.h"

#include <iostream>
#include <string>
#include <locale>
#include <codecvt>

#define SDEFL_IMPLEMENTATION
#include "sdefl.h"

#define SINFL_IMPLEMENTATION
#include "sinfl.h"

#define RND_IMPLEMENTATION
#include "rnd.h"

static void SanitizeString(std::string& str) noexcept
{
    // tinyfiledialogs doesn't like quotes
    // I'm starting to not like tinyfiledialogs...
    std::replace(str.begin(), str.end(), '\"', ' ');
    std::replace(str.begin(), str.end(), '\'', ' ');
}

#ifdef WIN32
inline static int WindowsShellExecute(const wchar_t* op, const wchar_t* program, const wchar_t* params) noexcept
{
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew
    // If the function succeeds, it returns a value greater than 32.
    // If the function fails, it returns an error value that indicates the cause of the failure.
    // The return value is cast as an HINSTANCE for backward compatibility with 16-bit Windows applications.
    // It is not a true HINSTANCE, however.
    // It can be cast only to an INT_PTR and compared to either 32 or the following error codes below.
    auto val = (INT_PTR)ShellExecuteW(NULL, op, program, params, NULL, SW_SHOWNORMAL);
    return val > 32;
}
#endif

namespace Util {

int OpenFileExplorer(const std::string& str)
{
#if defined(WIN32)
    std::wstring wstr = Utf8ToUtf16(str);
    std::wstringstream ss;
    ss << L'"' << wstr << L'"';
    auto params = ss.str();
    return WindowsShellExecute(nullptr, L"explorer", params.c_str());
#elif defined(__APPLE__)
    LOG_ERROR("Not implemented for this platform.");
    return 1;
#else
    return OpenUrl(str);
#endif
}

int OpenUrl(const std::string& url)
{
#if defined(WIN32)
    std::wstring wstr = Utf8ToUtf16(url);
    std::wstringstream ss;
    ss << L'"' << wstr << L'"';
    auto params = ss.str();
    return WindowsShellExecute(L"open", params.c_str(), NULL);
#elif defined(__APPLE__)
    LOG_ERROR("Not implemented for this platform.");
    return 1;
#else
    char tmp[1024];
    stbsp_snprintf(tmp, sizeof(tmp), "xdg-open \"%s\"", url.c_str());
    return std::system(tmp);
#endif
}

void OpenFileDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler, bool multiple, const std::vector<const char*>& filters, const std::string& filterText) noexcept
{
    struct FileDialogThreadData {
        bool multiple = false;
        std::string title;
        std::string path;
        std::vector<const char*> filters;
        std::string filterText;
        FileDialogResultHandler handler;
    };
    auto thread = [](void* ctx) {
        auto data = (FileDialogThreadData*)ctx;

        if (!DirectoryExists(data->path)) {
            data->path = "";
        }

#ifdef WIN32
        std::wstring wtitle = Utf8ToUtf16(data->title);
        std::wstring wpath = Utf8ToUtf16(data->path);
        std::wstring wfilterText = Utf8ToUtf16(data->filterText);
        std::vector<std::wstring> wfilters;
        std::vector<const wchar_t*> wc_str;
        wfilters.reserve(data->filters.size());
        wc_str.reserve(data->filters.size());
        for (auto&& filter : data->filters) {
            wfilters.emplace_back(Utf8ToUtf16(filter));
            wc_str.push_back(wfilters.back().c_str());
        }
        auto result = tinyfd_utf16to8(tinyfd_openFileDialogW(wtitle.c_str(), wpath.c_str(), wc_str.size(), wc_str.data(), wfilterText.empty() ? NULL : wfilterText.c_str(), data->multiple));
#elif __APPLE__
        auto result = tinyfd_openFileDialog(data->title.c_str(), data->path.c_str(), 0, nullptr, data->filterText.empty() ? NULL : data->filterText.c_str(), data->multiple);
#else
        auto result = tinyfd_openFileDialog(data->title.c_str(), data->path.c_str(), data->filters.size(), data->filters.data(), data->filterText.empty() ? NULL : data->filterText.c_str(), data->multiple);
#endif
        auto dialogResult = new FileDialogResult;
        if (result != nullptr) {
            if (data->multiple) {
                int last = 0;
                int index = 0;
                for (char c : std::string(result)) {
                    if (c == '|') {
                        dialogResult->files.emplace_back(std::string(result + last, index - last));
                        last = index + 1;
                    }
                    index += 1;
                }
                dialogResult->files.emplace_back(std::string(result + last, index - last));
            }
            else {
                dialogResult->files.emplace_back(result);
            }
        }

        EV::Enqueue<OFS_DeferEvent>(
            [resultHandler = std::move(data->handler), dialogResult]() {
                resultHandler(*dialogResult);
                delete dialogResult;
            });
        delete data;
        return 0;
    };
    auto threadData = new FileDialogThreadData;
    threadData->handler = std::move(handler);
    threadData->filters = filters;
    threadData->filterText = filterText;
    threadData->multiple = multiple;
    threadData->path = path;
    threadData->title = title;
    auto handle = SDL_CreateThread(thread, "OpenFileDialog", threadData);
    SDL_DetachThread(handle);
}

void SaveFileDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler, const std::vector<const char*>& filters, const std::string& filterText) noexcept
{
    struct SaveFileDialogThreadData {
        std::string title;
        std::string path;
        std::vector<const char*> filters;
        std::string filterText;
        FileDialogResultHandler handler;
    };
    auto thread = [](void* ctx) -> int32_t {
        auto data = (SaveFileDialogThreadData*)ctx;

        auto dialogPath = PathFromString(data->path);
        dialogPath.remove_filename();
        std::error_code ec;
        if (!std::filesystem::exists(dialogPath, ec)) {
            data->path = "";
        }

        SanitizeString(data->path);

        auto result = tinyfd_saveFileDialog(data->title.c_str(), data->path.c_str(), data->filters.size(), data->filters.data(), !data->filterText.empty() ? data->filterText.c_str() : NULL);

        FUN_ASSERT(result, "Ignore this if you pressed cancel.");
        auto saveDialogResult = new FileDialogResult;
        if (result != nullptr) {
            saveDialogResult->files.emplace_back(result);
        }
        EV::Enqueue<OFS_DeferEvent>([resultHandler = std::move(data->handler), saveDialogResult]() {
            resultHandler(*saveDialogResult);
            delete saveDialogResult;
        });
        delete data;
        return 0;
    };
    auto threadData = new SaveFileDialogThreadData;
    threadData->title = title;
    threadData->path = path;
    threadData->filters = filters;
    threadData->filterText = filterText;
    threadData->handler = std::move(handler);
    auto handle = SDL_CreateThread(thread, "SaveFileDialog", threadData);
    SDL_DetachThread(handle);
}

void OpenDirectoryDialog(const std::string& title, const std::string& path, FileDialogResultHandler&& handler) noexcept
{
    struct OpenDirectoryDialogThreadData {
        std::string title;
        std::string path;
        FileDialogResultHandler handler;
    };
    auto thread = [](void* ctx) -> int32_t {
        auto data = (OpenDirectoryDialogThreadData*)ctx;

        if (!DirectoryExists(data->path)) {
            data->path = "";
        }

        auto result = tinyfd_selectFolderDialog(data->title.c_str(), data->path.c_str());

        FUN_ASSERT(result, "Ignore this if you pressed cancel.");
        auto directoryDialogResult = new FileDialogResult;
        if (result != nullptr) {
            directoryDialogResult->files.emplace_back(result);
        }

        EV::Enqueue<OFS_DeferEvent>([resultHandler = std::move(data->handler), directoryDialogResult]() {
            resultHandler(*directoryDialogResult);
            delete directoryDialogResult;
        });
        delete data;
        return 0;
    };
    auto threadData = new OpenDirectoryDialogThreadData;
    threadData->title = title;
    threadData->path = path;
    threadData->handler = std::move(handler);
    auto handle = SDL_CreateThread(thread, "SaveFileDialog", threadData);
    SDL_DetachThread(handle);
}

void YesNoCancelDialog(const std::string& title, const std::string& message, YesNoDialogResultHandler&& handler)
{
    struct YesNoCancelThreadData {
        std::string title;
        std::string message;
        YesNoDialogResultHandler handler;
    };
    auto thread = [](void* user) -> int {
        YesNoCancelThreadData* data = (YesNoCancelThreadData*)user;
        auto result = tinyfd_messageBox(data->title.c_str(), data->message.c_str(), "yesnocancel", NULL, 1);
        YesNoCancel enumResult;
        switch (result) {
            case 0:
                enumResult = YesNoCancel::Cancel;
                break;
            case 1:
                enumResult = YesNoCancel::Yes;
                break;
            case 2:
                enumResult = YesNoCancel::No;
                break;
        }
        EV::Enqueue<OFS_DeferEvent>([resultHandler = std::move(data->handler), enumResult]() {
            resultHandler(enumResult);
        });
        delete data;
        return 0;
    };

    auto threadData = new YesNoCancelThreadData;
    threadData->title = title;
    threadData->message = message;
    threadData->handler = std::move(handler);
    auto handle = SDL_CreateThread(thread, "YesNoCancelDialog", threadData);
    SDL_DetachThread(handle);
}

void MessageBoxAlert(const std::string& title, const std::string& message) noexcept
{
    struct MessageBoxData {
        std::string title;
        std::string message;
    };

    auto thread = [](void* data) -> int {
        MessageBoxData* msg = (MessageBoxData*)data;

        SanitizeString(msg->title);
        SanitizeString(msg->message);
        tinyfd_messageBox(msg->title.c_str(), msg->message.c_str(), "ok", "info", 1);

        delete msg;
        return 0;
    };

    auto threadData = new MessageBoxData;
    threadData->title = title;
    threadData->message = message;
    auto handle = SDL_CreateThread(thread, "MessageBoxAlert", threadData);
    SDL_DetachThread(handle);
}

std::string Resource(const std::string& path) noexcept
{
    auto base = Basepath() / L"data" / Utf8ToUtf16(path);
    base.make_preferred();
    return base.u8string();
}

std::wstring Utf8ToUtf16(const std::string& str) noexcept
{
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wstr = converter.from_bytes(str);
        return wstr;
    }
    catch (const std::exception& ex) {
        LOGF_ERROR("Failed to convert to UTF-16.\n%s", str.c_str());
        return std::wstring();
    }
}

std::filesystem::path PathFromString(const std::string& str) noexcept
{
    auto result = std::filesystem::u8path(str);
    result.make_preferred();
    return result;
}

void ConcatPathSafe(std::filesystem::path& path, const std::string& element) noexcept
{
    path /= PathFromString(element);
}

bool SavePNG(const std::string& path, void* buffer, int32_t width, int32_t height, int32_t channels, bool flipVertical) noexcept
{
    stbi_flip_vertically_on_write(flipVertical);
    bool success = stbi_write_png(path.c_str(),
        width, height,
        channels, buffer, 0);
    return success;
}

std::filesystem::path FfmpegPath() noexcept
{
#if WIN32
    return PathFromString(Prefpath("ffmpeg.exe"));
#else
    auto ffmpegPath = std::filesystem::path("ffmpeg");
    return ffmpegPath;
#endif
}

const char* Format(const char* fmt, ...) noexcept
{
    static char Buffer[4096];
    va_list argp;
    va_start(argp, fmt);
    stbsp_vsnprintf(Buffer, sizeof(Buffer), fmt, argp);
    return Buffer;
}

static rnd_pcg_t pcg;
void InitRandom() noexcept
{
    time_t t = time(0);
    rnd_pcg_seed(&pcg, t);
}

float NextFloat() noexcept
{
    return rnd_pcg_nextf(&pcg);
}

uint32_t RandomColor(float s, float v, float alpha) noexcept
{
    // This is cool :^)
    // https://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically/
    constexpr float goldenRatioConjugate = 0.618033988749895f;
    static float H = NextFloat();

    H += goldenRatioConjugate;
    H = SDL_fmodf(H, 1.f);

    ImColor color;
    color.SetHSV(H, s, v, alpha);
    return ImGui::ColorConvertFloat4ToU32(color);
}

}
