#include "RobotPal/EditorLayer.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Util/SceneSerializer.h"
#include "RobotPal/Util/FileDialog.h"

#include <imgui.h>
#include <imgui_internal.h> // DockBuilder
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glad/gles2.h> // glClearColor 등

EditorLayer::EditorLayer(flecs::world& world) 
    : m_World(world) {
    m_GizmoOperation = ImGuizmo::TRANSLATE;
}

void EditorLayer::Init() {
    // 에디터 실행 시 자신만의 렌더링 캔버스(FBO) 생성
    m_ViewportFBO = Framebuffer::Create(1280, 720, TextureFormat::RGB8, true);
}

void EditorLayer::OnImGuiRender() {
    DrawDockSpace();

    DrawMenuBar();
    DrawViewport();
    DrawSceneHierarchy();
    DrawProperties();
    
    HandleShortcuts();

    ImGui::End(); // End DockSpace
}

void EditorLayer::DrawDockSpace() {
    static bool dockspaceOpen = true;
    static bool opt_fullscreen = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("RobotPal Editor", &dockspaceOpen, window_flags);
    ImGui::PopStyleVar();
    if (opt_fullscreen) ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("RobotPalDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        // 초기 레이아웃 설정 (최초 1회)
        static bool first_time = true;
        if (first_time) {
            first_time = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow("Properties", dock_right);
            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left);
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }
}

void EditorLayer::DrawViewport() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::Begin("Viewport");

    // 1. 뷰포트 크기 변경 감지 및 FBO 리사이즈
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    if (m_ViewportFBO->GetWidth() != (int)viewportPanelSize.x || 
        m_ViewportFBO->GetHeight() != (int)viewportPanelSize.y) 
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) 
        {
            m_ViewportFBO->Resize((int)viewportPanelSize.x, (int)viewportPanelSize.y);
            m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
        }
    }
    // 좌표 정보 갱신 (기즈모 및 피킹용)
    auto minRegion = ImGui::GetWindowContentRegionMin();
    auto maxRegion = ImGui::GetWindowContentRegionMax();
    auto offset = ImGui::GetWindowPos();
    m_ViewportBounds[0] = { minRegion.x + offset.x, minRegion.y + offset.y };
    m_ViewportBounds[1] = { maxRegion.x + offset.x, maxRegion.y + offset.y };

    m_ViewportFocused = ImGui::IsWindowFocused();
    m_ViewportHovered = ImGui::IsWindowHovered();

    // 2. [핵심] 활성화된 카메라 찾기
    // 이름으로 찾거나 태그로 찾습니다. 여기서는 "MainCamera" 이름 사용
    flecs::entity cameraEnt = m_World.lookup("SceneRoot::MainCamera");
    
    // 만약 카메라가 살아있다면 렌더링 연결
    if (cameraEnt.is_alive() && cameraEnt.has<Camera>()) {
        
        // A. 렌더링 타겟 연결 (카메라에게 내 FBO에 그리라고 지시)
        const RenderTarget* rt = cameraEnt.try_get<RenderTarget>();
        // 중복 설정 방지 (값이 다를 때만 set)
        if (!rt || rt->fbo != m_ViewportFBO) {
            cameraEnt.set<RenderTarget>({ m_ViewportFBO });
        }

        // B. 렌더링 결과(텍스처) 출력
        // OpenGL 텍스처(Bottom-Left)를 ImGui(Top-Left)에 맞게 뒤집어 그림
        auto textureID = m_ViewportFBO->GetColorAttachment()->GetID();
        ImGui::Image(reinterpret_cast<void*>(textureID), viewportPanelSize, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        // C. 기즈모 그리기
        DrawGizmo(cameraEnt);
    } 
    else {
        // [예외 처리] 카메라가 없을 때 (검은 화면)
        
        // FBO를 수동으로 클리어
        m_ViewportFBO->Bind();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        m_ViewportFBO->Unbind();

        // 검은 텍스처 출력
        auto textureID = m_ViewportFBO->GetColorAttachment()->GetID();
        ImGui::Image(reinterpret_cast<void*>(textureID), viewportPanelSize, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        // 경고 메시지
        ImVec2 textSize = ImGui::CalcTextSize("No Active Camera");
        ImGui::SetCursorPos(ImVec2((viewportPanelSize.x - textSize.x) * 0.5f, (viewportPanelSize.y - textSize.y) * 0.5f));
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No Active Camera");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::DrawGizmo(flecs::entity cameraEnt) {
    // if (!m_SelectedEntity.is_alive() || m_GizmoOperation == -1) return;

    // // 카메라 데이터 가져오기
    // auto* cam = cameraEnt.get_mut<Camera*>();
    // auto* camTrans = cameraEnt.get_mut<TransformMatrix*>();
    // if (!cam || !camTrans) return;

    // // View Matrix: 카메라 Transform의 역행렬
    // glm::mat4 view = glm::inverse((glm::mat4)*camTrans);
    
    // // Projection Matrix
    // float aspect = m_ViewportSize.x / m_ViewportSize.y;
    // glm::mat4 projection = glm::perspective(glm::radians(cam->fov), aspect, cam->nearPlane, cam->farPlane);

    // // 대상 Entity의 Transform
    // auto* transformComp = m_SelectedEntity.get_mut<TransformMatrix*>();
    // glm::mat4 transform = *transformComp;

    // // ImGuizmo 설정
    // ImGuizmo::SetOrthographic(false);
    // ImGuizmo::SetDrawlist();
    // ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, 
    //                   m_ViewportBounds[1].x - m_ViewportBounds[0].x, 
    //                   m_ViewportBounds[1].y - m_ViewportBounds[0].y);

    // // Snapping (Ctrl 키)
    // bool snap = ImGui::GetIO().KeyCtrl;
    // float snapValue = 0.5f; 
    // if (m_GizmoOperation == ImGuizmo::ROTATE) snapValue = 45.0f;
    // float snapValues[3] = { snapValue, snapValue, snapValue };

    // // 기즈모 그리기 및 조작
    // ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
    //                      (ImGuizmo::OPERATION)m_GizmoOperation, ImGuizmo::LOCAL, 
    //                      glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

    // if (ImGuizmo::IsUsing()) {
    //     glm::vec3 translation, rotation, scale;
    //     ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), 
    //                                           glm::value_ptr(translation), 
    //                                           glm::value_ptr(rotation), 
    //                                           glm::value_ptr(scale));

    //     // 값 업데이트
    //     m_SelectedEntity.set<Position>(translation);
    //     // ImGuizmo는 Degree로 주는데 엔진이 Radian을 쓴다면 변환 필요 (여기선 엔진이 Radian 쓴다고 가정)
    //     m_SelectedEntity.set<Rotation>(glm::radians(rotation)); 
    //     m_SelectedEntity.set<Scale>(scale);
    // }
}

void EditorLayer::DrawSceneHierarchy() {
    ImGui::Begin("Scene Hierarchy");

    m_World.each([&](flecs::entity e, const TransformMatrix& t) {
        // 이름 표시
        std::string name = e.name().c_str();
        if (name.empty()) name = "Entity " + std::to_string(e.id());
        
        ImGuiTreeNodeFlags flags = ((m_SelectedEntity == e) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
        
        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)e.id(), flags, "%s", name.c_str());
        if (ImGui::IsItemClicked()) {
            m_SelectedEntity = e;
        }

        if (opened) {
            ImGui::TreePop();
        }
    });

    // 빈 공간 클릭 시 선택 해제
    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
        m_SelectedEntity = flecs::entity::null();
    }
    ImGui::End();
}

