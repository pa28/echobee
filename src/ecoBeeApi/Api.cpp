//
// Created by richard on 30/01/23.
//

/*
 * Api.cpp Created by Richard Buckley (C) 30/01/23
 */

/**
 * @file Api.cpp
 * @author Richard Buckley <richard.buckley@ieee.org>
 * @version 1.0
 * @date 30/01/23
 */

#include <Info.hpp>
#include <ranges>
#include <fstream>
#include <ctime>
#include <sstream>
#include "Api.h"
#include "fmt/format.h"
#include "nlohmann/json.hpp"

namespace ecoBee {
    ApiStatus runtimeReport(const std::string &token, const std::string &url) {
        std::stringstream response{};
        cURLpp::Cleanup cleaner;
        cURLpp::Easy request;

        request.setOpt(new cURLpp::Options::Url(url));
        request.setOpt(new cURLpp::Options::Verbose(false));

        std::list<std::string> header;
        header.emplace_back("content-type: text/json;charset=UTF-8");
        auto authHeader = fmt::format("Authorization: Bearer {}", token);
        header.emplace_back(authHeader);
        request.setOpt(new cURLpp::Options::HttpHeader(header));

        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200 && code != 500)
            throw HtmlError(fmt::format("HTML error code: {}", code));

        auto report = nlohmann::json::parse(response);
        if (auto status = apiStatus(report["status"]["code"], report["status"]["message"]); status == ApiStatus::OK) {
            std::ofstream ofs("/home/richard/.config/ecoBeeApi/response.json");
            ofs << report.dump(4) << '\n';
            ofs.close();
            return status;
        } else {
            return status;
        }
    }

    ApiStatus thermostat(const std::string &token) {
        std::stringstream response{};
        cURLpp::Cleanup cleaner;
        cURLpp::Easy request;

        auto url = R"(https://api.ecobee.com/1/thermostat?format=json&body={"selection":{"selectionType":"registered",)"
                   R"(selectionMatch":"","includeRuntime":true}})";

        request.setOpt(new cURLpp::Options::Url(url));
        request.setOpt(new cURLpp::Options::Verbose(false));

        std::list<std::string> header;
        header.emplace_back("content-type: text/json");
        auto authHeader = fmt::format("Authorization: Bearer {}", token);
        header.emplace_back(authHeader);
        request.setOpt(new cURLpp::Options::HttpHeader(header));

        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200)
            throw HtmlError(fmt::format("HTML error code: {}", code));

        auto thermostat = nlohmann::json::parse(response);
        if (auto status = apiStatus(thermostat["status"]["code"], thermostat["status"]["code"]); status == ApiStatus::OK) {
            std::cout << thermostat.dump(4) << '\n';
            return status;
        } else {
            return status;
        }
    }

    ApiStatus refreshAccessToken(nlohmann::json &accessToken, const std::string &url, const std::string &apiKey,
                                 const std::string &token) {
        auto postData = fmt::format("grant_type=refresh_token&&code={}&client_id={}", token, apiKey);
        std::stringstream response{};
        cURLpp::Cleanup cleaner;
        cURLpp::Easy request;
        request.setOpt(new cURLpp::Options::Url(url));
        request.setOpt(new cURLpp::Options::Verbose(false));

        std::list<std::string> header;
        header.emplace_back("Content-Type: application/x-www-form-urlencoded");
        request.setOpt(new cURLpp::Options::HttpHeader(header));

        request.setOpt(new cURLpp::Options::PostFieldSize(static_cast<long>(postData.length())));
        request.setOpt(new cURLpp::Options::PostFields(postData));
        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200)
            throw HtmlError(fmt::format("HTML error code: {}", code));

        accessToken = nlohmann::json::parse(response);
        std::ofstream ofs("/home/richard/.config/ecoBeeApi/response.json");
        ofs << accessToken.dump(4) << '\n';
        ofs.close();
        return ApiStatus::OK;
    }

    ApiStatus statusPoll(nlohmann::json &poll, const std::string &token) {
        std::stringstream response{};
        cURLpp::Cleanup cleaner;
        cURLpp::Easy request;
        request.setOpt(new cURLpp::Options::Url(R"(https://api.ecobee.com/1/thermostatSummary?json=)"
            R"({"selection":{"selectionType":"registered","selectionMatch":"","includeEquipmentStatus":true}})"));
        request.setOpt(new cURLpp::Options::Verbose(false));

        std::list<std::string> header;
        header.emplace_back("Content-Type: text/json");
        header.emplace_back(fmt::format("Authorization: Bearer {}", token));
        request.setOpt(new cURLpp::Options::HttpHeader(header));
        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200 && code != 500)
            throw HtmlError(fmt::format("HTML error code: {}", code));

        poll = nlohmann::json::parse(response);

        if (auto status = apiStatus(poll["status"]["code"], poll["status"]["message"]); status == ApiStatus::OK) {
            return status;
        } else {
            return status;
        }
    }

    std::tuple<std::string, std::string, std::string, std::string, std::string> runtimeIntervals(const std::string &lastTime) {
        std::stringstream ss{lastTime};
        std::string format{DateTimeFormat};
        std::tm dtLast{};
        ss >> std::get_time(&dtLast, format.c_str());
        char buf[32];
        strftime(buf, 15, "%Y-%m-%d", &dtLast);
        std::string startTime{buf};
        auto startInt = fmt::format("{:d}",(dtLast.tm_hour*60 + dtLast.tm_min)/5);

        time_t now;
        time(&now);
        std::tm *dtNow;
        dtNow = gmtime(&now);
        strftime(buf, 15, "%Y-%m-%d", dtNow);
        std::string endTime{buf};
        auto endInt = fmt::format("{:d}", (dtNow->tm_hour*60 + dtNow->tm_min)/5);
        strftime(buf, 21, format.c_str(), dtNow);

        return {startTime,startInt,endTime,endInt,std::string{buf}};
    }

    template<class Delim> concept TokenDelimiter = requires {
        std::is_same_v<Delim,std::string> || std::is_same_v<Delim,char>;
    };

    template<class Delim>
    requires TokenDelimiter<Delim>
    std::tuple<std::string,std::string> tokenizeString(const std::string& source, Delim delim) {
        if (source.empty())
            return {std::string{},std::string{}};
        auto pos = source.find(delim);
        if (pos == std::string::npos)
            return {source,std::string{}};
        if constexpr (std::is_same_v<Delim,char>) {
            return {source.substr(0,pos),source.substr(pos+1)};
        } else {
            return {source.substr(0,pos),source.substr(pos+delim.length())};
        }
    }

    template<class Delim>
    requires TokenDelimiter<Delim>
    std::vector<std::string> tokenVector(const std::string& source, Delim delim) {
        std::vector<std::string> result{};
        for (auto token = tokenizeString(source,delim); !get<0>(token).empty();){
            result.emplace_back(get<0>(token));
            token = tokenizeString(get<1>(token),delim);
        }
        return result;
    }

    void processRuntimeData(const nlohmann::json &data) {
        [[maybe_unused]] size_t reportRowCount = data["reportList"][0]["rowCount"];
        auto columnList = tokenVector(data["columns"], ',');
        std::vector<std::string> sensorList{};
        std::map<std::string,Sensor> sensors{};
        for (auto &column : data["sensorList"][0]["columns"]) {
            sensorList.emplace_back(column);
        }
        for (auto &sensor : data["sensorList"][0]["sensors"]) {
            sensors.emplace(sensor["sensorId"],Sensor{sensor["sensorId"],sensor["sensorName"],sensor["sensorType"],sensor["sensorUsage"]});
            std::cout << sensor.dump() << '\n';
        }
        for (size_t idx = 0; idx < reportRowCount; ++idx) {
            nlohmann::json reportJson{};
            auto reportVector = tokenVector(data["reportList"][0]["rowList"][idx],',');
            auto sensorVector = tokenVector(data["sensorList"][0]["data"][idx],',');
            if (columnList.size() + 2 == reportVector.size()) {
                for(size_t colIdx = 0; colIdx < columnList.size(); ++colIdx) {
                    if (OperationTimeParam.find(columnList[colIdx]) != std::string_view::npos)
                        reportJson["operations"]["time"][columnList[colIdx]] = reportVector[colIdx + 2];
                    else if (OperationStateParam.find(columnList[colIdx]) != std::string_view::npos)
                        reportJson["operations"]["state"][columnList[colIdx]] = reportVector[colIdx + 2];
                    else if (columnList[colIdx].find("zoneHeatTemp") != std::string::npos ||
                            columnList[colIdx].find("zoneCoolTemp") != std::string::npos)
                        reportJson["operations"][columnList[colIdx]] = reportVector[colIdx + 2];
                    else if (columnList[colIdx].find("Humidity") != std::string::npos)
                        reportJson["humidity"][columnList[colIdx]] = reportVector[colIdx + 2];
                    else if (columnList[colIdx].find("Temp") != std::string::npos)
                        reportJson["temperature"][columnList[colIdx]] = reportVector[colIdx + 2];
                    else
                        reportJson["data"][columnList[colIdx]] = reportVector[colIdx + 2];
                }
            }
            if (sensorList.size() == sensorVector.size()) {
                for(size_t colIdx = 2; colIdx < sensorList.size(); ++colIdx) {
                    auto sensor = sensors.find(sensorList[colIdx]);
                    if (sensor != sensors.end()) {
                        switch (sensor->second.type) {
                            case Sensor::temperature:
                                reportJson["temperature"][sensor->second.name] = sensorVector[colIdx];
                                break;
                            case Sensor::humidity:
                                reportJson["humidity"][sensor->second.name] = sensorVector[colIdx];
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            std::cout << reportJson.dump(4) << '\n';
        }
    }
} // ecoBee