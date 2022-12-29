//
// Created by richard on 29/12/22.
//

/*
 * InfluxPush.h Created by Richard Buckley (C) 29/12/22
 */

#ifndef ECOBEEDATA_INFLUXPUSH_H
#define ECOBEEDATA_INFLUXPUSH_H


#include <string>
#include <utility>
#include <cmath>
#include <sstream>
#include <optional>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>

/**
 * @class InfluxPush
 *
 * https://archive.docs.influxdata.com/influxdb/v1.2/guides/writing_data/
 */
class InfluxPush {
private:
    const std::string TimeFmt{"%Y-%m-%dT%k:%M:%S%z"};
    unsigned long long timeStamp{};
    std::string influxHost{};
    bool connectTls{false};
    long influxPort{0};
    std::string influxDataBase{};

    std::stringstream measurements{};

public:
    InfluxPush() = delete;

    InfluxPush(std::string host, bool tls, long port, std::string dataBase)
        : influxHost(std::move(host)), connectTls(tls), influxPort(port), influxDataBase(std::move(dataBase)) {}

    void setMeasurementEpoch(const std::string& date, const std::string& time, const std::string& zone);

    void newMeasurements() {
        measurements.str("");
    }

    void addMeasurement(const std::string& prefix, const std::optional<std::string>& name, const std::optional<std::string>& value) {
        if (name.has_value() && !name.value().empty() && value.has_value() && !value.value().empty())
            measurements << prefix << name.value() << '=' << value.value() << ' ' << timeStamp << '\n';
    }

    bool pushData();

    [[maybe_unused]] auto getMeasurements() const {
        return measurements.str();
    }
};


#endif //ECOBEEDATA_INFLUXPUSH_H

