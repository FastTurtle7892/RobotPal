#include "RobotPal/EditorLayer.h"
#include "RobotPal/Components/Components.h"
#include "RobotPal/Util/SceneSerializer.h"
#include "RobotPal/Util/FileDialog.h"
#include "RobotPal/SandboxScene.h"
#include "RobotPal/SceneManager.h"
#include <imgui.h>
#include <imgui_internal.h> // DockBuilder
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glad/gles2.h> // glClearColor 등

#include "RobotPal/Systems/RenderSystemModule.h"


static glm::mat4 CreateViewMatrixFromWorld(const glm::mat4 &worldMatrix)
{
    // 1. 위치(Position) 추출 (4열)
    glm::vec3 pos = glm::vec3(worldMatrix * glm::vec4(0.f, 0.f, 0.f, 1.f));

    // 2. Forward(앞) 벡터 추출 및 정규화
    // OpenGL 메모리 레이아웃상 3열(인덱스 2)은 로컬 Z축(Backward)입니다.
    // 카메라는 -Z를 보므로, 이를 반전시켜 Forward를 구합니다.
    glm::vec3 backward = glm::vec3(worldMatrix[2]);
    glm::vec3 forward = glm::normalize(-backward); // 스케일 제거됨

    // 3. Right(오른쪽) 벡터 재구축 (스케일/쉐어링 제거의 핵심)
    // 월드 행렬의 X축(0열)을 쓰지 않고 외적으로 새로 구합니다.
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right;

    // 짐벌락(Gimbal Lock) 예외 처리: 카메라가 수직으로 위/아래를 볼 때
    if (glm::abs(glm::dot(forward, worldUp)) > 0.999f)
    {
        // 위를 보고 있으면 Right를 임의의 축(예: X축)으로 설정
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        right = glm::normalize(glm::cross(forward, worldUp));
    }

    // 4. Up(위) 벡터 재구축
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    // 5. View Matrix 직접 조립 (R_transposed * T_inverse)
    // GLM은 Column-Major이므로 [Col][Row] 순서 혹은 생성자에 열 순서로 대입
    glm::mat4 view(1.0f);

    // -- 회전 파트 (Transposed Rotation) --
    // Right Vector (1행)
    view[0][0] = right.x;
    view[1][0] = right.y;
    view[2][0] = right.z;

    // Up Vector (2행)
    view[0][1] = up.x;
    view[1][1] = up.y;
    view[2][1] = up.z;

    // Backward Vector (3행) - OpenGL 뷰 공간은 Z가 뒤쪽을 향함
    // forward의 반대인 backward(-forward)가 필요하지만,
    // 위에서 구한 forward 벡터를 기준으로 생각하면: -forward
    glm::vec3 viewZ = -forward;
    view[0][2] = viewZ.x;
    view[1][2] = viewZ.y;
    view[2][2] = viewZ.z;

    // -- 이동 파트 (Translation) --
    // 공식: -dot(Axis, Position)
    view[3][0] = -glm::dot(right, pos);
    view[3][1] = -glm::dot(up, pos);
    view[3][2] = -glm::dot(viewZ, pos);

    // 마지막 3,3은 1.0f (초기화시 설정됨)

    return view;
}

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
    DrawRobotView();
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
            ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.3f, nullptr, &dock_main_id);
            ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
            ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_right_down_id = ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Down, 0.4f, nullptr, &dock_right_id);

            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left_id);
            ImGui::DockBuilderDockWindow("Properties", dock_right_id);
            ImGui::DockBuilderDockWindow("Content Browser", dock_bottom_id);
            
            ImGui::DockBuilderDockWindow("Robot View", dock_right_down_id);
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

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_ViewportHovered && !ImGuizmo::IsOver()) 
    {
        // 1. 카메라 행렬 구하기 (기존 코드 활용)
        const Camera* cam = cameraEnt.try_get_mut<Camera>();
        const TransformMatrix* camTrans = cameraEnt.try_get_mut<TransformMatrix, World>();
        
        glm::mat4 view = CreateViewMatrixFromWorld(*camTrans);
        float aspect = m_ViewportSize.x / m_ViewportSize.y;
        glm::mat4 proj = glm::perspective(glm::radians(cam->fov), aspect, cam->nearPlane, cam->farPlane);

        // 2. 좌표 변환 (Y축 반전 및 -1 보정)
        auto [mx, my] = ImGui::GetMousePos();
        mx -= m_ViewportBounds[0].x;
        my -= m_ViewportBounds[0].y;
        
        int mouseX = (int)mx;
        int mouseY = (int)(m_ViewportSize.y - my) - 1; // ★ -1 중요

        // 3. 피킹 실행
        if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)m_ViewportSize.x && mouseY < (int)m_ViewportSize.y) 
        {
            auto* renderSystem = m_World.try_get_mut<RenderSystemModule>();
            if (renderSystem) {
                // A. 클릭된 엔티티 가져오기
                flecs::entity clickedEntity = renderSystem->PickEntity(
                    m_World, mouseX, mouseY, 
                    (int)m_ViewportSize.x, (int)m_ViewportSize.y, 
                    view, proj
                );

                // B. [스마트 선택 로직] 부모 찾기
                if (clickedEntity.is_alive()) {
                    flecs::entity rootEntity = clickedEntity;
                    while (true) {
                        flecs::entity parent = rootEntity.parent();
                        if (!parent.is_alive() || parent.name() == "SceneRoot") break;
                        rootEntity = parent;
                    }

                    // 이미 루트가 선택됐다면 자식 선택, 아니면 루트 선택
                    if (m_SelectedEntity == rootEntity) {
                        m_SelectedEntity = clickedEntity; 
                        std::cout << "Selected Child: " << m_SelectedEntity.name().c_str() << std::endl;
                    } else {
                        m_SelectedEntity = rootEntity;
                        std::cout << "Selected Root: " << m_SelectedEntity.name().c_str() << std::endl;
                    }
                } else {
                    m_SelectedEntity = flecs::entity::null(); // 빈공간 -> 선택 해제
                }
            }
        }
    }

        UpdateCameraMovement(cameraEnt);
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

