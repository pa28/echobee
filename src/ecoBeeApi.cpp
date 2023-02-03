//
// Created by richard on 30/01/23.
//
// https://www.normalexception.net/Code-Development/ecobee3-api

#include "Api.h"
#include <fmt/format.h>
#include <sstream>
#include "XDGFilePaths.h"
#include "InputParser.h"

using namespace ecoBee;
using json = nlohmann::json;

static constexpr std::array<std::string_view, 12> DataColumns = {
        "hvacMode",
        "zoneHvacMode",
        "zoneClimate",
        "zoneAveTemp",
        "auxHeat1",
        "compCool1",
        "fan",
        "zoneHeatTemp",
        "zoneCoolTemp",
        "outdoorTemp",
        "outdoorHumidity",
        "wind",
};

auto firstValidFile(const std::vector<std::filesystem::path> &paths) {
    for (const auto &path: paths) {
        if (!path.empty() && exists(path) && is_regular_file(path))
            return path;
    }

    return std::filesystem::path{};
}

enum class ConfigItem {
    InfluxTLS,
    InfluxHost,
    InfluxPort,
    InfluxDb,
    DeleteProcessed,
};

std::vector<ConfigFile::Spec> ConfigSpec
        {{
                 {"influxTLS", ConfigItem::InfluxTLS},
                 {"influxHost", ConfigItem::InfluxHost},
                 {"influxPort", ConfigItem::InfluxPort},
                 {"influxDb", ConfigItem::InfluxDb},
                 {"deleteProcessed", ConfigItem::DeleteProcessed},
         }};

int main(int argc, char **argv) {
    static constexpr std::string_view ConfigOption = "--config";
    static constexpr std::string_view ProcessOption = "--process";

    InfluxConfig influxConfig{};
    InputParser inputParser{argc, argv};

    xdg::Environment &environment{xdg::Environment::getEnvironment(false)};
    xdg::XDGFilePaths::XDG_Path_Set filePathSet{}, appAuthPathSet{}, configPathSet{};
    auto appAuthPath = firstValidFile(environment.get_configuration_paths("appAuth.json"));
    auto jsonAccessPath = firstValidFile(environment.get_configuration_paths("accessToken.json"));
    auto thermostatPath = firstValidFile(environment.get_configuration_paths("thermostat.json"));

    if (inputParser.cmdOptionExists(ConfigOption))
        configPathSet.emplace_back(inputParser.getCmdOption(ConfigOption));
    else
        configPathSet = environment.get_configuration_paths("config.txt");

    auto fileItr = xdg::Environment::firstExistingFile(configPathSet);
    if (!fileItr) {
        std::cerr << "None of the specified files exists and is a regular file:\n";
        for (const auto &filePath: configPathSet) {
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
                case ConfigItem::InfluxTLS:
                    influxConfig.influxTLS = ConfigFile::parseBoolean(data);
                    validValue = influxConfig.influxTLS.has_value();
                    break;
                case ConfigItem::DeleteProcessed:
                    influxConfig.deleteProcessed = ConfigFile::parseBoolean(data);
                    validValue = influxConfig.deleteProcessed.has_value();
                    break;
                case ConfigItem::InfluxHost:
                    influxConfig.influxHost = ConfigFile::parseText(data, [](char c) {
                        return ConfigFile::isalnum(c) || c == '.';
                    });
                    validValue = influxConfig.influxHost.has_value();
                    break;
                case ConfigItem::InfluxPort:
                    influxConfig.influxPort = ConfigFile::safeConvert<long>(data);
                    validValue = influxConfig.influxPort.has_value() && influxConfig.influxPort.value() > 0;
                    break;
                case ConfigItem::InfluxDb:
                    influxConfig.influxDb = ConfigFile::parseText(data, [](char c) {
                        return ConfigFile::isalnum(c) || c == '_';
                    });
                    validValue = influxConfig.influxDb.has_value();
                    break;
                default:
                    break;
            }
            validFile = validFile & validValue;
            if (!validValue) {
                std::cerr << "Invalid config value: " << ConfigSpec[idx].mKey << '\n';
            }
        });
    }
    configFile.close();

    if (inputParser.cmdOptionExists(ProcessOption)) {
        auto dataPath = firstValidFile(environment.get_configuration_paths(inputParser.getCmdOption(ProcessOption)));
        std::ifstream ifs{dataPath};
        auto report = json::parse(ifs);
        ifs.close();
        [[maybe_unused]] auto lastData = processRuntimeData(report, influxConfig);
        exit(0);
    }

    if (appAuthPath.empty() || jsonAccessPath.empty()) {
        throw std::runtime_error("Can not find required configuration files.");
    }

    std::ifstream ifs(appAuthPath);
    auto appAuth = json::parse(ifs);
    ifs.close();

    ifs.open(jsonAccessPath);
    auto jsonAccess = json::parse(ifs);
    ifs.close();

    ifs.open(thermostatPath);
    auto thermostatJson = json::parse(ifs);
    ifs.close();

    std::string ecoBeeTokenURL{"https://api.ecobee.com/token"};
    std::string apiKey = appAuth["API_Key"];
    std::string token = jsonAccess["refresh_token"];
    std::string access = jsonAccess["access_token"];

    json poll{};
    if (statusPoll(poll, access) == ApiStatus::TokenExpired) {
        if (refreshAccessToken(jsonAccess, ecoBeeTokenURL, apiKey, token) == ApiStatus::OK) {
            std::ofstream ofs{jsonAccessPath};
            ofs << jsonAccess.dump(4) << '\n';
            ofs.close();
            access = jsonAccess["access_token"];
        } else {
            std::cerr << "Can not refresh access token.\n";
            return 1;
        }
        if (statusPoll(poll, access) != ApiStatus::OK) {
            throw ApiError("API polling error.");
        }
    }
    std::string thermostat{};

    size_t idx = 0;
    for (std::string::size_type pos; (pos = thermostat.find(':')) != std::string::npos; thermostat.erase(0, pos + 1)) {
        token = thermostat.substr(0, pos);
        switch (idx) {
            case 0:
                if (token != thermostatJson["id"])
                    thermostat.clear();
                break;
            case 1:
                thermostatJson["name"] = token;
                break;
            case 2:
                thermostatJson["connected"] = token == "true";
                break;
            case 3:
                thermostatJson["thermostatRevision"] = token;
                break;
            case 4:
                thermostatJson["alertsRevision"] = token;
                break;
            case 5:
                thermostatJson["runtimeUpdate"] = token != thermostatJson["runtimeRevision"];
                thermostatJson["runtimeRevision"] = token;
                break;
            default:
                break;
        }
        ++idx;
    }
    if (idx == 6) {
        thermostatJson["internalUpdate"] = thermostat != thermostatJson["internalRevision"];
        thermostatJson["internalRevision"] = thermostat;
    }

    if (thermostatJson["runtimeUpdate"]) {
        auto [startDate, start, endDate, end, lastData] = runtimeIntervals(thermostatJson["lastData"]);
        auto fileName = fmt::format("{}:{}--{}:{}.json", startDate, start, endDate, end);
        json report{};
        if (runtimeReport(report, jsonAccess["access_token"],
                          runtimeReportUrl(DataColumns, true, startDate, start, endDate, end)) == ApiStatus::OK) {
            std::ofstream ofs(thermostatPath);
            ofs << thermostatJson.dump(4) << '\n';
            ofs.close();
            auto dataPath = environment.get_configuration_paths(fileName).front();
            ofs.open(dataPath);
            ofs << report.dump(4) << '\n';
            ofs.close();
            processRuntimeData(report, influxConfig);
        }
    }

    return 0;
}