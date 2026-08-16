#include <windows.h>
#include <gl/GL.h>
#include <cmath>
#include <iostream>

#pragma comment(lib, "opengl32.lib")

// OpenGL Extension definitions for Shader Reset (Minecraft 1.8.9 / OptiFine shader protection)
typedef void (APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY* PFNGLGETINTEGERVPROC)(GLenum pname, GLint* params);

#define GL_CURRENT_PROGRAM 0x8B8D

PFNGLUSEPROGRAMPROC glUseProgramCustom = nullptr;

// MinHook
#include "MinHook.h"

// ImGui
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[91m"
#define COLOR_GREEN   "\033[92m"
#define COLOR_YELLOW  "\033[93m"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef BOOL(WINAPI* tWglSwapBuffers)(HDC hdc);
tWglSwapBuffers oWglSwapBuffers = nullptr;

HWND g_hWnd = NULL;
WNDPROC g_oWndProc = NULL;
bool g_Init = false;
bool g_ShowMenu = true;

struct FeatureSettings {
    bool aimbot = false;
    bool drawFov = false;
    float fovSize = 90.0f;
    float smoothness = 5.0f;

    bool espBoxes = false;
    bool espNames = false;
    bool espTracers = false;

    bool bhop = false;
    bool fly = false;
    float moveSpeed = 1.0f;
    bool showWatermark = true;
} g_Settings;

void EnableANSI() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        hOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
}

void ApplyOstinTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(6, 4);

    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.96f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.55f, 0.22f, 0.85f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.22f, 0.85f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.55f, 0.22f, 0.85f, 0.80f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.55f, 0.22f, 0.85f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.55f, 0.22f, 0.85f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.55f, 0.22f, 0.85f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.48f, 0.18f, 0.75f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.48f, 0.18f, 0.75f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.70f, 0.30f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.55f, 0.22f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.30f, 1.00f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.30f, 0.50f);
}

void RenderRainbowWatermark(const char* text, float posX, float posY, float fontSize = 2.0f) {
    ImFont* font = ImGui::GetFont();
    if (!font || !font->IsLoaded()) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    float currentX = posX;
    float time = ImGui::GetTime() * 1.0f;

    for (int i = 0; text[i] != '\0'; i++) {
        char buf[2] = { text[i], '\0' };

        float hue = fmodf(time + (i * 0.15f), 1.0f);
        ImVec4 colorVec;
        ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, colorVec.x, colorVec.y, colorVec.z);

        drawList->AddText(font, ImGui::GetFontSize() * fontSize,
            ImVec2(currentX + 2.0f, posY + 2.0f), ImColor(0, 0, 0, 200), buf);

        drawList->AddText(font, ImGui::GetFontSize() * fontSize,
            ImVec2(currentX, posY), ImColor(colorVec.x, colorVec.y, colorVec.z, 1.0f), buf);

        ImVec2 textSize = font->CalcTextSizeA(ImGui::GetFontSize() * fontSize, FLT_MAX, 0.0f, buf);
        currentX += textSize.x;
    }
}

