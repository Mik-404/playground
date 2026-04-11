//
// Created by akhtyamovpavel on 5/1/20.
//


#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

class TreeTestCase : public ::testing::Test {
};

class FileFixture : public testing::Test {
protected:
    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / "test_fixture_dir";
        std::filesystem::create_directory(temp_dir);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(temp_dir);
    }
    
    std::filesystem::path temp_dir;
};

class FilterEmptyNodesTest : public testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories("test_root/a/b/c");
        std::filesystem::create_directories("test_root/empty_dir");
        std::ofstream("test_root/file1.txt") << "content";
        std::ofstream("test_root/a/file2.txt") << "content";
    }

    void TearDown() override {
        std::filesystem::remove_all("test_root");
    }
};
