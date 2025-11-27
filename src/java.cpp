// MIT License
// Copyright (c) 2025 cornedev

// Include headers.
//
#include "../include/java.hpp"
// Boolean for only letting one minecraft instance run.
//
std::atomic<bool> minecraftrunning = false;

namespace fs = std::filesystem;
using json = nlohmann::json;

// Used when not passing a custom logger.
//
void launcher::logger(const std::string& msg)
{
    std::cout << msg << std::endl;
}

// This saves versionid into the class and builds all the directories.
//
launcher::launcher
(
    const std::string& versionid,
    std::function<void(const std::string&)> logger
)
    :versionid(versionid)
{
    jsonpath = (fs::path(".minecraft") / "versions" / versionid / (versionid + ".json")).make_preferred().string();
    libspath = (fs::path(".minecraft") / "versions" / versionid / "libraries").make_preferred().string();
    nativespath = (fs::path(".minecraft") / "natives" / versionid).make_preferred().string();

    if (logger)
        logconsole = std::move(logger);
    else
        logconsole = launcher::logger;
}

// Called by curl every time bytes are downloaded.
//
static size_t curlcallback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::ofstream* stream = reinterpret_cast<std::ofstream*>(userdata);
    size_t written = size * nmemb;
    stream->write(reinterpret_cast<char*>(ptr), written);
    return written;
}

// This function downloads missing libraries.
//
void launcher::downloadfiles(const std::string& url, const std::string& outputpath)
{
    // Ensure folder exists.
    //
    fs::create_directories(fs::path(outputpath).parent_path());
    if (fs::exists(outputpath))
    {
        if (logconsole)
            logconsole("[Skip] " + outputpath);
        return;
    }
    if (logconsole)
        logconsole("[Download] " + url);
    // Open output file.
    //
    std::ofstream file(outputpath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open output file: " + outputpath);
    // CURL
    //
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize curl.");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlcallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    file.close();
    if (result != CURLE_OK)
    {
        throw std::runtime_error(
            std::string("CURL download failed: ") + curl_easy_strerror(result)
        );
    }
}

// This function extracts natives (.dll) from the version jar.
//
void launcher::extractnatives(const std::string& jarpath)
{
    // Ensure folder exists.
    //
    fs::create_directories(nativespath);

    int err = 0;
    // Open jar.
    //
    zip_t* zip = zip_open(jarpath.c_str(), 0, &err);

    if (!zip)
        throw std::runtime_error("Failed to open jar: " + jarpath);
    zip_int64_t numentries = zip_get_num_entries(zip, 0);
    for (zip_int64_t i = 0; i < numentries; ++i)
    {
        const char* name = zip_get_name(zip, i, 0);
        if (!name)
        {
            if (logconsole)
                logconsole("[Warn] Invalid ZIP entry at index: " + std::to_string(i));
            continue;
        }
        std::string entryname = name;
        // Only extract DLL files.
        //
        if (entryname.size() > 4 &&
            entryname.substr(entryname.size() - 4) == ".dll")
        {
            std::string outpath = (fs::path(nativespath) / fs::path(entryname).filename()).string();
            zip_file_t* zf = zip_fopen_index(zip, i, 0);
            if (!zf) 
            {
                if (logconsole)
                    logconsole("[Error] Failed to open ZIP entry: " + entryname);
                continue;
            }
            std::ofstream out(outpath, std::ios::binary);
            if (!out)
            {
                if (logconsole)
                    logconsole("[Error] Failed to create output DLL: " + outpath);
                zip_fclose(zf);
                continue;
            }
            // Allocate a temporary buffer for reading.
            //
            char buffer[4096];
            zip_int64_t bytesread;
            while ((bytesread = zip_fread(zf, buffer, sizeof(buffer))) > 0)
            {
                out.write(buffer, bytesread);
            }
            zip_fclose(zf);
            out.close();
        }
    }
    // Close jar.
    //
    zip_close(zip);
    if (logconsole)
        logconsole("[Extract] " + fs::path(jarpath).filename().string());
}

// This function collects all jar files.
//
std::string launcher::getclasspath()
{
    std::vector<std::string> jars;
    // Collect all .jar files recursively in libspath.
    //
    for (auto& entry : fs::recursive_directory_iterator(libspath))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".jar")
        {
            jars.push_back(entry.path().string());
        }
    }
    // Main game JAR.
    //
    fs::path mainjar = fs::absolute(fs::path(".minecraft") / "versions" / versionid / (versionid + ".jar")).make_preferred();
    if (fs::exists(mainjar))
    {
        jars.push_back(mainjar.string());
    }
    else
    {
        if (logconsole)
            logconsole("[Error] Missing version JAR: " + mainjar.string());
    }
    // Build classpath string using ';' separator (Windows).
    //
    std::string classpath;
    for (size_t i = 0; i < jars.size(); i++)
    {
        classpath += jars[i];
        if (i + 1 < jars.size())
            classpath += ";";
    }
    if (jars.empty()) {
        if (logconsole)
            logconsole("[Error] No JARs found in libraries directory: " + libspath);
    }
    return classpath;
}

