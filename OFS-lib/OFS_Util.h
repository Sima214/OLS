﻿#pragma once
#include "SDL_rwops.h"
#include "SDL_filesystem.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "bitsery/ext/growable.h"
#include "nlohmann/json.hpp"

#include <memory>
#include <fstream>
#include <iomanip>
#include <type_traits>
#include <filesystem>
#include <functional>
#include <vector>
#include <sstream>
#include <chrono>

#include "stb_sprintf.h"
#include "stb_image.h"

#include "OFS_Profiling.h"
#include "OFS_FileLogging.h"

#include "emmintrin.h" // for _mm_pause

#define OFS_PAUSE_INTRIN _mm_pause

// helper for FontAwesome. Version 4.7.0 2016 ttf
#define ICON_FOLDER_OPEN "\xef\x81\xbc"
#define ICON_VOLUME_UP "\xef\x80\xa8"
#define ICON_VOLUME_OFF "\xef\x80\xa6"
#define ICON_LONG_ARROW_UP "\xef\x85\xb6"
#define ICON_LONG_ARROW_DOWN "\xef\x85\xb5"
#define ICON_LONG_ARROW_RIGHT "\xef\x85\xb8"
#define ICON_ARROW_RIGHT "\xef\x81\xa1"
#define ICON_PLAY "\xef\x81\x8b"
#define ICON_PAUSE "\xef\x81\x8c"
#define ICON_GAMEPAD "\xef\x84\x9b"
#define ICON_HAND_RIGHT "\xef\x82\xa4"
#define ICON_BACKWARD "\xef\x81\x8a"
#define ICON_FORWARD "\xef\x81\x8e"
#define ICON_STEP_BACKWARD "\xef\x81\x88"
#define ICON_STEP_FORWARD "\xef\x81\x91"
#define ICON_GITHUB "\xef\x82\x9b"
#define ICON_SHARE "\xef\x81\x85"
#define ICON_EXCLAMATION "\xef\x84\xaa"
#define ICON_REFRESH "\xef\x80\xa1"
#define ICON_TRASH "\xef\x87\xb8"
#define ICON_RANDOM "\xef\x81\xb4"
#define ICON_WARNING_SIGN "\xef\x81\xb1"
#define ICON_LINK "\xef\x83\x81"
#define ICON_UNLINK "\xef\x84\xa7"
#define ICON_COPY "\xef\x83\x85"
#define ICON_LEAF "\xef\x81\xac"
// when adding new ones they need to get added in OFS_DynamicFontAtlas

#if defined(WIN32)
#define FUN_DEBUG_BREAK() __debugbreak()
#else
#define FUN_DEBUG_BREAK()
#endif

#ifndef NDEBUG
#define FUN_ASSERT_F(expr, format, ...) \
    if (expr) {} \
    else { \
        LOG_ERROR("============== ASSERTION FAILED =============="); \
        LOGF_ERROR("in file: \"%s\" line: %d", __FILE__, __LINE__); \
        LOGF_ERROR(format, __VA_ARGS__); \
        FUN_DEBUG_BREAK(); \
    }


// assertion without error message
#define FUN_ASSERT(expr, msg) \
    if (expr) {} \
    else { \
        LOG_ERROR("============== ASSERTION FAILED =============="); \
        LOGF_ERROR("in file: \"%s\" line: %d", __FILE__, __LINE__); \
        LOG_ERROR(msg); \
        FUN_DEBUG_BREAK(); \
    }
#else

#define FUN_ASSERT_F(expr, format, ...)
#define FUN_ASSERT(expr, msg)

#endif

namespace Util {
    template<typename T>
    inline T Clamp(T v, T mn, T mx) noexcept
    {
        return (v < mn) ? mn : (v > mx) ? mx
                                        : v;
    }

    template<typename T>
    inline T Min(T v1, T v2) noexcept
    {
        return (v1 < v2) ? v1 : v2;
    }

    template<typename T>
    inline T Max(T v1, T v2) noexcept
    {
        return (v1 > v2) ? v1 : v2;
    }

    template<typename T>
    inline T MapRange(T val, T a1, T a2, T b1, T b2) noexcept
    {
        return b1 + (val - a1) * (b2 - b1) / (a2 - a1);
    }

