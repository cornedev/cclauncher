// MIT License
// Copyright (c) 2025 cornedev

// Dependency headers.
//
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers.
#include <stdio.h>
#include <filesystem>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include "../include/java.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h" 
namespace fs = std::filesystem;

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

std::vector<std::string> consolelogs;
std::mutex logmutex;

void ImGuiLog(const std::string& msg)
{
    // Create timestamp.
    //
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
        tm.tm_hour, tm.tm_min, tm.tm_sec);
    // Push final formatted message.
    //
    consolelogs.push_back(std::string("[") + buf + "] " + msg);
}

// Change mode to dark or light.
//
bool mode = true;

// Main code.
//
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions.
    //
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Create mainwindow with graphics context.
    //
    GLFWwindow* mainwindow = glfwCreateWindow(535, 500, "cclauncher", nullptr, nullptr);
    if (mainwindow == nullptr) return 1;
    glfwMakeContextCurrent(mainwindow);
    // Create window icon using stb_image.
    //
    int icon_width, icon_height, icon_channels;
    unsigned char* icon_pixels = stbi_load("gfx/icon.png", &icon_width, &icon_height, &icon_channels, 4);
    if (icon_pixels)
    {
        GLFWimage images[1];
        images[0].width = icon_width;
        images[0].height = icon_height;
        images[0].pixels = icon_pixels;
        glfwSetWindowIcon(mainwindow, 1, images);
        stbi_image_free(icon_pixels);
    }
    // Enable vsync.
    //
    glfwSwapInterval(1);

    // Setup Dear ImGui context.
    //
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls.
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking.
    io.IniFilename = nullptr;

    ImGuiStyle& style = ImGui::GetStyle();
    // Setup Dear ImGui style.
    //
    if (mode == true)
    {
        ImGui::StyleColorsDark();
    }
    else if (mode == false)
    {
        ImGui::StyleColorsLight();
    }

    // Setup Platform/Renderer backends.
    //
    ImGui_ImplGlfw_InitForOpenGL(mainwindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts.
    //
    style.FontSizeBase = 16.0f;
    ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\Arial.ttf");
    IM_ASSERT(font != nullptr);

    // Check version folder path for combobox.
    //
    std::vector<std::string> versionnames;
    std::vector<const char*> versionitems;
    std::string versionspath = ".minecraft/versions";
    try {
        for (const auto& entry : fs::directory_iterator(versionspath))
        {
            if (entry.is_directory())
            {
                versionnames.push_back(entry.path().filename().string());
            }
        }
    }
    catch (...) {
        printf("Error: no version folder.\n");
    }
    for (auto& v : versionnames)
        versionitems.push_back(v.c_str());
    if (versionitems.empty())
        versionitems.push_back("error: no versions found.");

    char buf[64] = "";
    int selected = 0;
    static launcher* launcherglobal = nullptr;
    // booleans for popups.
    //
    bool usernamepopup = false;
    bool launchpopup = false;
    // boolean for the credit window at startup.
    //
    bool creditsmsg = true;
    while (!glfwWindowShouldClose(mainwindow))
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(mainwindow, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame.
        //
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Decide what should happen when a popup boolean is true.
        //
        if (usernamepopup)
            ImGui::OpenPopup("Error");
        if (launchpopup)
            ImGui::OpenPopup("Success");

        ImGuiWindowFlags popup_window_flags = ImGuiWindowFlags_AlwaysAutoResize
                              | ImGuiWindowFlags_NoMove;
        // Username error popup.
        //
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        if (ImGui::BeginPopupModal("Error", nullptr, popup_window_flags))
        {
            ImGui::Text("username is empty.");
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            if (ImGui::Button("OK"))
            {
                ImGui::CloseCurrentPopup();
                usernamepopup = false;
            }
            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        // Launch success popup.
        //
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        if (ImGui::BeginPopupModal("Success", nullptr, popup_window_flags))
        {
            ImGui::Text("minecraft is launching...");
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            if (ImGui::Button("OK"))
            {
                ImGui::CloseCurrentPopup();
                launchpopup = false;
            }
            ImGui::PopStyleVar();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        // Create the credit window at startup.
        //
        if (creditsmsg)
        {
            ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2( (535-400)/2, (500-200)/2 ), ImGuiCond_Always);

            ImGuiWindowFlags credit_window_flags = ImGuiWindowFlags_NoResize
                                    | ImGuiWindowFlags_NoCollapse
                                    | ImGuiWindowFlags_NoMove
                                    | ImGuiWindowFlags_NoSavedSettings
                                    | ImGuiWindowFlags_NoDocking;
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
                ImGui::Begin("Credits", nullptr, credit_window_flags);

                ImGui::TextWrapped("cclauncher v1.0\ncopyright (c) 2025 cornedev\n\nThanks for using my little launcher :)");
                
                ImGui::SetCursorPos(ImVec2(150, 150));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                if (ImGui::Button("OK", ImVec2(100, 30)))
                {
                    creditsmsg = false; // Close credits window and continue to main launcher.
                }
                ImGui::PopStyleVar();

                ImGui::End();
                ImGui::PopStyleVar();
            }

            // Skip rendering the rest of the launcher until credits are closed.
            //
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(mainwindow, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(mainwindow);

            continue; // Skip the rest of the loop for this frame.
        }
        
        // This section creates the launch window with username input, combobox and launch button.
        //
        ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(220, 300), ImGuiCond_Once);

        ImGuiWindowFlags main_window_flags = ImGuiWindowFlags_NoResize
                              | ImGuiWindowFlags_NoSavedSettings
                              | ImGuiWindowFlags_NoDocking
                              | ImGuiWindowFlags_NoMove
                              | ImGuiWindowFlags_NoSavedSettings;
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
            ImGui::Begin("Launch", nullptr, main_window_flags);

            ImGui::SetCursorPos(ImVec2(50, 240));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            if (ImGui::Button("Launch", ImVec2(120, 30)))
            {
                std::string username = buf;
                if (username.empty())
                {
                    usernamepopup = true;
                }
                else
                {
                    launchpopup = true;
                    std::string selectedversion;
                    if (!versionitems.empty() && versionitems[selected] && std::string(versionitems[selected]).find("error") == std::string::npos)
                        selectedversion = versionitems[selected];
                    static std::mutex launchermutex;
                    std::lock_guard<std::mutex> lock(launchermutex);
                    if (launcherglobal)
                    {
                        delete launcherglobal;
                        launcherglobal = nullptr;
                    }
                    // Use 1.21 as default version if no custom version is given.
                    //
                    launcher* launcherinstance = !selectedversion.empty() ? new launcher(selectedversion, ImGuiLog) : new launcher("1.21", ImGuiLog);
                    launcherglobal = launcherinstance;
                    launcher* threadlauncher = launcherinstance;

                    std::thread([threadlauncher, username]() {
                        threadlauncher->launchprocess(username);
                    }).detach();
                }
            }
            ImGui::PopStyleVar();

            ImGui::SetCursorPos(ImVec2(10, 35));
            ImGui::Text("Username");

            ImGui::SetCursorPos(ImVec2(10, 55));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushItemWidth(200);
            ImGui::InputText("##username", buf, 64);
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();

            ImGui::SetCursorPos(ImVec2(10, 110));
            ImGui::Text("Version");

            ImGui::SetCursorPos(ImVec2(10, 130));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushItemWidth(200);
            if (ImGui::Combo("##version", &selected, versionitems.data(), versionitems.size()))
            {
                printf("version: %s\n", versionitems[selected]);
            }
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();

            ImGui::End();
            ImGui::PopStyleVar();
        }

        // This section creates the console logging window.
        //
        ImGui::SetNextWindowPos(ImVec2(5, 325), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(525, 150), ImGuiCond_Once);

        ImGuiWindowFlags console_window_flags = ImGuiWindowFlags_NoResize
                              | ImGuiWindowFlags_NoSavedSettings
                              | ImGuiWindowFlags_NoDocking
                              | ImGuiWindowFlags_NoMove
                              | ImGuiWindowFlags_NoSavedSettings
                              | ImGuiWindowFlags_HorizontalScrollbar;
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
            ImGui::Begin("Console", nullptr, console_window_flags);

            // Autoscroll.
            //
            static bool scrolly = true;
            {
                std::lock_guard<std::mutex> lock(logmutex);
                if (ImGui::GetScrollY() < ImGui::GetScrollMaxY())
                    scrolly = false; else scrolly = true;
                for (const auto& line : consolelogs)
                {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (scrolly)
                    ImGui::SetScrollHereY(1.0f);
            }

            ImGui::End();
            ImGui::PopStyleVar();
        }

        // This section creates the skin select window (not finished).
        //
        ImGui::SetNextWindowPos(ImVec2(230, 5), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Once);

        ImGuiWindowFlags skin_window_flags = ImGuiWindowFlags_NoResize
                              | ImGuiWindowFlags_NoSavedSettings
                              | ImGuiWindowFlags_NoDocking
                              | ImGuiWindowFlags_NoMove;
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
            ImGui::Begin("Skin select", nullptr, skin_window_flags);
            ImGui::End();
            ImGui::PopStyleVar();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(mainwindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(mainwindow);
    }

    // Cleanup.
    //
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(mainwindow);
    glfwTerminate();
    return 0;
}