void EditorLayer::DrawRobotView() {
    ImGui::Begin("Robot View");

    flecs::entity robotCam = flecs::entity::null();

    m_World.query<RenderTarget>()
    .each([&](flecs::entity e, const RenderTarget& rt) {
        if (e.name() == "RobotCamera") {
            robotCam = e;
        }
    });

    if(robotCam==flecs::entity::null() || !robotCam.is_alive()|| !robotCam.has<Camera>())
    {
        ImGui::End();
        return;
    }

    RenderTarget* rt = robotCam.try_get_mut<RenderTarget>();

    if (rt && rt->fbo) {
        ImVec2 panelSize = ImGui::GetContentRegionAvail();
        uint64_t texID = rt->fbo->GetColorAttachment()->GetID();

        if (panelSize.x > 0 && panelSize.y > 0) {

            ImGui::Image((void*)(intptr_t)texID, panelSize, ImVec2(0, 1), ImVec2(1, 0));
        }
    } 
    else {
        const char* text = "No Robot Camera Signal";
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 textSize = ImGui::CalcTextSize(text);

        ImGui::SetCursorPos(ImVec2((windowSize.x - textSize.x) * 0.5f, (windowSize.y - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", text);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Entity 'RobotCamera' with 'RenderTarget' component is required.");
        }
    }

    ImGui::End();
}


void EditorLayer::UpdateCameraMovement(flecs::entity cameraEnt) {
    // 1. 뷰포트가 포커스되거나 호버링 상태일 때만 작동
    if (!m_ViewportHovered && !m_ViewportFocused) return;
    if (cameraEnt == flecs::entity::null() || !cameraEnt.is_alive()) return;

    // 2. 카메라 컴포넌트 가져오기 (Local 좌표 기준 수정)
    auto* pos = cameraEnt.try_get_mut<Position, Local>();
    auto* rot = cameraEnt.try_get_mut<Rotation, Local>();
    auto* matrix = cameraEnt.try_get<TransformMatrix, World>();

    if (!pos || !rot || !matrix) return;

    ImGuiIO& io = ImGui::GetIO();
    
    // 3. 마우스 델타값 (이번 프레임 이동량)
    float deltaX = io.MouseDelta.x;
    float deltaY = io.MouseDelta.y;

    // --- 카메라 축 벡터 추출 (World Matrix의 열벡터) ---
    // Col 0: Right, Col 1: Up, Col 2: Backward (Forward = -Col2)
    glm::mat4 view = *matrix; 
    glm::vec3 right = glm::normalize(glm::vec3(view[0])); 
    glm::vec3 up    = glm::normalize(glm::vec3(view[1])); 
    glm::vec3 forward = -glm::normalize(glm::vec3(view[2])); 

    // -----------------------------------------------------------
    // [모드 1] Fly Mode: 마우스 오른쪽 버튼(RMB) 홀드 시 (Unity 스타일)
    // -----------------------------------------------------------
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        // 1. 시점 회전 (Look)
        float sensitivity = 0.003f;
        rot->y -= io.MouseDelta.x * sensitivity; // Yaw
        rot->x -= io.MouseDelta.y * sensitivity; // Pitch
        rot->x = glm::clamp(rot->x, -1.57f, 1.57f); // 90도 제한

        // 2. WASD 이동 (Move)
        float speed = 5.0f * m_World.delta_time(); // 기본 속도
        if (io.KeyShift) speed *= 3.0f; // Shift 누르면 부스트 (빠른 이동)

        if (ImGui::IsKeyDown(ImGuiKey_W)) *pos += forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) *pos -= forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) *pos -= right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) *pos += right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) *pos -= glm::vec3(0,1,0) * speed; // 하강 (World Up)
        if (ImGui::IsKeyDown(ImGuiKey_E)) *pos += glm::vec3(0,1,0) * speed; // 상승 (World Up)

        // *중요* 우클릭 중에는 ImGuizmo나 다른 UI와 상호작용 안 하도록 처리 필요할 수 있음
    }
    // -----------------------------------------------------------
    // [모드 2] Pan & Zoom: 평상시 (우클릭 안 누를 때)
    // -----------------------------------------------------------
    else {
        // Pan (휠 클릭 드래그)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
             float panSpeed = 0.008f; // 거리 비례 계산하면 더 좋음
             *pos += (-right * io.MouseDelta.x * panSpeed) + (up * io.MouseDelta.y * panSpeed);
        }

        // Zoom (휠 스크롤)
        if (io.MouseWheel != 0.0f) {
            float zoomSpeed = 1.0f;
            *pos += forward * io.MouseWheel * zoomSpeed;
        }
    }
}

