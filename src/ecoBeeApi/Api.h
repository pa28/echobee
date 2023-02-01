//
// Created by richard on 30/01/23.
//

/*
 * Api.h Created by Richard Buckley (C) 30/01/23
 */

/**
 * @file Api.h
 * @author Richard Buckley <richard.buckley@ieee.org>
 * @version 1.0
 * @date 30/01/23
 * @brief 
 * @details
 */

#ifndef ECOBEEDATA_API_H
#define ECOBEEDATA_API_H

#include <string>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <nlohmann/json.hpp>
#include <exception>
#include <utility>
#include <fmt/format.h>

namespace ecoBee {

    static constexpr std::string_view DateTimeFormat = "%Y-%m-%dT%H:%M:%SZ";
    class HtmlError : public std::runtime_error {
    public:
        explicit HtmlError(const std::string& what_arg) : std::runtime_error(what_arg) {}
    };

    class ApiError : public std::runtime_error {
    public:
        explicit ApiError (const std::string& what_arg) : std::runtime_error(what_arg) {}
    };

    enum class ApiStatus {
        OK, TokenExpired
    };

    [[nodiscard]] inline ApiStatus apiStatus(int code, const std::string &message) {
        if (code == 0)
            return ApiStatus::OK;
        else if (code == 14)
            return ApiStatus::TokenExpired;
        else {
            throw ApiError(fmt::format("Code: {}, Message: {}", code, message));
        }
    }

    template<class Range>
    concept StringRange = requires(Range &range) {
        std::ranges::begin(range);
        std::ranges::end(range);
        { std::ranges::begin(range) } -> std::convertible_to<std::string_view*>;
    };

    static constexpr std::string_view Thermostat = "421866388280";
    static constexpr std::string_view ApiKey = "fpsld2HqnigU3P4vnsuP5zZ8pLlpsfup";
    static constexpr std::string_view AppPin = "LVMP-KJRP";

    struct AccessToken {
        std::string mAccessToken{}, mTokenType{}, mScope{}, mRefreshToke{};
        uint32_t mExpiresIn{};
    };

    struct App {
        std::string mApiKey, mApiPin, mAuthCode;
        AccessToken mAccessToken{};
    };

    static constexpr std::string_view OperationTimeParam = "auxHeat1,compCool1,fan";
    static constexpr std::string_view OperationStateParam = "HVACmode,zoneHVACmode,zoneClimate";
    struct Sensor {
        enum Type { unknown, airPressure, temperature, occupancy, humidity };
        std::string id{}, name{}, usage{};
        Type type{unknown};

        Sensor(std::string id, std::string name, const std::string& sType, std::string usage)
            : id(std::move(id)), name(std::move(name)), usage(std::move(usage)) {
            if (sType == "airPressure") type = airPressure;
            else if (sType == "temperature") type = temperature;
            else if (sType == "occupancy") type = occupancy;
            else if (sType == "humidity") type = humidity;
        }
    };

    /**
     * @class Api
     */
    struct Api {
        std::string mApiKey, mApiPin, mAuthCode;
    };

    template<class Columns>
    requires StringRange<Columns>

    std::string runtimeReportUrl(const Columns columns, bool includeSensors,
                 const std::string& startD, const std::string& startI, const std::string& endD, const std::string& endI) {
        std::stringstream url{};

        url << R"(https://api.ecobee.com/1/runtimeReport?format=json&body={"startDate":")" << startD
            << R"(","startInterval":")" << startI
            << R"(","endDate":")" << endD
            << R"(","endInterval":")" << endI
            << R"(","columns":")";

        for (bool first = true; const auto& column : columns) {
            if (!first) url << ',';
            url << column;
            first = false;
        }

        url << R"(","includeSensors":)" << (includeSensors ? "true" : "false") << ',';
        url << R"("selection":{"selectionType":"thermostats","selectionMatch":")" << Thermostat << R"("}})";
        std::cout << url.str() << '\n';
        return url.str();
    }

    [[nodiscard]] ApiStatus statusPoll(nlohmann::json &poll, const std::string &token);

    [[nodiscard]] ApiStatus
    runtimeReport(const std::string &token, const std::string &url);

    [[nodiscard]] ApiStatus refreshAccessToken(nlohmann::json& accessToken, const std::string& url, const std::string& apiKey,
                                 const std::string& token);

    [[nodiscard]] ApiStatus thermostat(const std::string &token);

    [[nodiscard]] std::tuple<std::string,std::string,std::string,std::string,std::string> runtimeIntervals(const std::string& lastTime);

    void processRuntimeData(const nlohmann::json& data);
} // ecoBee

#endif //ECOBEEDATA_API_H
