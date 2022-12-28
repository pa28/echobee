//
// Created by richard on 2022-12-27.
//

#include <unistd.h>
#include <pwd.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <array>
#include <vector>
#include "ConfigFile.h"
#include "InputParser.h"
#include "XDGFilePaths.h"

using namespace std;

class EcoBeeDataFile {
private:
    std::array<char, 3> footPrint{'\357', '\273', '\277'};
    bool fileGood{true};
    std::vector<std::string> header{};
    std::vector<std::string> data;

public:
    explicit operator bool() const noexcept {
        return fileGood;
    }

    void processDataFile(const filesystem::path &file);

    bool processHeader(const std::string& line);

    bool processData(const std::string& line);
};

void EcoBeeDataFile::processDataFile(const filesystem::path &file) {
    std::cout << file.string() << ": ";
    std::ifstream strm(file.c_str());
    if (strm) {
        bool footPrintGood{false};
        std::cout << "Open\n";
        std::string line;
        bool headerRead = false;
        while (fileGood && std::getline(strm, line)) {
            // Check the footprint.
            if (!footPrintGood) {
                if (line.length() > footPrint.size()) {
                    auto lineItr = line.begin();
                    for (const auto fpReq: footPrint) {
                        if (fpReq != *lineItr++) {
                            strm.close();
                            return;
                        }
                    }
                }
                footPrintGood = true;
                continue;
            }

            if (!line.empty()) {
                auto c1 = line.at(0);
                if (c1 != '#') {
                    if (headerRead) {
                        fileGood &= processData(line);
                    } else {
                        if (headerRead = fileGood = processHeader(line); !headerRead)
                            return;
                    }
                }
            }
        }
        strm.close();
    } else {
        std::cerr << strerror(errno) << '\n';
    }
}

bool EcoBeeDataFile::processHeader(const std::string& line) {
    for ( std::string::size_type start = 0, pos = 0; start < line.length(); start = pos + 1) {
        pos = line.find(',', start);
        if (pos == std::string::npos) {
            header.push_back(line.substr(start));
            return true;
        } else {
            header.push_back(line.substr(start, pos - start));
        }
    }
    return false;
}

bool EcoBeeDataFile::processData(const string &line) {
    data.clear();
    for ( std::string::size_type start = 0, pos = 0; start < line.length(); start = pos + 1) {
        pos = line.find(',', start);
        if (pos == std::string::npos) {
            data.push_back(line.substr(start));
            return data.size() == header.size();
        } else {
            data.push_back(line.substr(start, pos - start));
        }
    }
    return data.size() == header.size();
}

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
                        dataFilePath.append(dataPath->substr(n + 1));
                    }
                } else {
                    dataFilePath.append(dataPath.value());
                }
                std::ranges::for_each(std::filesystem::directory_iterator{dataFilePath},
                                      [dataPrefix](const auto &dir_entry) {
                                          EcoBeeDataFile ecoBeeData{};
                                          if (dir_entry.is_regular_file() &&
                                              dir_entry.path().filename().string().rfind(dataPrefix.value(), 0) == 0)
                                              ecoBeeData.processDataFile(dir_entry);
                                      });
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
