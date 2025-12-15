#pragma once
#include <flecs.h>
#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "RobotPal/Core/Framebuffer.h"

class EditorLayer {
public:
    EditorLayer(flecs::world& world);
    ~EditorLayer() = default;

    // 초기화 및 메인 루프 호출 함수
    void Init();
    void OnImGuiRender();

    // 외부에서 엔티티 선택이 필요할 때 (예: 로그 창에서 클릭 등)
    void SetSelectedEntity(flecs::entity entity) { m_SelectedEntity = entity; }
    flecs::entity GetSelectedEntity() const { return m_SelectedEntity; }

private:
    // --- UI 패널 그리기 ---
    void DrawDockSpace();
    void DrawMenuBar();
    void DrawViewport();      // Game View
    void DrawSceneHierarchy();// Entity List
    void DrawEntityNode(flecs::entity e);
    void DrawProperties();    // Inspector
    
    // --- 기능 ---
    void DrawGizmo(flecs::entity cameraEnt);
    void HandleShortcuts();
    
    // 파일 I/O
    void NewScene();
    void SaveScene();
    void LoadScene();

private:
    flecs::world& m_World;
    
    // [핵심] 뷰포트용 FBO (에디터가 영구 소유)
    std::shared_ptr<Framebuffer> m_ViewportFBO;

    // 선택된 엔티티
    flecs::entity m_SelectedEntity;

    // Viewport 상태 관리
    glm::vec2 m_ViewportSize = { 1280.0f, 720.0f };
    glm::vec2 m_ViewportBounds[2];
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;

    // Gizmo Operation (ImGuizmo::TRANSLATE, ROTATE, SCALE)
    int m_GizmoOperation = -1; 
};