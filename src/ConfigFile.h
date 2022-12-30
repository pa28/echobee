/**
 * @file ConfigFile.h
 * @author Richard Buckley <richard.buckley@ieee.org>
 * @version 1.0
 * @date 2021-09-02
 */

#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <utility>
#include <charconv>
#include <cstring>
#include <optional>

/**
 * @class ConfigFile
 * @brief
 */
class ConfigFile {
public:
    enum Status {
        OK,
        NO_FILE,
        OPEN_FAIL,
    };

    struct Spec {
        std::string_view mKey{};
        std::size_t mIdx{};

        template<typename IndexType>
        Spec(const std::string_view key, IndexType idx) : mKey(key), mIdx(static_cast<std::size_t>(idx)) {}
    };

protected:
    std::filesystem::path mConfigFilePath{};
    std::ifstream mIstrm{};
    std::string_view mCallsign{};
    std::string_view mPasscode{};
    double mLatitude{0};
    double mLongitude{0};
    long mRadius{0};

    std::string::size_type matchKey(std::string::iterator first, std::string::iterator last, const std::string_view& key);


public:
    ConfigFile() = delete;

    explicit ConfigFile(std::filesystem::path configFilePath) : mConfigFilePath(std::move(configFilePath)) {}

    Status open();

    Status process(const std::vector<ConfigFile::Spec>& configSpecs, const std::function<void(std::size_t, const std::string_view&)>& callback);

    void close();

    template<typename T>
    std::optional<T> safeConvert(const std::string_view &stringView) {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>, "Type not supported.");

        if (stringView.empty())
            return std::nullopt;

        if constexpr (std::is_integral_v<T>) {
            // Use simpler (and safer?) std::from_chars() for integers because if is available.
            T value;
            auto[ptr, ec] = std::from_chars(stringView.data(), stringView.data() + stringView.length(), value);
            if (ec == std::errc() && ptr == stringView.data() + stringView.length())
                return value;
            else
                return std::nullopt;
        } else if constexpr (std::is_floating_point_v<T>) {
            // Use more cumbersome std::strtod() for doubles.
            static constexpr std::size_t BufferSize = 64;
            static char buffer[BufferSize];

            if (stringView.length() > BufferSize - 1)
                throw std::range_error("String to long to convert.");

            char *endPtr{nullptr};
            std::memset(buffer, 0, BufferSize);
            std::strncpy(buffer, stringView.data(), stringView.length());
            std::optional<T> value = std::strtod(buffer, &endPtr);
            if ((endPtr >= buffer) && (static_cast<unsigned long>(endPtr - buffer) < stringView.length()))
                value = std::nullopt;
            return value;
        }
    }

    static char nullFilter(char c) { return c; }

    static bool isalnum(char c) { return ::isalnum(c) != 0;}

    static bool isdigit(char c) { return ::isdigit(c) != 0;}

    static bool isNameChar(char ch) { return ::isprint(ch) != 0 && ::isspace(ch) == 0 && ch != '/';}

    static char toupper(char c) { return static_cast<char>(::toupper(c)); }

    static std::optional<std::string> parseText(const std::string_view &text, const std::function<bool(char)>& valid,
                                                const std::function<char(char)>& filter = ConfigFile::nullFilter) {
        std::string result{};
        for (auto c : text) {
            if (valid(c)) {
                result.append(1, filter(c));
            } else {
                return std::nullopt;
            }
        }
        return result;
    }

    /**
     * @brief Parse a boolean value which may be one of "0" "false" "no" "1" "true" "yes" case insensitive.
     * @param stringView the string to parse
     * @return true, false, or std::nullopt
     */
    static std::optional<bool> parseBoolean(const std::string_view &stringView) {
        if (stringView == "0")
            return false;
        if (stringView == "1")
            return true;

        std::string upperCase{stringView};
        std::transform(upperCase.begin(), upperCase.end(), upperCase.begin(),
                       [](unsigned char c) { return std::toupper(c); });

        if (upperCase == "NO" || upperCase == "FALSE")
            return false;
        if (upperCase == "YES" || upperCase == "TRUE")
            return true;

        return std::nullopt;
    }

    /**
     * @brief Parse a std::string_view to return a std::filesystem::path or std::nullopt on failure.
     * @param stringView A string view containing the desired filesystem path.
     * @return a std::optional with a filesystem::path or nullopt.
     */
    static std::optional<std::filesystem::path> parseFilesystemPath(const std::string_view &stringView) {
        auto pathString = ConfigFile::parseText(stringView, [](char c) {
            return ConfigFile::isNameChar(c) || c == '/';
        });
        if (pathString.has_value()) {
            std::filesystem::path dataFilePath;
            if (pathString.value()[0] == '~') {
                struct passwd *pw = getpwuid(geteuid());
                dataFilePath.append(pw->pw_dir);
                if (auto n = pathString->find_first_of('/');
                        n != std::string::npos && n < pathString->length()) {
                    dataFilePath.append(pathString->substr(n + 1));
                }
            } else {
                dataFilePath.append(pathString.value());
            }
            return dataFilePath;
        }
        return std::nullopt;
    }

};

