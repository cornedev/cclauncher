// MIT License
// Copyright (c) 2025 cornedev

#pragma once

// Dependency headers.
//
#include <string>
#include <functional>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <windows.h>
#include <atomic>
#include <curl/curl.h>
#include <zip.h>
#include <nlohmann/json.hpp>
extern std::atomic<bool> minecraftrunning;

class launcher
{
public:
    launcher
    (
        const std::string& versionid = "1.21",
        std::function<void(const std::string&)> logger = nullptr
    );
public:
    void launchprocess(const std::string& username);

private:
    static void logger(const std::string& msg);
    void downloadfiles(const std::string& url, const std::string& outputpath);
    void extractnatives(const std::string& jarpath);
    void setuplauncher();
    std::string getclasspath();
    std::string buildlaunchcommand(const std::string& username);
    // Version information.
    //
    std::string versionid;
    std::string jsonpath;
    std::string nativespath;
    std::string libspath;
    std::function<void(const std::string&)> logconsole;
};
