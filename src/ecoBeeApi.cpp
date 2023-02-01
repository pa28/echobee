//
// Created by richard on 30/01/23.
//
// https://www.normalexception.net/Code-Development/ecobee3-api

#include "Api.h"
#include <fmt/format.h>
#include <sstream>
#include <fstream>
#include "XDGFilePaths.h"
#include "InputParser.h"

using namespace ecoBee;
using json = nlohmann::json;

static constexpr std::array<std::string_view,12> DataColumns = {
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

auto firstValidFile(const std::vector<std::filesystem::path>& paths) {
    for (const auto& path : paths) {
        if (!path.empty() && exists(path) && is_regular_file(path))
            return path;
    }

    return std::filesystem::path{};
}

int main(int argc, char **argv) {

    InputParser inputParser{argc, argv};

    xdg::Environment &environment{xdg::Environment::getEnvironment(false)};
    xdg::XDGFilePaths::XDG_Path_Set filePathSet{}, appAuthPathSet{};
    auto appAuthPath = firstValidFile(environment.get_configuration_paths("appAuth.json"));
    auto jsonAccessPath = firstValidFile(environment.get_configuration_paths("accessToken.json"));
    auto thermostatPath = firstValidFile(environment.get_configuration_paths("thermostat.json"));
    auto dataPath = firstValidFile(environment.get_configuration_paths("2023-01-31:185--2023-01-31:250.json"));

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

    ifs.open(dataPath);
    auto runtimeData = json::parse(ifs);
    ifs.close();
    processRuntimeData(runtimeData);

    std::string ecoBeeTokenURL{"https://api.ecobee.com/token"};
    std::string apiKey = appAuth["API_Key"];
    std::string token = jsonAccess["refresh_token"];
    std::string access = jsonAccess["access_token"];

#if 1
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
    std::cout << poll.dump(4) << '\n';
    std::string thermostat{};
    for (const auto& t : poll["revisionList"]) {
        // 421866388280:Thermostat:true:230109012202:221221084857:230131181519:230131175500
        thermostat = t;
        std::cout << thermostat << '\n';
    }

#endif
    std::cout << thermostatJson.dump(4) << '\n';
    size_t idx = 0;
    for (std::string::size_type pos = thermostat.find(':'); (pos = thermostat.find(':')) != std::string::npos; thermostat.erase(0, pos + 1)) {
        token = thermostat.substr(0,pos);
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
        std::cout << token << '\n';
    }
    if (idx == 6) {
        thermostatJson["internalUpdate"] = thermostat != thermostatJson["internalRevision"];
        thermostatJson["internalRevision"] = thermostat;
    }
    std::cout << thermostat << '\n';

    std::cout << thermostatJson.dump(4) << '\n';

    if (thermostatJson["runtimeUpdate"]) {
        auto [startDate, start, endDate, end, lastData] = runtimeIntervals(thermostatJson["lastData"]);
        auto fileName = fmt::format("{}:{}--{}:{}.json",startDate,start,endDate,end);
        if (runtimeReport(jsonAccess["access_token"], runtimeReportUrl(DataColumns, true, startDate, start, endDate, end)) == ApiStatus::OK) {
            std::ofstream ofs(thermostatPath);
            ofs << thermostatJson.dump(4) << '\n';
            ofs.close();
        }
    }

    return 0;
}