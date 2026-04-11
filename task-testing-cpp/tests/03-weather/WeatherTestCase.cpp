//
// Created by Pavel Akhtyamov on 02.05.2020.
//

#include "WeatherTestCase.h"
#include "WeatherMock.h"
#include <iostream>


using ::testing::Return;
using ::testing::_;


TEST_F(WeatherTestCase, GetTemperatureParsesCorrectly) {
    MockWeather mock;
    std::string city = "London";
    std::string locationKey = "328328";

    EXPECT_CALL(mock, Get(city, kLocationUrl))
        .WillOnce(Return(key_parse_london));

    EXPECT_CALL(mock, Get(city, kBaseUrl + locationKey))
        .WillOnce(Return(temp_data_london));
    
    EXPECT_CALL(mock, GetTemperature(city));
    EXPECT_CALL(mock, GetLocationKey(city));

    mock.SetApiKey("test_key");
    EXPECT_FLOAT_EQ(mock.GetTemperature(city), 12.2f);
}

TEST_F(WeatherTestCase, GetLocationKeyCachesResults) {
    MockWeather mock;
    std::string city = "London";
    std::string key = "328328";

    EXPECT_CALL(mock, Get(city, kLocationUrl))
        .Times(1)
        .WillOnce(Return(key_parse_london));
    
    EXPECT_CALL(mock, GetLocationKey(city))
        .Times(2);

    EXPECT_EQ(mock.GetLocationKey(city), key);
    EXPECT_EQ(mock.GetLocationKey(city), key);
}

TEST_F(WeatherTestCase, GetTomorrowDiffMuchWarmer) {
    MockWeather mock;
    std::string city1 = "London";
    std::string key1 = "328328";

    std::string city2 = "Paris";
    std::string key2 = "623";

    EXPECT_CALL(mock, Get(city1, kLocationUrl))
        .Times(1)
        .WillOnce(Return(key_parse_london));

    EXPECT_CALL(mock, Get(city2, kLocationUrl))
        .Times(1)
        .WillOnce(Return(key_parse_paris));

    EXPECT_CALL(mock, Get(city1, kBaseUrl + key1))
        .Times(1)
        .WillOnce(Return(temp_data_london));

    EXPECT_CALL(mock, Get(city2, kBaseUrl + key2))
        .Times(1)
        .WillOnce(Return(temp_data_paris));

    EXPECT_CALL(mock, GetTemperature(city1));
    EXPECT_CALL(mock, GetTemperature(city2));

    EXPECT_CALL(mock, GetLocationKey(city1));
    EXPECT_CALL(mock, GetLocationKey(city2));

    EXPECT_FLOAT_EQ(mock.FindDiffBetweenTwoCities(city1, city2), 12.2f - 14.8f);
}

TEST_F(WeatherTestCase, GetResponseThrowsOnError) {
    MockWeather mock;
    std::string city = "imjustahog";
    EXPECT_CALL(mock, Get(city, _))
        .WillOnce(Return(empty_data));
    EXPECT_CALL(mock, GetLocationKey(city));
    EXPECT_CALL(mock, GetTemperature(city));
    
    EXPECT_THROW(mock.GetTemperature(city), std::runtime_error);
}

TEST_F(WeatherTestCase, GetResponseThrowsBadResponceCode) {
    MockWeather mock;
    std::string city = "imjustahog";
    empty_data.status_code = 404;
    EXPECT_CALL(mock, Get(city, _))
        .WillOnce(Return(empty_data));
    EXPECT_CALL(mock, GetLocationKey(city));
    EXPECT_CALL(mock, GetTemperature(city));
    
    EXPECT_THROW(mock.GetTemperature(city), std::invalid_argument);
}

TEST_F(WeatherTestCase, GetResponseForecast) {
    MockWeather mock;
    std::string city = "Paris";
    std::string locationKey = "623";

    EXPECT_CALL(mock, GetTomorrowTemperature(city));
    EXPECT_CALL(mock, GetLocationKey(city));

    EXPECT_CALL(mock, Get(city, kLocationUrl))
        .WillOnce(Return(key_parse_paris));

    EXPECT_CALL(mock, Get(city, kForecastUrl + locationKey))
        .WillOnce(Return(forec_data_paris));

    EXPECT_EQ(mock.GetTomorrowTemperature(city), 17.9f);
}

TEST_F(WeatherTestCase, GetResponseForecastDiff) {
    MockWeather mock;
    std::string city = "Paris";
    std::string locationKey = "623";

    EXPECT_CALL(mock, Get(city, kLocationUrl))
        .WillOnce(Return(key_parse_paris));

    EXPECT_CALL(mock, Get(city, kForecastUrl + locationKey))
        .WillOnce(Return(forec_data_paris));
    
    EXPECT_CALL(mock, Get(city, kBaseUrl + locationKey))
        .WillOnce(Return(temp_data_paris));

    EXPECT_CALL(mock, GetTomorrowTemperature(city));
    EXPECT_CALL(mock, GetTemperature(city));
    EXPECT_CALL(mock, GetLocationKey(city))
        .Times(2);
    std::string res = "The weather in " + city + " tomorrow will be much warmer than today.";
    EXPECT_EQ(mock.GetTomorrowDiff(city), res);
}

TEST_F(WeatherTestCase, GetResponseAdditionalStupidTest) {
    MockWeather mock;
    std::string city = "Paris";
    std::string locationKey = "623";

    EXPECT_CALL(mock, GetTomorrowTemperature(city))
        .WillOnce(Return(17.4f))
        .WillOnce(Return(17.0f))
        .WillOnce(Return(16.2f))
        .WillOnce(Return(13.0f));
    EXPECT_CALL(mock, GetTemperature(city))
        .WillRepeatedly(Return(16.8f));

    std::string res = "The weather in " + city + " tomorrow will be warmer than today.";
    EXPECT_EQ(mock.GetTomorrowDiff(city), res);

    res = "The weather in " + city + " tomorrow will be the same than today.";
    EXPECT_EQ(mock.GetTomorrowDiff(city), res);
    
    res = "The weather in " + city + " tomorrow will be colder than today.";
    EXPECT_EQ(mock.GetTomorrowDiff(city), res);
    
    res = "The weather in " + city + " tomorrow will be much colder than today.";
    EXPECT_EQ(mock.GetTomorrowDiff(city), res);
}

TEST_F(WeatherTestCase, GetResponseDiffString) {
    MockWeather mock;
    std::string city1 = "London";

    std::string city2 = "Paris";

    EXPECT_CALL(mock, GetTemperature(city1))
        .WillOnce(Return(17.4f))
        .WillOnce(Return(13.0f));

    EXPECT_CALL(mock, GetTemperature(city2))
        .WillRepeatedly(Return(16.8f));

    std::string res = "Weather in " + city1 + " is warmer than in " + city2 + " by 0 degrees";
    EXPECT_EQ(mock.GetDifferenceString(city1, city2), res);
    
    res = "Weather in " + city1 + " is colder than in " + city2 + " by 3 degrees";
    EXPECT_EQ(mock.GetDifferenceString(city1, city2), res);
}