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
} // ecoBee