    template<typename T>
    inline T Lerp(T startVal, T endVal, float t) noexcept
    {
        return startVal + ((endVal - startVal) * t);
    }

#ifdef WIN32
    inline std::string WindowsMaxPath(const char* path, int32_t pathLen) noexcept
    {
        std::string buffer;
        buffer.reserve(strlen("\\\\?\\") + pathLen);
        buffer.append("\\\\?\\");
        buffer.append(path, pathLen);
        return buffer;
    }
#endif

    inline SDL_RWops* OpenFile(const char* path, const char* mode, int32_t path_len) noexcept
    {
#ifdef WIN32
        SDL_RWops* handle = nullptr;
        if (path_len >= _MAX_PATH) {
            auto max = WindowsMaxPath(path, path_len);
            handle = SDL_RWFromFile(max.c_str(), mode);
        }
        else {
            handle = SDL_RWFromFile(path, mode);
        }
#else
        auto handle = SDL_RWFromFile(path, mode);
#endif
        return handle;
    }

    inline size_t ReadFile(const char* path, std::vector<uint8_t>& buffer) noexcept
    {
        auto file = OpenFile(path, "rb", strlen(path));
        if (file) {
            buffer.clear();
            buffer.resize(SDL_RWsize(file));
            SDL_RWread(file, buffer.data(), sizeof(uint8_t), buffer.size());
            SDL_RWclose(file);
            return buffer.size();
        }
        return 0;
    }

    inline std::string ReadFileString(const char* path) noexcept
    {
        std::string str;
        auto file = OpenFile(path, "rb", strlen(path));
        if (file) {
            str.resize(SDL_RWsize(file));
            SDL_RWread(file, str.data(), 1, str.size());
            SDL_RWclose(file);
        }
        return str;
    }

    inline size_t WriteFile(const char* path, const void* buffer, size_t size) noexcept
    {
        auto file = OpenFile(path, "wb", strlen(path));
        if (file) {
            auto written = SDL_RWwrite(file, buffer, 1, size);
            SDL_RWclose(file);
            return written;
        }
        return 0;
    }

    inline nlohmann::json ParseJson(const std::string& jsonText, bool* success) noexcept
    {
        nlohmann::json json;
        *success = false;
        if (!jsonText.empty()) {
            try {
                json = nlohmann::json::parse(jsonText, nullptr, false, true);
                *success = !json.is_discarded();
            }
            catch (const std::exception& e) {
                *success = false;
                LOGF_ERROR("%s", e.what());
            }
        }
        return json;
    }

    inline nlohmann::json ParseCBOR(const std::vector<uint8_t>& data, bool* success) noexcept
    {
        try {
            auto json = nlohmann::json::from_cbor(data);
            *success = !json.is_discarded();
            return json;
        }
        catch (const std::exception& e) {
            *success = false;
            LOGF_ERROR("%s", e.what());
        }
        return {};
    }

    inline std::string SerializeJson(const nlohmann::json& json, bool pretty = false) noexcept
    {
        auto jsonText = json.dump(pretty ? 4 : -1, ' ');
        return jsonText;
    }

    inline std::vector<uint8_t> SerializeCBOR(const nlohmann::json& json) noexcept
    {
        auto data = nlohmann::json::to_cbor(json);
        return data;
    }

    inline float ParseTime(const char* timeStr, bool* succ) noexcept
    {
        int hours = 0;
        int minutes = 0;
        int seconds = 0;
        int milliseconds = 0;
        *succ = false;

        if (sscanf(timeStr, "%d:%d:%d.%d", &hours, &minutes, &seconds, &milliseconds) < 3) {
            return NAN;
        }

        if (hours >= 0
            && minutes >= 0 && minutes <= 59
            && seconds >= 0 && seconds <= 59
            && milliseconds >= 0 && milliseconds <= 999) {
            float time = 0.f;
            time += (float)hours * 60.f * 60.f;
            time += (float)minutes * 60.f;
            time += (float)seconds;
            time += (float)milliseconds / 1000.f;
            *succ = true;
            return time;
        }
        return NAN;
    }

