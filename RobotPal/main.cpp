#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <limits>
#include <fstream> // CSV 파일 읽기를 위해
#include <sstream> // 문자열 파싱을 위해
#include <cmath>   // 오차 계산(sqrtf)을 위해
#include <limits>
#include <filesystem>
#include <sstream>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <functional>
static std::function<void()> MainLoopForEmscriptenP;
static void MainLoopForEmscripten() { MainLoopForEmscriptenP(); }
#define EMSCRIPTEN_MAINLOOP_BEGIN MainLoopForEmscriptenP = [&]() { do
#define EMSCRIPTEN_MAINLOOP_END \
    while (0)                   \
        ;                       \
    }                           \
    ;                           \
    emscripten_set_main_loop(MainLoopForEmscripten, 0, true)
#else
#define EMSCRIPTEN_MAINLOOP_BEGIN
#define EMSCRIPTEN_MAINLOOP_END
#endif

std::string csv =
    R"(
x,y
-1.252533587,3.39996726
-1.260352771,3.278731356
-1.244533282,3.175862366
-1.256977519,2.937028121
-1.246046184,2.853542793
-1.145580655,2.615776232
-1.058528158,2.614427404
-0.9605277,2.655442533
-0.691070266,2.610061663
-0.642611269,2.581211103
-0.347159803,2.687671868
-0.238884487,2.719528305
-0.026825252,2.649864224
0.127658481,2.706991561
0.336446177,2.646171857
0.547075619,2.674623014
0.79686986,2.682532935
0.946437077,2.656436873
1.0921141,2.610782793
1.125391494,2.766057173
1.184440121,2.640974663
1.211275436,2.774203985
1.210801642,2.983226228
1.214699816,3.188006023
1.213800022,3.304716857
1.21748022,3.408415855
1.211308872,3.436360236
1.225950005,3.765583831
1.223843085,3.768406724
1.224557451,3.999448492
1.221410617,4.077535806
1.210705863,4.320082871
1.190736119,4.41501359
1.091001688,4.440767462
0.965954579,4.455702675
0.780033641,4.484380415
0.554784667,4.491064612
0.311036899,4.47502664
0.166480836,4.458782332
-0.021158548,4.489950146
-0.461245356,4.486351828
-0.464377308,4.486028724
-0.650928737,4.472884056
-0.798652179,4.420722192
-1.232593874,4.410485111
-1.040683687,4.352214177
-1.244180324,4.192197152
-1.2834856,3.918996483
-1.242946206,3.708042094
-1.232710589,3.588712683
-1.207750177,3.559664916
-1.281747112,3.409047157
-1.275786349,2.874534213
-1.203497253,2.671061995
-1.108145965,2.639887694
-1.021775821,2.677397899
-1.270429244,2.659264273
-1.290199177,2.630971113
-1.171262089,2.600388821
-0.984607787,2.66695372
-0.991136797,2.644784423
-0.783313841,2.637567056
-0.506775994,2.669947296
-0.329071675,2.736958257
-0.194002223,2.702894641
0.122116002,2.632738195
0.290049538,2.708473676
0.514544745,2.688382674
0.780049155,2.683732481
0.99401847,2.710473759
1.224494575,2.770949941
1.213283691,2.863177596
1.190289997,3.05901156
1.218571816,3.069627197
1.207700271,3.37845822
1.173921188,3.750574495
1.220055952,3.797084708
1.209679931,4.386735558
1.111494785,4.451679751
1.095511398,4.512696054
0.86502765,4.426016452
0.534799799,4.498320706
0.286621916,4.480842318
0.187184172,4.466079051
-0.12319745,4.43829048
-0.176010694,4.489356566
-0.26735801,4.461924886
-0.648842516,4.457458962
-0.854859732,4.459661204
-1.020728012,4.362168535
-1.023355331,4.354691409
-0.999165997,4.335585522
-1.128625192,4.307507275
-1.149418993,4.392729799
-1.234027066,4.022094905
-1.259303171,3.100588639
-1.246055386,2.815987403
-1.173835116,2.684118569
-1.14560169,2.647260275
-1.226294992,2.642728346
-0.972813008,2.613858367
-1.152643416,2.639170718
-1.131989782,2.598937723
-0.758597689,2.580819549
-0.57210279,2.597748717
-0.387120026,2.621571682
-0.122050217,2.66720898
-0.104201495,2.687981036
0.334351465,2.669140142
0.373849165,2.69418574
0.555475482,2.672891129
0.722328562,2.684518848
1.070910994,2.684259608
1.319508003,2.638446253
1.165015685,2.666615543
1.222707868,2.680099124
1.208985698,2.76423875
1.238309832,3.048063116
1.13769931,3.42603857
1.239653179,3.549549495
1.235636487,3.695759325
1.236399236,3.867158767
1.212537127,3.923472807
0.989495013,4.215424014
1.143050604,4.311036455
0.7934032,4.490446677
0.827349286,4.46397728
0.88722371,4.432068827
0.87424046,4.505974214
0.022827064,4.509942231
-0.008581779,4.469991762
0.126295931,4.438203391
-0.308515933,4.479388119
-0.854366281,4.428335834
-0.903703383,4.489957705
-0.827781078,4.423209071
-1.033259754,4.254983966
-1.031254299,4.245060052
-1.104217652,4.152645417
-1.19653388,3.973061717
-1.29391822,3.527090245
-1.281514009,3.214896023
-1.166199439,2.958218972
-1.081009647,2.868764112
-1.216994229,2.6835308
-1.178245032,2.674976266
-1.154591,2.684178487
-0.963783049,2.676488122
-1.156839643,2.679560241
-0.92850694,2.692684269
-0.572419197,2.638623934
)";

