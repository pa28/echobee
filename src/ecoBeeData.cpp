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

/**
 * @class EcoBeDataFile
 * @brief Abstract the CSV file made available by ecobee
 */
class EcoBeeDataFile {
public:
    using DataLine = std::vector<std::string>;
    using DataFile = std::vector<DataLine>;

    /**
     * Indexes into the header and data vectors.
     */
    enum DataIndex {
        Date [[maybe_unused]],
        Time [[maybe_unused]],
        SystemSetting [[maybe_unused]],
        SystemMode [[maybe_unused]],
        CalendarEvent [[maybe_unused]],
        ProgramMode [[maybe_unused]],
        CoolSetTemp [[maybe_unused]],
        HeatSetTemp [[maybe_unused]],
        CurrentTemp [[maybe_unused]],
        CurrentHumidity [[maybe_unused]],
        OutdoorTemp [[maybe_unused]],
        WindSpeed [[maybe_unused]],
        CoolStage1Sec [[maybe_unused]],
        HeadStage1Sec [[maybe_unused]],
        FanSec [[maybe_unused]],
        DMOffset [[maybe_unused]],
        ThermostatTemp [[maybe_unused]],
        ThermostatHumidity [[maybe_unused]],
        ThermostatMotion [[maybe_unused]],
        ThermostatAirPressure [[maybe_unused]],
        Sensor0Temp [[maybe_unused]],
        Sensor0Motion [[maybe_unused]],
    };
private:
    std::array<char, 3> footPrint{'\357', '\273', '\277'};  ///< Not really sure what this is, let's call it a footprint.
    bool fileGood{true};    ///< True if the file passes parsing.
    std::vector<std::string> header{};  ///< The data item headers.
    DataFile dataFile;                  ///< The data in the file.

public:
    explicit operator bool() const noexcept {
        return fileGood;
    }

    void processDataFile(const filesystem::path &file);

    bool processHeader(const std::string& line);

    bool processData(const std::string& line);

    [[maybe_unused]] [[nodiscard]] size_t sensorCount() const {
        if (fileGood) {
            return (header.size() - Sensor0Temp) / 2;
        }
        return 0;
    }

    [[maybe_unused]] [[nodiscard]] std::optional<const std::string> getHeader(DataIndex dataIndex) const {
        auto idx = static_cast<size_t>(dataIndex);
        if (fileGood && idx < header.size())
            return header[idx];
        return std::nullopt;
    }

    [[maybe_unused]] [[nodiscard]] std::optional<const std::string> getData(DataIndex dataIndex, const DataLine &dataLine) const {
        auto idx = static_cast<size_t>(dataIndex);
        if (fileGood && idx < header.size())
            return dataLine[idx];
        return std::nullopt;
    }

    [[maybe_unused]] [[nodiscard]] auto begin() const {
        return dataFile.cbegin();
    }

    [[maybe_unused]] [[nodiscard]] auto end() const {
        return dataFile.cend();
    }
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
    for ( std::string::size_type start = 0, pos; start < line.length(); start = pos + 1) {
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
    DataLine data;
    for ( std::string::size_type start = 0, pos; start < line.length(); start = pos + 1) {
        pos = line.find(',', start);
        if (pos == std::string::npos) {
            data.push_back(line.substr(start));
            break;
        } else {
            data.push_back(line.substr(start, pos - start));
        }
    }
    if (data.size() == header.size()) {
        dataFile.push_back(data);
        return true;
    }
    return false;
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
                                              dir_entry.path().filename().string().rfind(dataPrefix.value(), 0) == 0) {
                                              ecoBeeData.processDataFile(dir_entry);
                                              for (const auto &line : ecoBeeData) {
                                                  std::cout << ecoBeeData.getData(EcoBeeDataFile::DataIndex::Date, line).value() << 'T'
                                                            << ecoBeeData.getData(EcoBeeDataFile::DataIndex::Time, line).value() << " EST "
                                                            << ecoBeeData.getData(EcoBeeDataFile::DataIndex::CurrentTemp, line).value() << '\n';
                                              }
                                          }
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
