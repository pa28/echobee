//
// Created by richard on 2022-12-27.
//

#include <pwd.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <array>
#include <vector>
#include "ConfigFile.h"
#include "InputParser.h"
#include "XDGFilePaths.h"
#include "InfluxPush.h"

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
        Sensor1Temp [[maybe_unused]],
        Sensor1Motion [[maybe_unused]],
        Sensor2Temp [[maybe_unused]],
        Sensor2Motion [[maybe_unused]],
        Sensor3Temp [[maybe_unused]],
        Sensor3Motion [[maybe_unused]],
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

    static std::string escapeHeader(const std::string& hdr);

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

std::string EcoBeeDataFile::escapeHeader(const std::string& hdr) {
    auto workingHdr = hdr;
    if (auto pos = workingHdr.rfind(" ("); pos != std::string::npos) {
        workingHdr = workingHdr.substr(0, pos);
    }

    if (!workingHdr.empty())
        for( auto pos = workingHdr.rfind(' '); pos > 0 && pos != std::string::npos; pos = workingHdr.rfind(' ', pos)) {
            workingHdr.insert(pos, 1, '\\');
        }

    return workingHdr;
}

bool EcoBeeDataFile::processHeader(const std::string& line) {
    for ( std::string::size_type start = 0, pos; start < line.length(); start = pos + 1) {
        pos = line.find(',', start);
        if (pos == std::string::npos) {
            header.push_back(escapeHeader(line.substr(start)));
            return true;
        } else {
            header.push_back(escapeHeader(line.substr(start, pos - start)));
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
    std::optional<bool> influxTLS{false};
    std::optional<bool> deleteProcessed{false};
    std::optional<std::string> influxHost{"influx"};
    std::optional<std::string> influxDb{"ecobee"};
    std::optional<long> influxPort{8086};

    std::array<EcoBeeDataFile::DataIndex,10> reportedData = {
            EcoBeeDataFile::DataIndex::CurrentTemp,
            EcoBeeDataFile::DataIndex::CurrentHumidity,
            EcoBeeDataFile::DataIndex::OutdoorTemp,
            EcoBeeDataFile::DataIndex::Sensor0Temp,
            EcoBeeDataFile::DataIndex::Sensor1Temp,
            EcoBeeDataFile::DataIndex::Sensor2Temp,
            EcoBeeDataFile::DataIndex::Sensor3Temp,
            EcoBeeDataFile::DataIndex::ThermostatTemp,
            EcoBeeDataFile::DataIndex::CoolSetTemp,
            EcoBeeDataFile::DataIndex::HeatSetTemp,
    };

    enum class ConfigItem {
        DataPrefix,
        DataPath,
        InfluxTLS,
        InfluxHost,
        InfluxPort,
        InfluxDb,
        DeleteProcessed,
    };

    std::vector<ConfigFile::Spec> ConfigSpec
            {{
                     {"dataPath", ConfigItem::DataPath},
                     {"dataPrefix", ConfigItem::DataPrefix},
                     {"influxTLS", ConfigItem::InfluxTLS},
                     {"influxHost", ConfigItem::InfluxHost},
                     {"influxPort", ConfigItem::InfluxPort},
                     {"influxDb", ConfigItem::InfluxDb},
                     {"deleteProcessed", ConfigItem::DeleteProcessed},
             }};

    std::optional<std::filesystem::path> dataPath{};
    std::optional<std::string> dataPrefix{};

    try {
        xdg::Environment &environment{xdg::Environment::getEnvironment(false)};

        InputParser inputParser{argc, argv};

        xdg::XDGFilePaths::XDG_Path_Set configPathSet{};

        if (inputParser.cmdOptionExists(ConfigOption))
            configPathSet.emplace_back(inputParser.getCmdOption(ConfigOption));
        else
            configPathSet = environment.get_configuration_paths("config.txt");

        auto fileItr = xdg::Environment::firstExistingFile(configPathSet);
        if (!fileItr) {
            std::cerr << "None of the specified files exists and is a regular file:\n";
            for (const auto &filePath : configPathSet) {
                std::cerr << '\t' << filePath.string() << '\n';
            }
            return 1;
        }

        ConfigFile configFile{fileItr.value()};
        if (auto status = configFile.open(); status == ConfigFile::OK) {
            bool validFile{true};
            configFile.process(ConfigSpec, [&](std::size_t idx, const std::string_view &data) {
                bool validValue{false};
                switch (static_cast<ConfigItem>(idx)) {
                    case ConfigItem::DataPath:
                        dataPath = ConfigFile::parseFilesystemPath(data);
                        validValue = dataPath.has_value();
                        break;
                    case ConfigItem::DataPrefix:
                        dataPrefix = ConfigFile::parseText(data, [](char c) {
                            return ConfigFile::isNameChar(c);
                        });
                        validValue = dataPath.has_value();
                        break;
                    case ConfigItem::InfluxTLS:
                        influxTLS = ConfigFile::parseBoolean(data);
                        validValue = influxTLS.has_value();
                        break;
                    case ConfigItem::DeleteProcessed:
                        deleteProcessed = ConfigFile::parseBoolean(data);
                        validValue = deleteProcessed.has_value();
                        break;
                    case ConfigItem::InfluxHost:
                        influxHost = ConfigFile::parseText(data, [](char c) {
                            return ConfigFile::isalnum(c) || c == '.';
                        });
                        validValue = influxHost.has_value();
                        break;
                    case ConfigItem::InfluxPort:
                        influxPort = configFile.safeConvert<long>(data);
                        validValue = influxPort.has_value() && influxPort.value() > 0;
                        break;
                    case ConfigItem::InfluxDb:
                        influxDb = ConfigFile::parseText(data, [](char c) {
                            return ConfigFile::isalnum(c) || c == '_';
                        });
                        validValue = influxDb.has_value();
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

            if (validFile && dataPath.has_value() && dataPrefix.has_value()) {
                std::ranges::for_each(std::filesystem::directory_iterator{dataPath.value()},
                                      [&](const auto &dir_entry) {
                                          EcoBeeDataFile ecoBeeData{};
                                          InfluxPush influxPush(influxHost.value(), influxTLS.value(), influxPort.value(), influxDb.value());
                                          if (dir_entry.is_regular_file() &&
                                              dir_entry.path().filename().string().rfind(dataPrefix.value(), 0) == 0) {
                                              ecoBeeData.processDataFile(dir_entry);
                                              for (const auto &line : ecoBeeData) {
                                                  influxPush.newMeasurements();

                                                  std::cout << ecoBeeData.getData(EcoBeeDataFile::DataIndex::Date, line).value()
                                                            << ' '
                                                            << ecoBeeData.getData(EcoBeeDataFile::DataIndex::Time, line).value()
                                                            << '\r';
                                                  std::cout.flush();
                                                  influxPush.setMeasurementEpoch(ecoBeeData.getData(EcoBeeDataFile::DataIndex::Date, line).value(),
                                                                                 ecoBeeData.getData(EcoBeeDataFile::DataIndex::Time, line).value(),
                                                                                 "-0500");
                                                  for (const auto dataIdx : reportedData) {
                                                      influxPush.addMeasurement("Home ",ecoBeeData.getHeader(dataIdx),
                                                                                ecoBeeData.getData(dataIdx, line));
                                                  }

                                                  influxPush.pushData();
                                              }
                                              std::cout << '\n';
                                              if (deleteProcessed.has_value() && deleteProcessed.value()) {
                                                  std::error_code ec;
                                                  auto res = std::filesystem::remove(dir_entry, ec);
                                                  if (!res) {
                                                      std::cerr << ec << '\n';
                                                  }
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