// --- CSV 데이터 로더 ---
bool LoadDataFromCSV(const char *file_name, std::vector<float> &out_x, std::vector<float> &out_y)
{
    std::istringstream file(csv);
    // if (!file.is_open())
    // {
    //     std::cerr << "Error: Could not open CSV file: " << file_name << std::endl;
    //     return false;
    // }

    std::string line;
    // 헤더(x,y) 스킵
    if (!std::getline(file, line))
    {
        std::cerr << "Error: CSV file is empty." << std::endl;
        return false;
    }

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string value;
        float x, y;

        // X 값 읽기
        if (!std::getline(ss, value, ','))
            continue;
        try
        {
            x = std::stof(value);
        }
        catch (...)
        {
            continue;
        }

        // Y 값 읽기
        if (!std::getline(ss, value, ','))
            continue;
        try
        {
            y = std::stof(value);
        }
        catch (...)
        {
            continue;
        }

        out_x.push_back(x);
        out_y.push_back(y);
    }

    // file.close();
    return !out_x.empty();
}

// --- 오차 계산 헬퍼 ---
float CalculateError(float x1, float y1, float x2, float y2)
{
    return std::sqrt(std::pow(x1 - x2, 2) + std::pow(y1 - y2, 2));
}

int main(void)
{

    GLFWwindow *window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char *glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    // Create window with graphics context
    window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // --- 데이터 및 상태 변수 초기화 ---
    std::vector<float> all_data_x;
    std::vector<float> all_data_y;
    std::vector<float> plot_history_x;
    std::vector<float> plot_history_y;

    bool data_loaded = LoadDataFromCSV("/x_y_data.csv", all_data_x, all_data_y);
    if (!data_loaded)
    {
        // 데이터 로드 실패 시 에러 메시지를 표시하고 종료
        // (실제 앱에서는 ImGui 창에 에러를 표시할 수 있음)
        std::cerr << "Failed to load data. Exiting." << std::endl;
        glfwTerminate();
        return -1;
    }

    size_t data_index = 0;
    size_t total_data_count = all_data_x.size();

    float current_x = all_data_x[0];
    float current_y = all_data_y[0];
    float current_error = 0.0f;

    // '정답값' (가정) - 필요에 따라 이 값을 수정하세요
    const ImVec2 TARGET_POINT = ImVec2(0.0f, 0.0f);

    double last_update_time = 0.0;
    const double update_interval = 0.1; // 0.1초

#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- 실시간 데이터 업데이트 로직 ---
        double current_time = glfwGetTime();
        if (current_time - last_update_time > update_interval && total_data_count > 0)
        {
            last_update_time = current_time;

            // 다음 데이터 인덱스 (순환)
            data_index = (data_index + 1) % total_data_count;

            // 현재 값 업데이트
            current_x = all_data_x[data_index];
            current_y = all_data_y[data_index];

            // 플롯 히스토리에 추가
            plot_history_x.push_back(current_x);
            plot_history_y.push_back(current_y);

            // (선택적) 히스토리가 너무 길어지면 오래된 데이터 삭제
            // if (plot_history_x.size() > 1000) {
            //     plot_history_x.erase(plot_history_x.begin());
            //     plot_history_y.erase(plot_history_y.begin());
            // }

            // 오차 계산
            current_error = CalculateError(current_x, current_y, TARGET_POINT.x, TARGET_POINT.y);
        }

        // --- 요청된 레이아웃 그리기 ---
        {
            // 1. 전체 화면을 덮는 메인 창 설정
            ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

            ImGui::Begin("MainLayoutWindow", NULL, window_flags);

            float window_width = ImGui::GetContentRegionAvail().x;
            float window_height = ImGui::GetContentRegionAvail().y;

            // --- 2. 왼쪽: 2D Plot (70%) ---
            ImGui::BeginChild("PlotPane", ImVec2(window_width * 0.7f, window_height), true);

            // ImPlot 플래그 (축 자동 맞춤, 비율 고정)
            if (ImPlot::BeginPlot("2D Plot", ImVec2(-1, -1), ImPlotFlags_Equal))
            {
                // 플롯 히스토리 그리기
                if (!plot_history_x.empty())
                {
                    ImPlot::PlotLine("Data Points", plot_history_x.data(), plot_history_y.data(), (int)plot_history_x.size());
                }

                // 현재 위치를 마커로 표시
                ImPlot::PlotScatter("Current", &current_x, &current_y, 1);

                ImPlot::EndPlot();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // --- 3. 오른쪽: 정보 (30%) ---
            ImGui::BeginChild("InfoPane", ImVec2(0, window_height), false); // 남은 너비 모두 사용

            float info_pane_height = ImGui::GetContentRegionAvail().y;

            // --- 3-1. 오른쪽 위: 현재 x, y 값 ---
            ImGui::BeginChild("CurrentValue", ImVec2(0, info_pane_height * 0.5f), true);
            ImGui::Text("Current Position");
            ImGui::Separator();
            ImGui::Text("X: %.4f", current_x);
            ImGui::Text("Y: %.4f", current_y);
            ImGui::EndChild();

            // --- 3-2. 오른쪽 아래: 오차 값 ---
            ImGui::BeginChild("ErrorValue", ImVec2(0, 0), true); // 남은 높이 모두 사용
            ImGui::Text("Error (from Target: %.1f, %.1f)", TARGET_POINT.x, TARGET_POINT.y);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.8f, 1.0f), "%.4f", current_error);
            ImGui::EndChild();

            ImGui::EndChild(); // InfoPane

            ImGui::End(); // MainLayoutWindow
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow *backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext(); // ImPlot 컨텍스트 파괴
    ImGui::DestroyContext();

    glfwDestroyWindow(window);

    glfwTerminate();
    return 0;
}