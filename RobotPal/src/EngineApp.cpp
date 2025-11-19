#include "RobotPal/EngineApp.h"
#include "RobotPal/Window.h"
#include "RobotPal/ImGuiManager.h"
#include "RobotPal/Util/emscripten_mainloop.h"
#include "RobotPal/SceneManager.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <glad/gles2.h>

#include "RobotPal/VertexArray.h"
#include "RobotPal/Buffer.h"
#include "RobotPal/Shader.h"

#include "RobotPal/GlobalComponents.h"
#include "RobotPal/SandboxScene.h"

void EngineApp::Run()
{
    Init();
    MainLoop();
    Shutdown();
}

void EngineApp::Init()
{
    m_Window=std::make_shared<Window>(1280, 720, "RobotPal");
    m_Window->Init();
    ImGuiManager::Get().Init(m_Window->GetNativeWindow());

    m_SceneManager = std::make_shared<SceneManager>(m_World);
    m_SceneManager->LoadScene<SandboxScene>();

    m_World.set<WindowData>({ (float)1280, (float)720});
}

void EngineApp::MainLoop()
{
    m_LastFrameTime=(float)glfwGetTime();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glEnable(GL_DEPTH_TEST);
#ifdef __EMSCRIPTEN__
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!m_Window->ShouldClose())
#endif
    {
        float currentFrame = (float)glfwGetTime();
        float dt = currentFrame - m_LastFrameTime;
        m_LastFrameTime = currentFrame;

        if (dt > 0.1f) dt = 0.1f;

        m_Window->PollEvents();
        if (m_Window->IsMinimized())
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }
        
        // Clear the screen
        int display_w, display_h;
        glfwGetFramebufferSize((GLFWwindow*)m_Window->GetNativeWindow(), &display_w, &display_h);
        m_World.set<WindowData>({ (float)display_w, (float)display_h});

        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        m_SceneManager->OnUpdate(dt);
        m_World.progress(dt);

        // Start the Dear ImGui frame
        ImGuiManager::Get().NewFrame();

        //draw gui
        {
            m_SceneManager->OnImGuiRender();
        }

        ImGuiManager::Get().PrepareRender();
        ImGuiManager::Get().Render(m_Window->GetNativeWindow());
        
        m_Window->SwapBuffers();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
}

void EngineApp::Shutdown()
{
    ImGuiManager::Get().Shutdown();
    m_Window->Shutdown();
}