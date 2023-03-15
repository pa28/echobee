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
#include <chrono>
#include <utility>
#include <date/tz.h>
#include "Api.h"
#include "nlohmann/json.hpp"
#include "InfluxPush.h"

namespace ecoBee {
    ApiStatus runtimeReport(nlohmann::json &data, const std::string &token, const std::string &url) {
        std::stringstream response{};
        cURLpp::Cleanup cleaner;
        cURLpp::Easy request;

        request.setOpt(new cURLpp::Options::Url(url));
        request.setOpt(new cURLpp::Options::Verbose(false));

        std::list<std::string> header;
        header.emplace_back("content-type: text/json;charset=UTF-8");
        header.emplace_back(ysh::StringComposite("Authorization: Bearer ", token));
        request.setOpt(new cURLpp::Options::HttpHeader(header));

        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200 && code != 500) {
            throw HtmlError(ysh::StringComposite("HTML error code: ", code));
        }

        data = nlohmann::json::parse(response);
        return apiStatus(data["status"]["code"], data["status"]["message"]);
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
        header.emplace_back(ysh::StringComposite("Authorization: Bearer ", token));
        request.setOpt(new cURLpp::Options::HttpHeader(header));

        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200) {
            throw HtmlError(ysh::StringComposite("HTML error code: ", code));
        }

        auto thermostat = nlohmann::json::parse(response);
        return apiStatus(thermostat["status"]["code"], thermostat["status"]["code"]);
    }

    /**
     * @brief Refresh the access token.
     * @param accessToken A RETURNED Json structure with an (possibly expired) access token and a refresh token.
     * @param url The access token refresh URL from echoBee API documentation.
     * @param apiKey The API key assigned at application registration by the developer.
     * @param token The refresh token.
     * @throws HtmlError if the HTML response code != 200.
     * @return ApiStatus::OK
     */
    ApiStatus refreshAccessToken(nlohmann::json &accessToken, const std::string &url, const std::string &apiKey,
                                 const std::string &token) {
        auto postData = ysh::StringComposite("grant_type=refresh_token&&code=", token, "&client_id=", apiKey);
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
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200) {
            throw HtmlError(ysh::StringComposite("HTML error code: ", code));
        }

        accessToken = nlohmann::json::parse(response);
        std::ofstream ofs("/home/richard/.config/ecoBeeApi/response.json");
        ofs << accessToken.dump(4) << '\n';
        ofs.close();
        return ApiStatus::OK;
    }

    /**
     * @brief Poll the thermostat data for updates.
     * @details This poll should be executed before getting a runtime report to determine if it is worth requesting
     * a report. This will also determine if the access token the application currently holds is valid or should
     * be replaced. See refreshAccessToken.
     * @param poll The RETURNED poll data.
     * @param token An access token.
     * @throws HtmlError if HTML response code is not 200 and not 500 which is returned if the requests is not
     * authorized, probably due to an expired authentication token.
     * @return ApiStatus::OK if the poll succeeds, ApiStatus::Expired if the access token is expired.
     */
    ApiStatus statusPoll(nlohmann::json &poll, const std::string &token) {
        std::stringstream response{};
        cURLpp::Cleanup cleaner;
        cURLpp::Easy request;
        request.setOpt(new cURLpp::Options::Url(R"(https://api.ecobee.com/1/thermostatSummary?json=)"
            R"({"selection":{"selectionType":"registered","selectionMatch":"","includeEquipmentStatus":true}})"));
        request.setOpt(new cURLpp::Options::Verbose(false));

        std::list<std::string> header;
        header.emplace_back("Content-Type: text/json");

        header.emplace_back(ysh::StringComposite("Authorization: Bearer ", token));
        request.setOpt(new cURLpp::Options::HttpHeader(header));
        request.setOpt(new curlpp::options::WriteStream(&response));

        request.perform();
        if (auto code = curlpp::infos::ResponseCode::get(request); code != 200 && code != 500) {
            throw HtmlError(ysh::StringComposite("HTML error code: ", code));
        }

        poll = nlohmann::json::parse(response);

        return apiStatus(poll["status"]["code"], poll["status"]["message"]);
    }

    std::string localToGMT(const std::string &date, const std::string &time) {
        // Gather the local time zone and DST information.
        std::tm localDateTime{};
        time_t epoch;
        ::time(&epoch);

        // Convert the provided local time to GMT.
        localDateTime = *(localtime(&epoch));
        std::string dateTimeString = date;
        dateTimeString.append("T").append(time);

        std::string format{DateTimeFormat};
        std::tm dateTime{};
        strptime(dateTimeString.c_str(), format.c_str(), &dateTime);

        // Bring in the time zone and GMT offset information gathered earlier.
        dateTime.tm_zone = localDateTime.tm_zone;
        dateTime.tm_gmtoff = localDateTime.tm_gmtoff;
        dateTime.tm_isdst = localDateTime.tm_isdst;

        // Complete conversion to GMT.
        epoch = ::mktime(&dateTime);
        char buf[32];
        strftime(buf, 31, format.c_str(), gmtime(&epoch));
        return std::string{buf};
    }

    /**
     * @brief Derive runtime report time interval data from the last runtime report and the current time.
     * @details Data is stored in rows representing a 5 minute period called an "interval". These times must be
     * specified in GMT. Times returned are in the thermostat registered timezone including DST.
     * @param lastTime The time GMT of the last runtime report.
     * @return A tuple with the start date, interval and end data, interval.
     */
    std::tuple<std::string, std::string, std::string, std::string, std::string> runtimeIntervals(const std::string &lastTime) {
        std::stringstream ss{lastTime};
        std::string format{DateTimeFormat};
        std::tm dtLast{};
        ss >> std::get_time(&dtLast, format.c_str());
        char buf[32];
        strftime(buf, 15, "%Y-%m-%d", &dtLast);
        std::string startTime{buf};
        auto startInt = std::to_string((dtLast.tm_hour*60 + dtLast.tm_min)/5);

        time_t now;
        time(&now);
        std::tm *dtNow;
        dtNow = gmtime(&now);
        strftime(buf, 15, "%Y-%m-%d", dtNow);
        std::string endTime{buf};
        auto endInt = std::to_string((dtNow->tm_hour*60 + dtNow->tm_min)/5);
        strftime(buf, 21, format.c_str(), dtNow);

        return {startTime,startInt,endTime,endInt,std::string{buf}};
    }

    /**
     * @brief A concept for a delimiter which can be a string or a character.
     * @tparam Delim The type of delimiter
     */
    template<class Delim> concept TokenDelimiter = requires {
        std::is_same_v<Delim,std::string> || std::is_same_v<Delim,char>;
    };

    /**
     * @brief Iteratively tokenize a string by a delimiter.
     * @details See tokenVector() for an example usage.
     * @tparam Delim The type of delimiter.
     * @param source The source string.
     * @param delim The delimiter value.
     * @return A tuple with the found token (or empty) and the remainder of the source (or empty).
     */
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

    /**
     * @brief Decompose a delimited string into tokens in a std::vector<std::string>.
     * @tparam Delim The delimiter type.
     * @param source The source std::string.
     * @param delim The delimiter value.
     * @return A possibly empty std::vector<std::string>
     */
    template<class Delim>
    requires TokenDelimiter<Delim>
    std::vector<std::string> tokenVector(const std::string& source, Delim delim) {
        std::vector<std::string> result{};
        for (auto token = tokenizeString(source,delim); !get<0>(token).empty(); token = tokenizeString(get<1>(token),delim)){
            result.emplace_back(get<0>(token));
        }
        return result;
    }

    /**
     * @brief Process the results of a Runtime Report.
     * @details The runtime report is digested to produce a structure more representative of the operation of the HVAC
     * system and therefor easier to display for a naive user. Data is returned as CSV rows for the system overall and
     * for the fleet of sensors if present and requested in the runtime report.
     * @param data The Json structure returned by the runtime report.
     * @return A std::string with the GMT time string of last data row processed. Empty if no data processed.
     */
    std::string processRuntimeData(const nlohmann::json &data, const InfluxConfig &config) {
        size_t reportRowCount = data["reportList"][0]["rowCount"];
        std::string newLastTime{};
        InfluxPush influx(config.influxHost.value(), config.influxTLS.value(), config.influxPort.value(), config.influxDb.value());

        // Tokenize report column titles.
        auto columnList = tokenVector(data["columns"], ',');
        std::vector<std::string> sensorList{};
        std::map<std::string,Sensor> sensors{};

        // Tokenize sensor column titles which are sensor ID strings.
        for (auto &column : data["sensorList"][0]["columns"]) {
            sensorList.emplace_back(column);
        }

        // Gather the sensor structures and map them to their IDs
        for (auto &sensor : data["sensorList"][0]["sensors"]) {
            sensors.emplace(sensor["sensorId"],Sensor{sensor["sensorId"],sensor["sensorName"],sensor["sensorType"],sensor["sensorUsage"]});
        }

        // Process each row of returned data.
        for (size_t idx = 0; idx < reportRowCount; ++idx) {
            nlohmann::json reportJson{};
            auto reportVector = tokenVector(data["reportList"][0]["rowList"][idx],',');
            auto sensorVector = tokenVector(data["sensorList"][0]["data"][idx],',');

            // Process thermostat/system data
            if (columnList.size() + 2 == reportVector.size()) {
                /**
                 * Categorize data into:
                 *  - Operations time data. These are data that indicate how long during a 5 minute interval
                 *  equipment was operating or operating in a specific mode.
                 *  - Operations state data. The operating mode or state at the beginning of the interval.
                 *  - Temperatures.
                 *  - Humidity.
                 */
                for(size_t colIdx = 0; colIdx < columnList.size(); ++colIdx) {
                    if (!reportVector[colIdx + 2].empty()) {
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
            }

            /**
             * Process sensor data, only temperatures, humanities and pressures are handled in this version.
             * Occupancy is ignored.
             */
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
                            case Sensor::airPressure:
                                reportJson["airPressure"][sensor->second.name] = sensorVector[colIdx];
                                break;
                            default:
                                break;
                        }
                    }
                }
            }

            /**
             * The data row is ready to use, sent to an InfluxDB for example. If the reportJson structure is
             * empty, the CSV row had no data.
             */
            if (!reportJson.empty()) {
                influxPush(reportJson, influx, reportVector[0], reportVector[1]);
                newLastTime = localToGMT(reportVector[0], reportVector[1]);
            }
        }

        return newLastTime;
    }

    std::string escapeHeader(const std::string& hdr) {
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

    std::string FtoC(const std::string& f) { return std::to_string((atof(f.c_str()) - 32.f)  * 5.f/9.f); }

    std::string hectoPascals(const std::string& s) { return std::to_string(atof(s.c_str()) / 100.f); }

    void timeBasedOperation(InfluxPush &influx, const std::string& name, const std::string& data, const std::string& time,
                            int seconds, bool state);

    void influxPush(nlohmann::json &row, InfluxPush &influx, const std::string &date, const std::string &time) {
        const static std::string prefix{"Home "};
        const static std::optional<std::string>True{"true"};
        const static std::optional<std::string>False{"false"};

        influx.newMeasurements();
        influx.setMeasurementEpoch(date, time);
        bool dataWritten = false;

        for (const auto& item : row["humidity"].items()) {
            dataWritten |= influx.addMeasurement(prefix, escapeHeader(item.key()), item.value());
        }

        for (const auto& item : row["temperature"].items()) {
            dataWritten |= influx.addMeasurement(prefix, escapeHeader(item.key()), FtoC(item.value()));
        }

        for (const auto& item : row["airPressure"].items()) {
            [[maybe_unused]] auto v = item.value();
            [[maybe_unused]] auto p = hectoPascals(v);
            dataWritten |= influx.addMeasurement(prefix, escapeHeader(item.key()), hectoPascals(item.value()));
        }

        if (row["operations"]["state"]["HVACmode"] == "heat") {
            dataWritten += influx.addMeasurement(prefix, "SetPoint", FtoC(row["operations"]["zoneHeatTemp"]));
        } else if (row["operations"]["state"]["zoneHVACmode"] == "cool") {
            dataWritten += influx.addMeasurement(prefix, "SetPoint", FtoC(row["operations"]["zoneCoolTemp"]));
        }

        /*
         * Time specified operations
         *
         */

        struct TimeOp {
            std::string name{};
            bool state{};
            unsigned long seconds{};
            unsigned long long timestamp{};
            explicit TimeOp(std::string  name) :name(std::move(name)) {}
        };

        std::vector<TimeOp> timeOpList{};
        timeOpList.emplace_back("Fan");
        timeOpList.emplace_back("Heat");
        timeOpList.emplace_back("Cool");

        std::string HVACmode = row["operations"]["state"]["zoneHVACmode"];
        if (HVACmode == "heatOff") {
            for (auto& op : timeOpList)
                op.state = false;
        } else if (HVACmode == "heatStage1On") {
            timeOpList[0].state = timeOpList[1].state = true;
            timeOpList[2].state = false;
        } else if (HVACmode == "compressorCoolStage10n") {
            timeOpList[0].state = timeOpList[2].state = true;
            timeOpList[1].state = false;
        }

        timeOpList[0].seconds = strtoul(std::string{row["operations"]["time"]["fan"]}.c_str(), nullptr, 10);
        timeOpList[1].seconds = strtoul(std::string{row["operations"]["time"]["auxHeat1"]}.c_str(), nullptr, 10);
        timeOpList[2].seconds = strtoul(std::string{row["operations"]["time"]["compCool1"]}.c_str(), nullptr, 10);

        for (auto& op : timeOpList) {
            op.timestamp = influx.getMeasurementEpoch();
            op.state &= op.seconds != 0;
            op.state |= op.seconds == 300;
            if (op.seconds != 0 && op.seconds != 300) {
                if (op.state)
                    op.timestamp += op.seconds * 1000000000;
                else
                    op.timestamp -= (300 - op.seconds) * 1000000000;
            }
            dataWritten |= influx.addMeasurement(prefix, op.name, (op.state ? True : False), op.timestamp);
        }

        if (dataWritten)
            influx.pushData();
        dataWritten = false;
    }
} // ecoBee