    inline int FormatTime(char* buf, const int bufLen, float timeSeconds, bool withMs) noexcept
    {
        OFS_PROFILE(__FUNCTION__);
        namespace chrono = std::chrono;
        FUN_ASSERT(bufLen >= 0, "wat");
        if (std::isinf(timeSeconds) || std::isnan(timeSeconds))
            timeSeconds = 0.f;

        auto duration = chrono::duration<float>(timeSeconds);

        int hours = chrono::duration_cast<chrono::hours>(duration).count();
        auto timeConsumed = chrono::duration<float>(60.f * 60.f) * hours;

        int minutes = chrono::duration_cast<chrono::minutes>(duration - timeConsumed).count();
        timeConsumed += chrono::duration<float>(60.f) * minutes;

        int seconds = chrono::duration_cast<chrono::seconds>(duration - timeConsumed).count();

        if (withMs) {
            timeConsumed += chrono::duration<float>(1.f) * seconds;
            int ms = chrono::duration_cast<chrono::milliseconds>(duration - timeConsumed).count();
            return stbsp_snprintf(buf, bufLen, "%02d:%02d:%02d.%03d", hours, minutes, seconds, ms);
        }
        else {
            return stbsp_snprintf(buf, bufLen, "%02d:%02d:%02d", hours, minutes, seconds);
        }
    }

    int OpenFileExplorer(const std::string& path);
    int OpenUrl(const std::string& url);

    std::wstring Utf8ToUtf16(const std::string& str) noexcept;

    std::filesystem::path PathFromString(const std::string& str) noexcept;
    void ConcatPathSafe(std::filesystem::path& path, const std::string& element) noexcept;

    inline std::filesystem::path Basepath() noexcept
    {
        char* base = SDL_GetBasePath();
        auto path = Util::PathFromString(base);
        SDL_free(base);
        return path;
    }

    inline std::string Filename(const std::string& path) noexcept
    {
        return Util::PathFromString(path)
            .replace_extension("")
            .filename()
            .u8string();
    }

    inline bool FileExists(const std::string& file) noexcept
    {
        bool exists = false;
#if WIN32
        std::wstring wfile = Util::Utf8ToUtf16(file);
        struct _stati64 s;
        exists = _wstati64(wfile.c_str(), &s) == 0;
#else
        auto handle = OpenFile(file.c_str(), "r", file.size());
        if (handle != nullptr) {
            SDL_RWclose(handle);
            exists = true;
        }
        else {
            LOGF_WARN("\"%s\" doesn't exist", file.c_str());
        }
#endif
        return exists;
    }

    inline bool DirectoryExists(const std::string& dir) noexcept
    {
        std::error_code ec;
        bool exists = std::filesystem::exists(Util::PathFromString(dir), ec);
        return exists && !ec;
    }

    // http://www.martinbroadhurst.com/how-to-trim-a-stdstring.html
    inline std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") noexcept
    {
        str.erase(0, str.find_first_not_of(chars));
        return str;
    }

    inline std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ") noexcept
    {
        str.erase(str.find_last_not_of(chars) + 1);
        return str;
    }

    inline std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ") noexcept
    {
        return ltrim(rtrim(str, chars), chars);
    }

    inline bool ContainsInsensitive(const char* haystack, const char* needle) noexcept
    {
        size_t length = SDL_strlen(needle);
        while (*haystack) {
            if (SDL_strncasecmp(haystack, needle, length) == 0) {
                return true;
            }
            ++haystack;
        }
        return false;
    }

    inline bool StringEqualsInsensitive(const std::string& string1, const std::string string2) noexcept
    {
        if (string1.length() != string2.length()) return false;
        return ContainsInsensitive(string1.c_str(), string2.c_str());
    }

    inline bool StringEndsWith(const std::string& string, const std::string& ending) noexcept
    {
        if (string.length() >= ending.length()) {
            return (0 == string.compare(string.length() - ending.length(), ending.length(), ending));
        }
        return false;
    }

    inline bool StringStartsWith(const std::string& string, const std::string& start) noexcept
    {
        if (string.length() >= start.length()) {
            for (int i = 0; i < start.size(); i++) {
                if (string[i] != start[i]) return false;
            }
            return true;
        }
        return false;
    }

    struct FileDialogResult {
        std::vector<std::string> files;
    };

    using FileDialogResultHandler = std::function<void(FileDialogResult&)>;

    void OpenFileDialog(const std::string& title,
        const std::string& path,
        FileDialogResultHandler&& handler,
        bool multiple = false,
        const std::vector<const char*>& filters = {},
        const std::string& filterText = "") noexcept;

