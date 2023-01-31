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

auto firstValidFile(std::vector<std::filesystem::path> paths) {
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

    if (appAuthPath.empty() || jsonAccessPath.empty()) {
        throw std::runtime_error("Can not find required configuration files.");
    }

    std::ifstream ifs(appAuthPath);
    auto appAuth = json::parse(ifs);
    ifs.close();

    ifs.open(jsonAccessPath);
    auto jsonAccess = json::parse(ifs);
    ifs.close();

    std::string ecoBeeTokenURL{"https://api.ecobee.com/token"};
    std::string apiKey = appAuth["API_Key"];
    std::string token = jsonAccess["refresh_token"];

    refreshAccessToken(jsonAccess, ecoBeeTokenURL, apiKey, token);
    std::ofstream ofs{jsonAccessPath};
    ofs << jsonAccess.dump(4) << '\n';
    ofs.close();

    runtimeReport(jsonAccess["access_token"]);
}