// This function reads the version json and builds the launch command used to launch minecraft.
//
std::string launcher::buildlaunchcommand(const std::string& username)
{
    // Read version JSON.
    //
    std::ifstream file(jsonpath);
    if (!file)
    {
        if (logconsole)
            logconsole("[Error] Failed to read version JSON: " + jsonpath);
        return "";
    }
    json versiondata;
    file >> versiondata;
    // Parse main class.
    //
    std::string mainclass = versiondata.contains("mainClass")
        ? versiondata["mainClass"].get<std::string>()
        : "";
    if (mainclass.empty())
    {
        if (logconsole)
            logconsole("[Error] mainClass missing in version JSON.");
        return "";
    }
    // Parse asset index.
    //
    std::string assetindex = versiondata.contains("assets")
        ? versiondata["assets"].get<std::string>()
        : versionid;
    // Build classpath.
    //
    std::string classpath = getclasspath();
    // Build paths.
    //
    fs::path nativesdir = fs::absolute(nativespath);
    fs::path versiongamedir = fs::absolute(fs::path(".minecraft/versions") / versionid);
    fs::path versionassetsdir = versiongamedir / "assets";
    // Ensure directories exist.
    //
    fs::create_directories(versiongamedir);
    fs::create_directories(versionassetsdir);
    // Build Java command.
    //
    std::string cmd;
    cmd += "-Xmx2G -Xms1G ";
    cmd += "-Djava.library.path=\"" + nativesdir.string() + "\" ";
    cmd += "-cp \"" + classpath + "\" ";
    cmd += mainclass + " ";
    cmd += "--username " + username + " ";
    cmd += "--version " + versionid + " ";
    cmd += "--gameDir \"" + versiongamedir.string() + "\" ";
    cmd += "--assetsDir \"" + versionassetsdir.string() + "\" ";
    cmd += "--assetIndex " + assetindex + " ";
    cmd += "--uuid 00000000-0000-0000-0000-000000000000 ";
    cmd += "--accessToken 0 ";
    cmd += "--userType mojang";
    return cmd;
}