void EditorLayer::DrawGizmo(flecs::entity cameraEnt) {
    // 1. 유효성 검사
    if (m_SelectedEntity == flecs::entity::null() || !m_SelectedEntity.is_alive() || m_GizmoOperation == -1) return;

    // 2. 카메라 데이터 가져오기
    auto* cam = cameraEnt.try_get<Camera>();
    auto* camTrans = cameraEnt.try_get<TransformMatrix, World>();
    if (!cam || !camTrans) return;

    // View Matrix: 카메라 월드 행렬의 역행렬
    glm::mat4 view = CreateViewMatrixFromWorld(*camTrans);
    
    // Projection Matrix
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 projection = glm::perspective(glm::radians(cam->fov), aspect, cam->nearPlane, cam->farPlane);

    // 3. [핵심] 기즈모에 사용할 'World 행렬' 가져오기
    // 기즈모는 화면상에서 객체의 '실제 위치'에 붙어야 하므로 World 행렬을 사용해야 합니다.
    auto* worldComp = m_SelectedEntity.try_get<TransformMatrix, World>();
    
    // 만약 World 행렬이 아직 계산 안 됐다면 Local이라도 가져옴 (Fallback)
    glm::mat4 currentTransform;
    if (worldComp) {
        currentTransform = *worldComp;
    } else {
        auto* localComp = m_SelectedEntity.try_get<TransformMatrix, Local>();
        if (!localComp) return; // 위치 정보가 아예 없으면 리턴
        currentTransform = *localComp;
    }

    // 4. ImGuizmo 설정
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, 
                      m_ViewportBounds[1].x - m_ViewportBounds[0].x, 
                      m_ViewportBounds[1].y - m_ViewportBounds[0].y);

    // Snapping
    bool snap = ImGui::GetIO().KeyCtrl;
    float snapValue = 0.5f; 
    if (m_GizmoOperation == ImGuizmo::ROTATE) snapValue = 45.0f;
    float snapValues[3] = { snapValue, snapValue, snapValue };

    // 5. 기즈모 조작 (World Matrix 기준)
    // 여기서 사용자가 조작하면 currentTransform(World) 값이 변합니다.
    ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                         (ImGuizmo::OPERATION)m_GizmoOperation, ImGuizmo::LOCAL, 
                         glm::value_ptr(currentTransform), nullptr, snap ? snapValues : nullptr);

    // 6. 변경된 World 값을 Local로 변환하여 저장
    if (ImGuizmo::IsUsing()) {
        glm::mat4 localMatrix = currentTransform;

        // [부모 처리] 자식 엔티티라면: Local = inv(ParentWorld) * NewWorld
        flecs::entity parent = m_SelectedEntity.parent();
        if (parent.is_alive()) {
            auto* parentWorld = parent.try_get<TransformMatrix, World>();
            if (parentWorld) {
                localMatrix = glm::inverse((glm::mat4)*parentWorld) * currentTransform;
            }
        }

        // [GLM 분해] 행렬에서 Position, Rotation, Scale 추출
        // ImGuizmo::Decompose 대신 glm::decompose를 써야 회전이 안정적입니다.
        glm::vec3 translation, scale, skew;
        glm::quat rotation;
        glm::vec4 perspective;
        glm::decompose(localMatrix, scale, rotation, translation, skew, perspective);

        // 값 업데이트 (Local 태그 사용)
        m_SelectedEntity.set<Position, Local>({translation});
        
        // Quaternion -> Euler Angles (Radian) 변환
        m_SelectedEntity.set<Rotation, Local>({glm::eulerAngles(rotation)}); 
        
        m_SelectedEntity.set<Scale, Local>({scale});
    }
}