    void SaveFileDialog(const std::string& title,
        const std::string& path,
        FileDialogResultHandler&& handler,
        const std::vector<const char*>& filters = {},
        const std::string& filterText = "") noexcept;

    void OpenDirectoryDialog(const std::string& title,
        const std::string& path,
        FileDialogResultHandler&& handler) noexcept;

    enum class YesNoCancel {
        Yes,
        No,
        Cancel
    };

    using YesNoDialogResultHandler = std::function<void(YesNoCancel)>;
    void YesNoCancelDialog(const std::string& title, const std::string& message, YesNoDialogResultHandler&& handler);

    void MessageBoxAlert(const std::string& title, const std::string& message) noexcept;

    std::string Resource(const std::string& path) noexcept;

    inline std::string Prefpath(const std::string& path = std::string()) noexcept
    {
        static const char* const cachedPref = SDL_GetPrefPath("OFS", "OFS3_data");
        static const std::filesystem::path prefPath = Util::PathFromString(cachedPref);
        if (!path.empty()) {
            std::filesystem::path rel = Util::PathFromString(path);
            rel.make_preferred();
            return (prefPath / rel).u8string();
        }
        return prefPath.u8string();
    }

    inline std::string PrefpathOFP(const std::string& path) noexcept
    {
        static const char* const cachedPref = SDL_GetPrefPath("OFS", "OFP_data");
        static const std::filesystem::path prefPath = Util::PathFromString(cachedPref);
        std::filesystem::path rel = Util::PathFromString(path);
        rel.make_preferred();
        return (prefPath / rel).u8string();
    }

    inline bool CreateDirectories(const std::filesystem::path& dirs) noexcept
    {
        std::error_code ec;
#ifdef WIN32
        if (dirs.u8string().size() >= _MAX_PATH) {
            auto pString = dirs.u8string();
            auto max = WindowsMaxPath(pString.c_str(), pString.size());
            std::filesystem::create_directories(max, ec);
            if (ec) {
                LOGF_ERROR("Failed to create directory: %s", ec.message().c_str());
                return false;
            }
            return true;
        }
#endif
        std::filesystem::create_directories(dirs, ec);
        if (ec) {
            LOGF_ERROR("Failed to create directory: %s", ec.message().c_str());
            return false;
        }
        return true;
    }

    bool SavePNG(const std::string& path, void* buffer, int32_t width, int32_t height, int32_t channels = 3, bool flipVertical = true) noexcept;

    std::filesystem::path FfmpegPath() noexcept;

    const char* Format(const char* fmt, ...) noexcept;

    inline const char* FormatBytes(size_t bytes) noexcept
    {
        if (bytes < 1024) {
            return Util::Format("%lld bytes", bytes); // bytes
        }
        else if (bytes >= 1024 && bytes < (1024 * 1024)) {
            return Util::Format("%0.2lf KB", bytes / 1024.0); // kilobytes
        }
        else if (bytes >= (1024 * 1024) && bytes < (1024 * 1024 * 1024)) {
            return Util::Format("%0.2lf MB", bytes / (1024.0 * 1024.0)); // megabytes
        }
        else /*if (bytes > (1024 * 1024 * 1024))*/ {
            return Util::Format("%0.2lf GB", bytes / (1024.0 * 1024.0 * 1024.0)); // gigabytes
        }
    }

    inline bool InMainThread() noexcept
    {
        static auto Main = SDL_ThreadID();
        return SDL_ThreadID() == Main;
    }

    // https://stackoverflow.com/questions/56940199/how-to-capture-a-unique-ptr-in-a-stdfunction
    template<class F>
    auto MakeSharedFunction(F&& f)
    {
        return
            [pf = std::make_shared<std::decay_t<F>>(std::forward<F>(f))](auto&&... args) -> decltype(auto) {
                return (*pf)(decltype(args)(args)...);
            };
    }

    void InitRandom() noexcept;
    float NextFloat() noexcept;
    uint32_t RandomColor(float s, float v, float alpha = 1.f) noexcept;
};

#define FMT(fmt, ...) Util::Format(fmt, __VA_ARGS__)

template<typename FlagBitsType>
struct FlagTraits {
    static constexpr bool isBitmask = false;
};