void EditorLayer::DrawProperties() {
    // ImGui::Begin("Properties");
    // if (m_SelectedEntity.is_alive()) {
    //     ImGui::Text("ID: %d", (int)m_SelectedEntity.id());
    //     ImGui::Separator();

    //     if (m_SelectedEntity.has<Position>()) {
    //         if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    //             // Position
    //             Position* pos = m_SelectedEntity.get_mut<Position*>();
    //             ImGui::DragFloat3("Position", (float*)pos, 0.1f);

    //             // Rotation (Radian <-> Degree 변환)
    //             Rotation* rot = m_SelectedEntity.get_mut<Rotation*>();
    //             glm::vec3 rotDeg = glm::degrees(glm::vec3(*rot));
    //             if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotDeg), 0.1f)) {
    //                 *rot = glm::radians(rotDeg);
    //             }

    //             // Scale
    //             Scale* scale = m_SelectedEntity.get_mut<Scale*>();
    //             ImGui::DragFloat3("Scale", (float*)scale, 0.1f);
    //         }
    //     }

    //     // Camera Component
    //     if (m_SelectedEntity.has<Camera>()) {
    //         if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    //             Camera* cam = m_SelectedEntity.get_mut<Camera*>();
    //             ImGui::DragFloat("FOV", &cam->fov, 0.1f, 1.0f, 179.0f);
    //             ImGui::DragFloat("Near", &cam->nearPlane);
    //             ImGui::DragFloat("Far", &cam->farPlane);
    //             ImGui::Checkbox("Fisheye Mode", &cam->useFisheye);
    //         }
    //     }

    //     // 컴포넌트 추가 버튼 (예시)
    //     if (ImGui::Button("Add Component")) ImGui::OpenPopup("AddComponent");

    //     if (ImGui::BeginPopup("AddComponent")) {
    //         if (ImGui::MenuItem("Camera")) {
    //             m_SelectedEntity.set<Camera>({60.0f, 0.1f, 1000.0f});
    //             ImGui::CloseCurrentPopup();
    //         }
    //         ImGui::EndPopup();
    //     }

    // } else {
    //     ImGui::TextDisabled("Select an entity.");
    // }
    // ImGui::End();
}

void EditorLayer::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) SaveScene();
            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) LoadScene();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
             if (ImGui::MenuItem("Translate", "W")) m_GizmoOperation = ImGuizmo::TRANSLATE;
             if (ImGui::MenuItem("Rotate", "E")) m_GizmoOperation = ImGuizmo::ROTATE;
             if (ImGui::MenuItem("Scale", "R")) m_GizmoOperation = ImGuizmo::SCALE;
             ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void EditorLayer::HandleShortcuts() {
    if (m_ViewportFocused) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = ImGuizmo::SCALE;
    }

    bool ctrl = ImGui::GetIO().KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) SaveScene();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) LoadScene();
}

void EditorLayer::NewScene() {

}

void EditorLayer::SaveScene() {

}

void EditorLayer::LoadScene() {

}