void EditorLayer::DrawSceneHierarchy() {
    ImGui::Begin("Scene Hierarchy");

    // 1. SceneRoot 엔티티 찾기
    flecs::entity root = m_World.lookup("SceneRoot");
    
    if (root.is_alive()) {
        // 2. SceneRoot의 바로 아래 자식들부터 그리기 시작
        // (SceneRoot 자체는 목록에 표시하지 않고, 그 내용물부터 보여줍니다)
        root.children([this](flecs::entity child) {
            DrawEntityNode(child);
        });
    } 
    else {
        // 혹시 SceneRoot가 아직 안 만들어졌을 경우 대비
        ImGui::TextDisabled("SceneRoot not found");
    }

    // 빈 공간 클릭 시 선택 해제
    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
        m_SelectedEntity = flecs::entity::null();
    }

    ImGui::End();
}

void EditorLayer::DrawEntityNode(flecs::entity e) {
    // 이름 가져오기
    std::string name = e.name().c_str();
    if (name.empty()) name = "Entity " + std::to_string(e.id());

    // 트리 노드 플래그 설정
    ImGuiTreeNodeFlags flags = ((m_SelectedEntity == e) ? ImGuiTreeNodeFlags_Selected : 0);
    flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    // 자식이 없으면 Leaf 노드(화살표 없음), 있으면 OpenOnArrow
    // (flecs에서 자식 유무를 미리 알기 어려우므로 일단 그립니다. 
    //  최적화를 원하면 e.child_count() 같은 API가 있는지 확인 필요하지만, 보통은 그냥 그려도 무방합니다.)
    
    bool opened = ImGui::TreeNodeEx((void*)(uint64_t)e.id(), flags, "%s", name.c_str());
    
    // 클릭 시 선택
    if (ImGui::IsItemClicked()) {
        m_SelectedEntity = e;
    }

    // 노드가 열렸다면 자식들을 재귀적으로 그림
    if (opened) {
        e.children([this](flecs::entity child) {
            DrawEntityNode(child);
        });
        ImGui::TreePop();
    }
}