// This function reads the version json and downloads libraries and natives using downloadfile() and extractnatives().
//
void launcher::setuplauncher()
{
    // Read and parse the version JSON.
    //
    json j;
    {
        std::ifstream f(jsonpath);
        if (!f)
        {
            if (logconsole)
                logconsole("[Error] Failed to open JSON: " + jsonpath);
            return;
        }
        f >> j;
    }
    // Check for libraries array.
    //
    if (!j.contains("libraries") || !j["libraries"].is_array()) {
        if (logconsole)
            logconsole("[Error] Version JSON missing 'libraries' array.");
        return;
    }
    // Download normal libraries
    //
    for (const auto& lib : j["libraries"])
    {
        if (!lib.contains("downloads")) continue;
        if (!lib["downloads"].contains("artifact")) continue;
        const auto& artifact = lib["downloads"]["artifact"];
        std::string url = artifact.value("url", "");
        std::string apath = artifact.value("path", "");
        if (url.empty() || apath.empty())
        {
            if (logconsole)
                logconsole("[Warn] Skip library (missing URL or path).");
            continue;
        }
        fs::path targetpath = fs::path(libspath) / fs::path(apath).make_preferred();
        try {
            downloadfiles(url, targetpath.string());
        } catch (const std::exception& e) {
            if (logconsole)
                logconsole(std::string("[Error] Downloading failed: ") + e.what());
        }
    }
    // Extract native JARs.
    //
    for (const auto& lib : j["libraries"])
    {
        if (!lib.contains("name")) continue;
        std::string name = lib["name"];
        if (name.find("natives-windows") != std::string::npos)
        {
            if (!lib.contains("downloads")) continue;
            if (!lib["downloads"].contains("artifact")) continue;
            const auto& artifact = lib["downloads"]["artifact"];
            std::string apath = artifact.value("path", "");
            if (apath.empty()) continue;
            fs::path jar = fs::absolute(fs::path(libspath) / fs::path(apath)).make_preferred();
            if (fs::exists(jar))
            {
                extractnatives(jar.string());
            }
        }
    }
}

// This function runs setuplauncher(), builds the final java command and starts minecraft.
//
void launcher::launchprocess(const std::string& username)
{
    // Setup launcher and launch command.
    //
    setuplauncher();
    std::string args = buildlaunchcommand(username);
    if (args.empty()) {
        if (logconsole)
            logconsole("[Error] No launch arguments generated.");
        return;
    }
    // Check for java.
    //
    std::string javapath = ".minecraft\\java\\bin\\java.exe";
    if (!std::filesystem::exists(javapath))
    {
        if (logconsole)
            logconsole("[Error] Java not found: " + javapath);
        return;
    }
    if (logconsole)
        logconsole("[Launch] Starting Java process...");
    // Prepare command line: "java.exe <args>".
    //
    std::string commandline = "\"" + javapath + "\" " + args;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    HANDLE stdoutRead, stdoutWrite;
    HANDLE stderrRead, stderrWrite;
    // Create pipes for STDOUT.
    //
    CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0)) {
        if (logconsole)
            logconsole("[Error] Failed to create stdout pipe.");
        return;
    }
    // Create pipes for STDERR.
    //
    CreatePipe(&stderrRead, &stderrWrite, &sa, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);
    if (!CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
        if (logconsole)
            logconsole("[Error] Failed to create stderr pipe.");
        return;
    }
    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFO);
    si.hStdInput = NULL;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    // Create process.
    //
    BOOL success = CreateProcessA(
        NULL,
        commandline.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);
    if (!success)
    {
        if (logconsole)
            logconsole("[Error] Failed to start java process.");
        return;
    }
    // Thread: read stdout.
    //
    std::thread([this, stdoutRead]() {
        char buffer[1024];
        DWORD bytesread;
        while (ReadFile(stdoutRead, buffer, sizeof(buffer) - 1, &bytesread, NULL) && bytesread > 0)
        {
            buffer[bytesread] = '\0';
            if (logconsole)
                logconsole(std::string(buffer));
        }
        CloseHandle(stdoutRead);
    }).detach();
    // Thread: read stderr.
    //
    std::thread([=]() {
        WaitForSingleObject(pi.hProcess, INFINITE);
        minecraftrunning = false;
        if (logconsole)
            logconsole("[Launch] Minecraft closed.");
        CloseHandle(pi.hProcess);
    }).detach();
    CloseHandle(pi.hThread);
    if (logconsole)
        logconsole("[Launch] Minecraft launch request sent...");
}