template<typename BitType>
class Flags {
public:
    using MaskType = typename std::underlying_type<BitType>::type;

private:
    MaskType _mask;

public:
    // constructors
    constexpr Flags() noexcept : _mask(0) {}

    constexpr Flags(BitType bit) noexcept : _mask(static_cast<MaskType>(bit)) {}

    constexpr Flags(Flags<BitType> const& rhs) noexcept = default;

    constexpr explicit Flags(MaskType flags) noexcept : _mask(flags) {}

    // helpers
    template<typename S>
    void serialize(S& s)
    {
        s.template value<sizeof(MaskType)>(_mask);
    }

    // relational operators
    constexpr bool operator<(Flags<BitType> const& rhs) const noexcept
    {
        return _mask < rhs._mask;
    }

    constexpr bool operator<=(Flags<BitType> const& rhs) const noexcept
    {
        return _mask <= rhs._mask;
    }

    constexpr bool operator>(Flags<BitType> const& rhs) const noexcept
    {
        return _mask > rhs._mask;
    }

    constexpr bool operator>=(Flags<BitType> const& rhs) const noexcept
    {
        return _mask >= rhs._mask;
    }

    constexpr bool operator==(Flags<BitType> const& rhs) const noexcept
    {
        return _mask == rhs._mask;
    }

    constexpr bool operator!=(Flags<BitType> const& rhs) const noexcept
    {
        return _mask != rhs._mask;
    }

    // logical operator
    constexpr bool operator!() const noexcept
    {
        return !_mask;
    }

    // bitwise operators
    constexpr Flags<BitType> operator&(Flags<BitType> const& rhs) const noexcept
    {
        return Flags<BitType>(_mask & rhs._mask);
    }

    constexpr Flags<BitType> operator|(Flags<BitType> const& rhs) const noexcept
    {
        return Flags<BitType>(_mask | rhs._mask);
    }

    constexpr Flags<BitType> operator^(Flags<BitType> const& rhs) const noexcept
    {
        return Flags<BitType>(_mask ^ rhs._mask);
    }

    constexpr Flags<BitType> operator~() const noexcept
    {
        return Flags<BitType>(_mask ^ FlagTraits<BitType>::allFlags._mask);
    }

    // assignment operators
    constexpr Flags<BitType>& operator=(Flags<BitType> const& rhs) noexcept = default;

    constexpr Flags<BitType>& operator|=(Flags<BitType> const& rhs) noexcept
    {
        _mask |= rhs._mask;
        return *this;
    }

    constexpr Flags<BitType>& operator&=(Flags<BitType> const& rhs) noexcept
    {
        _mask &= rhs._mask;
        return *this;
    }

    constexpr Flags<BitType>& operator^=(Flags<BitType> const& rhs) noexcept
    {
        _mask ^= rhs._mask;
        return *this;
    }

    // cast operators
    explicit constexpr operator bool() const noexcept
    {
        return !!_mask;
    }

    explicit constexpr operator MaskType() const noexcept
    {
        return _mask;
    }
};

// relational operators only needed for pre C++20
template<typename BitType>
constexpr bool operator<(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator>(bit);
}

template<typename BitType>
constexpr bool operator<=(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator>=(bit);
}

template<typename BitType>
constexpr bool operator>(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator<(bit);
}

template<typename BitType>
constexpr bool operator>=(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator<=(bit);
}

template<typename BitType>
constexpr bool operator==(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator==(bit);
}

template<typename BitType>
constexpr bool operator!=(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator!=(bit);
}

// bitwise operators
template<typename BitType>
constexpr Flags<BitType> operator&(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator&(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator|(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator|(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator^(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator^(bit);
}

// bitwise operators on BitType
template<typename BitType,
    typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator&(BitType lhs, BitType rhs) noexcept
{
    return Flags<BitType>(lhs) & rhs;
}

template<typename BitType,
    typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator|(BitType lhs, BitType rhs) noexcept
{
    return Flags<BitType>(lhs) | rhs;
}

template<typename BitType,
    typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator^(BitType lhs, BitType rhs) noexcept
{
    return Flags<BitType>(lhs) ^ rhs;
}

template<typename BitType,
    typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator~(BitType bit) noexcept
{
    return ~(Flags<BitType>(bit));
}
