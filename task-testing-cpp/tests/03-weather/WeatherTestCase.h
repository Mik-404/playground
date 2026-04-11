
#pragma once

#include <gtest/gtest.h>
#include <fstream>
#include <cpr/cpr.h>

class WeatherTestCase : public ::testing::Test {
    std::string path_to_london_code = "data/resp1.txt";
    std::string path_to_empty_response = "data/resp2.txt";
    std::string path_to_temp_data_london = "data/resp3.txt";
    std::string path_to_paris_code = "data/resp4.txt";
    std::string path_to_temp_data_paris = "data/resp5.txt";
    std::string path_to_forecast_data_paris = "data/resp6.txt";

protected:
    std::string cur_resp;
    std::ifstream in;
    cpr::Response key_parse_london;
    cpr::Response temp_data_london;
    cpr::Response key_parse_paris;
    cpr::Response forec_data_paris;
    cpr::Response temp_data_paris;
    cpr::Response empty_data;

    const cpr::Url kBaseUrl = cpr::Url{"http://dataservice.accuweather.com/currentconditions/v1/"};
    const cpr::Url kForecastUrl = cpr::Url{"http://dataservice.accuweather.com/forecasts/v1/daily/5day/"};
    const cpr::Url kLocationUrl = cpr::Url{"http://dataservice.accuweather.com/locations/v1/cities/search"};

    void SetUp() override {
        in.open(path_to_london_code);

        cur_resp = std::string (std::istreambuf_iterator<char>(in), 
                        std::istreambuf_iterator<char>());
        key_parse_london.text = cur_resp;
        key_parse_london.status_code = 200;
        in.close();

        in.open(path_to_empty_response);
        cur_resp = std::string (std::istreambuf_iterator<char>(in), 
                        std::istreambuf_iterator<char>());
        empty_data.text = cur_resp;
        empty_data.status_code = 200;
        in.close();

        in.open(path_to_temp_data_london);
        cur_resp = std::string (std::istreambuf_iterator<char>(in), 
                        std::istreambuf_iterator<char>());
        temp_data_london.text = cur_resp;
        temp_data_london.status_code = 200;
        in.close();

        in.open(path_to_paris_code);

        cur_resp = std::string (std::istreambuf_iterator<char>(in), 
                        std::istreambuf_iterator<char>());
        key_parse_paris.text = cur_resp;
        key_parse_paris.status_code = 200;
        in.close();

        in.open(path_to_temp_data_paris);
        cur_resp = std::string (std::istreambuf_iterator<char>(in), 
                        std::istreambuf_iterator<char>());
        temp_data_paris.text = cur_resp;
        temp_data_paris.status_code = 200;
        in.close();

        in.open(path_to_forecast_data_paris);
        cur_resp = std::string (std::istreambuf_iterator<char>(in), 
                        std::istreambuf_iterator<char>());
        forec_data_paris.text = cur_resp;
        forec_data_paris.status_code = 200;
        in.close();
    }

    void TearDown() override {
        std::filesystem::remove_all("test_root");
    }
};
