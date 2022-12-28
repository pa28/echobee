//
// Created by richard on 2022-12-27.
//

#include <unistd.h>
#include <pwd.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include "ConfigFile.h"
#include "InputParser.h"
#include "XDGFilePaths.h"

using namespace std;

int main(int argc, char **argv) {
    static constexpr std::string_view ConfigOption = "--config";

    enum class ConfigItem {
        DataPrefix,
        DataPath,
    };

    std::vector<ConfigFile::Spec> ConfigSpec
            {{
                     {"dataPath", ConfigItem::DataPath},
                     {"dataPrefix", ConfigItem::DataPrefix},
             }};

    std::optional<std::string> dataPath{};
    std::optional<std::string> dataPrefix{};

    try {
        xdg::Environment &environment{xdg::Environment::getEnvironment(true)};
        filesystem::path configFilePath = environment.appResourcesAppend("config.txt");

        InputParser inputParser{argc, argv};

        if (inputParser.cmdOptionExists(ConfigOption))
            configFilePath = std::filesystem::path{inputParser.getCmdOption(ConfigOption)};

        ConfigFile configFile{configFilePath};
        if (auto status = configFile.open(); status == ConfigFile::OK) {
            bool validFile{true};
            configFile.process(ConfigSpec, [&](std::size_t idx, const std::string_view &data) {
                bool validValue{false};
                switch (static_cast<ConfigItem>(idx)) {
                    case ConfigItem::DataPath:
                        dataPath = ConfigFile::parseText(data, [](char c) {
                            return ConfigFile::isNameChar(c) || c == '/';
                        });
                        validValue = dataPath.has_value();
                        break;
                    case ConfigItem::DataPrefix:
                        dataPrefix = ConfigFile::parseText(data, [](char c) {
                            return ConfigFile::isNameChar(c);
                        });
                        validValue = dataPath.has_value();
                        break;
                    default:
                        break;
                }
                validFile = validFile & validValue;
                if (!validValue) {
                    std::cerr << "Invalid config value: " << ConfigSpec[idx].mKey << '\n';
                }
            });
            configFile.close();

            if (dataPath.has_value() && dataPrefix.has_value()) {
                filesystem::path dataFilePath;
                if (dataPath.value()[0] == '~') {
                    struct passwd *pw = getpwuid(geteuid());
                    dataFilePath.append(pw->pw_dir);
                    if (auto n = dataPath->find_first_of('/');
                        n != string::npos && n < dataPath->length()) {
                        dataFilePath.append(dataPath->substr(n+1));
                    }
                } else {
                    dataFilePath.append(dataPath.value());
                }
                std::ranges::for_each( std::filesystem::directory_iterator{dataFilePath},
                                       [dataPrefix](const auto& dir_entry) {
                                           if (dir_entry.is_regular_file() &&
                                                dir_entry.path().filename().string().rfind(dataPrefix.value(), 0) == 0)
                                                   std::cout << dir_entry.path() << '\n';
                                       } );
            }
        } else {
            return 1;
        }
    } catch (exception &e) {
        cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