void EditorLayer::DrawProperties() {
    ImGui::Begin("Properties");
    if (m_SelectedEntity.is_alive()) {
        ImGui::Text("ID: %d", (int)m_SelectedEntity.id());
        ImGui::Separator();

        if (m_SelectedEntity.has<TransformMatrix, Local>()) {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Position
                Position* pos = m_SelectedEntity.try_get_mut<Position, Local>();
                ImGui::DragFloat3("Position", (float*)pos, 0.1f);

                // Rotation (Radian <-> Degree 변환)
                Rotation* rot = m_SelectedEntity.try_get_mut<Rotation, Local>();
                glm::vec3 rotDeg = glm::degrees(glm::vec3(*rot));
                if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotDeg), 0.1f)) {
                    *rot = glm::radians(rotDeg);
                }

                // Scale
                Scale* scale = m_SelectedEntity.try_get_mut<Scale, Local>();
                ImGui::DragFloat3("Scale", (float*)scale, 0.1f);
            }
        }

        // Camera Component
        if (m_SelectedEntity.has<Camera>()) {
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                Camera* cam = m_SelectedEntity.try_get_mut<Camera>();
                ImGui::DragFloat("FOV", &cam->fov, 0.1f, 1.0f, 179.0f);
                ImGui::DragFloat("Near", &cam->nearPlane);
                ImGui::DragFloat("Far", &cam->farPlane);
                ImGui::Checkbox("Fisheye Mode", &cam->useFisheye);
            }
        }

        // // 컴포넌트 추가 버튼 (예시)
        // if (ImGui::Button("Add Component")) ImGui::OpenPopup("AddComponent");

        // if (ImGui::BeginPopup("AddComponent")) {
        //     if (ImGui::MenuItem("Camera")) {
        //         m_SelectedEntity.set<Camera>({60.0f, 0.1f, 1000.0f});
        //         ImGui::CloseCurrentPopup();
        //     }
        //     ImGui::EndPopup();
        // }

    } else {
        ImGui::TextDisabled("Select an entity.");
    }
    ImGui::End();
}

void EditorLayer::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) SaveScene();
            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) LoadScene();
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Select / None", "Q")) m_GizmoOperation = -1;
            if (ImGui::MenuItem("Translate", "W")) m_GizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::MenuItem("Rotate", "E")) m_GizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::MenuItem("Scale", "R")) m_GizmoOperation = ImGuizmo::SCALE;
            ImGui::EndMenu();
        }

        ImGui::Button("Reset Scene");
        if(ImGui::IsItemClicked())
        {
           m_World.get_mut<const SceneManagerHandle>().instance->LoadScene<SandboxScene>();
        }
        ImGui::EndMenuBar();

    }
}

void EditorLayer::HandleShortcuts() {
    // 우클릭(카메라 이동) 중일 때는 단축키 처리를 하지 않음
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) return;
    
    // 만약 텍스트 입력창(Inspector 등)을 쓰고 있다면 단축키 무시
    if (ImGui::GetIO().WantTextInput) return;
    
    //if (m_ViewportFocused) 
    {
        // [Q] 선택 모드 (기즈모 끄기)
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
            m_GizmoOperation = -1; 
        }
        
        // [W] 이동 (Translate)
        if (ImGui::IsKeyPressed(ImGuiKey_W)) {
            m_GizmoOperation = ImGuizmo::TRANSLATE;
        }

        // [E] 회전 (Rotate)
        if (ImGui::IsKeyPressed(ImGuiKey_E)) {
            m_GizmoOperation = ImGuizmo::ROTATE;
        }

        // [R] 크기 (Scale)
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            m_GizmoOperation = ImGuizmo::SCALE;
        }
    }

    bool ctrl = ImGui::GetIO().KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) SaveScene();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) LoadScene();
}

void EditorLayer::NewScene() {

}

void EditorLayer::SaveScene() {
    // 1. 포매팅 없는 Raw String 가져오기
     RobotPal::SceneSerializer sceneSerializer(m_World);
    std::string rawSceneData = sceneSerializer.SerializeToString();
        
    // 2. 저장 요청
    FileDialog::Instance().Save(
        "SceneSaveKey", 
        "Save Scene", 
        ".robotpal",           // [변경] 확장자 필터
        "MyScene.robotpal",    // [변경] 기본 파일명
        rawSceneData           // 내용 전달
    );
}

void EditorLayer::LoadScene() {
    FileDialog::Instance().Open(
            "SceneLoadKey", 
            "Load Scene", 
            ".robotpal",  // [변경] 확장자 필터
        [&](const FileData& data) {
            // 원본 문자열 그대로 로드
            RobotPal::SceneSerializer sceneSerializer(m_World);
            sceneSerializer.DeserializeFromString(data.content);
            std::cout << "Scene Loaded: " << data.fileName << std::endl;
        }
    );
}