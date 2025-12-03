#include "RobotPal/SandboxScene.h"
#include "RobotPal/Buffer.h"
#include "RobotPal/GlobalComponents.h"
#include "RobotPal/Core/AssetManager.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Network/NetworkEngine.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp> // [중요] 쿼터니언 -> 행렬 변환용
#include <imgui.h>
#include <glad/gles2.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
std::shared_ptr<Framebuffer> camView;
void SandboxScene::OnEnter()
{
    auto hdrID = AssetManager::Get().LoadTextureHDR("/Assets/airport.hdr");
    m_World.set<Skybox>({hdrID, 1.0f, 0.0f});

    auto modelPrefab = AssetManager::Get().GetPrefab(m_World, "/Assets/jetank.glb");
    auto prefabEntity = CreateEntity("mainModel");
    prefabEntity.GetHandle().is_a(modelPrefab);

    prefabEntity.SetLocalPosition(glm::vec3(0.f, 0.f, 0.7f));
    prefabEntity.SetLocalRotation(glm::radians(glm::vec3(0.f, -90.f, 0.f)));

    auto prefabEntity2 = CreateEntity("mainModel2");
    prefabEntity2.GetHandle().is_a(modelPrefab);
    prefabEntity2.SetLocalPosition({0.4f, 0.f, 0.5f});
    prefabEntity2.SetLocalRotation(glm::radians(glm::vec3(0.f, -211.f, 0.f)));

    auto mapPrefab = AssetManager::Get().GetPrefab(m_World, "/Assets/map.glb");
    auto map=CreateEntity("map");
    map.GetHandle().is_a(mapPrefab);

    auto mainCam=CreateEntity("mainCam");
    mainCam.Set<Camera>({});
    mainCam.SetLocalPosition({0.1f, 0.5f, 1.1f});
    mainCam.SetLocalRotation(glm::radians(glm::vec3(-35.f, -0.15f, 0.f)));

    camView=Framebuffer::Create(400, 400);
    auto robotCamera=CreateEntity("robotCam");
    robotCamera.Set<Camera>({80.f, 0.001f, 1000.f})
               .Set<RenderTarget>({camView})
               .Set<VideoSender>({400, 400, 15.0f});
    
    auto attachPoint=prefabEntity.FindChildByNameRecursive(prefabEntity, "Cam");
    if(attachPoint)
    {
        robotCamera.SetParent(attachPoint);
    }
    m_StreamingManager = std::make_shared<StreamingManager>();
    
}  

void SandboxScene::OnUpdate(float dt)
{
    if (m_StreamingManager && m_StreamingManager->IsConnected())
    {
        m_TimeSinceLastSend += dt;

        auto q = m_World.query<const VideoSender, const RenderTarget>();
        
        // Assuming a single VideoSender entity.
        q.each([this](const VideoSender& sender, const RenderTarget& target) {
            if (m_TimeSinceLastSend >= 1.0f / sender.fpsLimit)
            {
                m_TimeSinceLastSend = 0.f;

                auto texture = target.fbo->GetColorAttachment();
                auto pixel_data = texture->GetAsyncData();
                if (!pixel_data.empty())
                {
                    #ifdef __EMSCRIPTEN__
                    int channels = 4; // GetAsyncData returns RGBA for WebGL
                    #else
                    int channels = (texture->GetFormat() == TextureFormat::RGBA8) ? 4 : 3;
                    #endif
                    m_StreamingManager->SendFrame(pixel_data, texture->GetWidth(), texture->GetHeight(), channels);
                }
            }
        });
    }
}

void SandboxScene::OnExit()
{

}

void SandboxScene::OnImGuiRender()
{
    // --- Streaming Manager UI ---
    ImGui::Begin("Streaming Controls");
    if (m_StreamingManager)
    {
        ImGui::Text("Status: %s", m_StreamingManager->GetStatusMessage().c_str());
        if (m_StreamingManager->IsConnected())
        {
            if (ImGui::Button("Disconnect"))
            {
                m_StreamingManager->Disconnect();
            }
        }
        else
        {
            // The python server is on port 9999
            if (ImGui::Button("Connect to ws://127.0.0.1:9999"))
            {
                m_StreamingManager->ConnectToServer("ws://127.0.0.1:9999");
            }
        }
    }
    else
    {
        ImGui::Text("Streaming Manager is not initialized.");
    }
    ImGui::End();
    
    // --- Robot Camera View ---
    ImGui::Begin("robotCam");
    ImGui::Image((void*)(intptr_t)camView->GetColorAttachment()->GetID(), ImVec2(400, 400), ImVec2(0, 1), ImVec2(1, 0)); // Flipped UVs for correct orientation
    ImGui::End();

    // --- Scene Graph & Inspector ---
    ImGui::Begin("Scene Graph & Inspector");

    static flecs::entity selectedEntity;

    if (ImGui::BeginTable("SplitLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableNextColumn();
        ImGui::BeginChild("HierarchyRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        ImGui::TextDisabled("Hierarchy");
        ImGui::Separator();

        auto drawNode = [&](auto&& self, flecs::entity e) -> void {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selectedEntity == e) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            bool hasChildren = false;
            e.children([&](flecs::entity) { hasChildren = true; });
            if (!hasChildren) {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }

            if (e.name() == "mainModel") {
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }

            const char* name = e.name().c_str();
            std::string label = (name && *name) ? name : std::to_string(e.id());

            bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)e.id(), flags, "%s", label.c_str());

            if (ImGui::IsItemClicked()) {
                selectedEntity = e;
            }

            if (opened) {
                if (hasChildren) {
                    e.children([&](flecs::entity child) {
                        self(self, child);
                    });
                }
                ImGui::TreePop();
            }
        };

        flecs::entity rootModel = m_SceneRoot;
        if (rootModel.is_alive()) {
            rootModel.children([&](flecs::entity child) {
                drawNode(drawNode, child);
            });
        }

        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("InspectorRegion", ImVec2(0, 0), false);

        ImGui::TextDisabled("Inspector");
        ImGui::Separator();

        if (selectedEntity.is_alive()) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[%s] Properties", selectedEntity.name().c_str());
            ImGui::Spacing();

            Entity selectedEntityWrapper(selectedEntity);

            {
                auto pos = selectedEntityWrapper.GetLocalPosition();
                if(ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f))
                {
                    selectedEntityWrapper.SetLocalPosition(pos);
                }
            }

            {
                glm::vec3 eulerAngles = glm::degrees(selectedEntityWrapper.GetLocalRotation());
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerAngles), 1.0f)) 
                {
                    selectedEntityWrapper.SetLocalRotation(glm::radians(eulerAngles));
                }
            }

            {
                auto scale = selectedEntityWrapper.GetLocalScale();
                if(ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.05f))
                {
                    selectedEntityWrapper.SetLocalScale(scale);
                }
            }
        } else {
            ImGui::TextDisabled("No entity selected.");
        }
        
        ImGui::EndChild();
        ImGui::EndTable();
    }

    ImGui::End();
}