//
// Created by Pavel Akhtyamov on 02.05.2020.
//

#pragma once

#include <gmock/gmock.h>
#include <Weather.h>

using ::testing::_;

class MockWeather : public Weather {
public:
    MOCK_METHOD(cpr::Response, Get, (const std::string & city, const cpr::Url & url), (override));

    MOCK_METHOD(float, GetTemperature, (const std::string & city), (override));
    MOCK_METHOD(float, GetTomorrowTemperature, (const std::string & city), (override));
    MOCK_METHOD(std::string, GetLocationKey, (const std::string & city), (override));

    MockWeather() {
        ON_CALL(*this, GetTemperature(_))
            .WillByDefault([this](const std::string & city) {
                return Weather::GetTemperature(city);
        });
        ON_CALL(*this, GetLocationKey(_))
            .WillByDefault([this](const std::string & city) {
                return Weather::GetLocationKey(city);
        });
        ON_CALL(*this, GetTomorrowTemperature(_))
            .WillByDefault([this](const std::string & city) {
                return Weather::GetTomorrowTemperature(city);
        });
    }
};