void RenderOstinMenu() {
    ImGui::SetNextWindowSize(ImVec2(520, 360), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("EmreX - Minecraft 1.8.9 Edition", &g_ShowMenu, ImGuiWindowFlags_NoCollapse)) {

        if (ImGui::BeginTabBar("OstinTabs")) {

            if (ImGui::BeginTabItem("Combat")) {
                ImGui::Columns(2, "CombatColumns", false);

                ImGui::BeginChild("AimbotSection", ImVec2(0, 0), true);
                ImGui::TextDisabled("MAIN AIMBOT");
                ImGui::Separator();
                ImGui::Checkbox("Enable Aimbot", &g_Settings.aimbot);
                ImGui::Checkbox("Draw FOV", &g_Settings.drawFov);
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::BeginChild("AimbotSettings", ImVec2(0, 0), true);
                ImGui::TextDisabled("SETTINGS");
                ImGui::Separator();
                ImGui::SliderFloat("FOV", &g_Settings.fovSize, 1.0f, 180.0f, "%.1f deg");
                ImGui::SliderFloat("Smoothness", &g_Settings.smoothness, 1.0f, 20.0f, "%.1f");
                ImGui::EndChild();

                ImGui::Columns(1);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Visuals")) {
                ImGui::BeginChild("ESPSection", ImVec2(0, 0), true);
                ImGui::TextDisabled("PLAYER ESP");
                ImGui::Separator();
                ImGui::Checkbox("2D Box ESP", &g_Settings.espBoxes);
                ImGui::Checkbox("Name ESP", &g_Settings.espNames);
                ImGui::Checkbox("Snaplines / Tracers", &g_Settings.espTracers);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Misc")) {
                ImGui::Columns(2, "MiscColumns", false);

                ImGui::BeginChild("MovementSection", ImVec2(0, 0), true);
                ImGui::TextDisabled("MOVEMENT");
                ImGui::Separator();
                ImGui::Checkbox("BunnyHop", &g_Settings.bhop);
                ImGui::Checkbox("Fly Hack", &g_Settings.fly);
                ImGui::SliderFloat("Speed Multiplier", &g_Settings.moveSpeed, 1.0f, 5.0f, "%.1fx");
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::BeginChild("ExtraSection", ImVec2(0, 0), true);
                ImGui::TextDisabled("EXTRA");
                ImGui::Separator();
                ImGui::Checkbox("Rainbow Watermark", &g_Settings.showWatermark);
                ImGui::EndChild();

                ImGui::Columns(1);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Settings")) {
                ImGui::BeginChild("ConfigSection", ImVec2(0, 0), true);
                ImGui::TextDisabled("MENU CONFIG");
                ImGui::Separator();
                ImGui::Text("Menu Key: [INSERT]");
                if (ImGui::Button("Reset Settings", ImVec2(120, 25))) {
                    g_Settings = FeatureSettings();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_ShowMenu = !g_ShowMenu;
    }

    if (g_ShowMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;
    }

    return CallWindowProc(g_oWndProc, hWnd, uMsg, wParam, lParam);
}

BOOL WINAPI hkWglSwapBuffers(HDC hdc) {
    if (!g_Init) {
        g_hWnd = WindowFromDC(hdc);

        if (g_hWnd) {
            g_oWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            ApplyOstinTheme();

            ImGui_ImplWin32_Init(g_hWnd);
            ImGui_ImplOpenGL2_Init();

            glUseProgramCustom = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");

            g_Init = true;
        }
    }

    // Minecraft 1.8.9 OpenGL Shader & State Koruması
    GLint last_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);

    if (glUseProgramCustom)
        glUseProgramCustom(0);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_Settings.showWatermark) {
        RenderRainbowWatermark("EMREX 1.8.9", 15.0f, 15.0f, 2.0f);
    }

    if (g_ShowMenu) {
        RenderOstinMenu();
    }

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    glPopMatrix();
    glPopAttrib();

    if (glUseProgramCustom && last_program != 0)
        glUseProgramCustom(last_program);

    return oWglSwapBuffers(hdc);
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);

    EnableANSI();

    std::cout << COLOR_YELLOW << "[+] EmreX MC 1.8.9 Baslatiliyor..." << COLOR_RESET << std::endl;

    if (MH_Initialize() == MH_OK) {
        std::cout << COLOR_GREEN << "[+] MinHook baslatildi" << COLOR_RESET << std::endl;
    }
    else {
        std::cout << COLOR_RED << "[-] MinHook baslatilamadi!" << COLOR_RESET << std::endl;
        return 0;
    }

    HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
    if (hOpenGL) {
        void* pWglSwapBuffers = (void*)GetProcAddress(hOpenGL, "wglSwapBuffers");
        if (pWglSwapBuffers && MH_CreateHook(pWglSwapBuffers, &hkWglSwapBuffers, reinterpret_cast<void**>(&oWglSwapBuffers)) == MH_OK) {
            if (MH_EnableHook(pWglSwapBuffers) == MH_OK) {
                std::cout << COLOR_GREEN << "[+] OpenGL wglSwapBuffers Hook atildi" << COLOR_RESET << std::endl;
            }
        }
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    }
    return TRUE;
}