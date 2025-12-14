#ifndef __ENGINEAPP_H__
#define __ENGINEAPP_H__
#include <memory>
#include <glm/glm.hpp>
#include <flecs.h>

class Window;
class SceneManager;
class EditorLayer;

class EngineApp
{
public:
    void Run();


private:
    void Init();
    void MainLoop();
    void Shutdown();
    
    float m_LastFrameTime;
    std::shared_ptr<EditorLayer> m_EditorLayer;
    std::shared_ptr<SceneManager> m_SceneManager;
    std::shared_ptr<Window> m_Window;
    
    flecs::world m_World;
};

#endif