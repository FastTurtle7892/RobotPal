#include "RobotPal/Util/FileDialog.h"
#include <iostream>

// --- Platform Specific Includes ---
#ifdef __EMSCRIPTEN__
    #include "RobotPal/Util/emscripten_browser_file.h"
#else
    #include "ImGuiFileDialog.h"
    #include <fstream>
    #include <sstream>
#endif

// 싱글톤 인스턴스 반환
FileDialog& FileDialog::Instance() {
    static FileDialog instance;
    return instance;
}

FileDialog::FileDialog() {}
FileDialog::~FileDialog() {}

void FileDialog::init(float scale_factor) {
    m_scaleFactor = scale_factor;
    // 필요한 초기화 로직이 있다면 여기에 작성
}

void FileDialog::unit() {
    m_callback = nullptr;
    // 리소스 정리 로직
}

void FileDialog::manageGPU() {
    // 썸네일 텍스처 관리 등 GPU 작업이 필요하면 여기에 작성
}

void FileDialog::Open(const std::string& key, const std::string& title, const std::string& filters, OnFileLoaded callback) {
    m_callback = callback;
    m_mode = Mode::Open;       // 모드 변경
#ifdef __EMSCRIPTEN__
    // Web: 비동기 업로드 요청
    // filters 예시: ".png,.jpg" (브라우저 input accept 속성)
    emscripten_browser_file::upload(filters, FileDialog::WebUploadCallback, this);
#else
    // Desktop: ImGuiFileDialog 열기
    IGFD::FileDialogConfig config;
    config.path = ".";
    // ImGuiFileDialog의 인스턴스를 사용
    ImGuiFileDialog::Instance()->OpenDialog(key, title, filters.c_str(), config);
    m_currentKey = key;
#endif
}

void FileDialog::Save(const std::string &key, const std::string &title, const std::string &filters, const std::string &defaultName, const std::string &content)
{
#ifdef __EMSCRIPTEN__
    // Web: 다이얼로그 없이 바로 다운로드 트리거
    emscripten_browser_file::download(defaultName, "text/plain", content);
#else
    // Desktop: 저장 다이얼로그 열기
    m_contentToSave = content; // 저장할 내용을 멤버 변수에 임시 저장
    m_mode = Mode::Save;       // 모드 변경
    
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.filePathName = defaultName; // 기본 파일명 설정
    
    ImGuiFileDialog::Instance()->OpenDialog(key, title, filters.c_str(), config);
    m_currentKey = key;
#endif
}

void FileDialog::display(const ImVec4& viewportRect) {
#ifdef __EMSCRIPTEN__
    // --- Web Implementation ---
    // 비동기 콜백으로 데이터가 준비되었는지 확인
    if (m_webFileReady && m_callback) {
        m_callback(m_webFileData);
        
        // 상태 초기화
        m_webFileReady = false;
        m_callback = nullptr; 
        
        // Web 데이터 내용 비우기 (메모리 절약)
        m_webFileData.content.clear();
    }
#else
    // --- Desktop Implementation ---
    // [수정 핵심] 다이얼로그가 처음 열릴 때의 기본 크기와 위치를 지정합니다.
    ImVec2 maxSize = ImVec2(viewportRect.z * 0.8f, viewportRect.w * 0.8f);  // 전체 화면의 80%
    ImVec2 minSize = ImVec2(maxSize.x * 0.5f, maxSize.y * 0.5f);            // 그 절반

    // 1. 크기 설정 (FirstUseEver: 처음 켜질 때만 적용, 이후엔 사용자가 조절한 크기 기억)
    ImGui::SetNextWindowSize(minSize, ImGuiCond_FirstUseEver);

    // 2. (선택사항) 화면 중앙에 띄우기? 필요하다면 주석 해제
    ImVec2 center = ImVec2(viewportRect.x + viewportRect.z * 0.5f, viewportRect.y + viewportRect.w * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    // 다이얼로그 표시 및 로직
    if (ImGuiFileDialog::Instance()->Display(m_currentKey.c_str())) {
        
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            
            if (m_mode == Mode::Open) {
                // [Open 모드] 기존 로직: 파일을 읽어서 콜백 호출
                if (m_callback) {
                    FileData data;
                    data.filePath = filePath;
                    data.fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
                    
                    std::ifstream file(filePath);
                    if (file) {
                        std::stringstream buffer;
                        buffer << file.rdbuf();
                        data.content = buffer.str();
                    }
                    m_callback(data);
                }
            } 
            else if (m_mode == Mode::Save) {
                // [Save 모드] 추가 로직: m_contentToSave를 파일에 쓰기
                std::ofstream file(filePath);
                if (file.is_open()) {
                    file << m_contentToSave;
                    file.close();
                    std::cout << "[FileDialog] Saved to " << filePath << std::endl;
                } else {
                    std::cerr << "[FileDialog] Failed to save file." << std::endl;
                }
                m_contentToSave.clear(); // 내용 비우기
            }
        }
        
        ImGuiFileDialog::Instance()->Close();
        m_callback = nullptr;
    }
#endif
}

// --- Web Specific Static Callback Implementation ---
#ifdef __EMSCRIPTEN__
void FileDialog::WebUploadCallback(std::string const &filename, std::string const &mime_type, std::string_view buffer, void *callback_data) {
    auto* self = static_cast<FileDialog*>(callback_data);
    if (self) {
        self->m_webFileData.fileName = filename;
        self->m_webFileData.fileExtension = mime_type; 
        self->m_webFileData.content = std::string(buffer);
        self->m_webFileData.filePath = ""; // Web은 실제 경로 없음
        
        self->m_webFileReady = true; // 플래그 설정 -> display()에서 처리됨
    }
}
#endif