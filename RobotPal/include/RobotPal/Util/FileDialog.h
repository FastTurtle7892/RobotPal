#pragma once

#include "imgui.h" // ImVec4 등을 위해 필요
#include <string>
#include <functional>
#include <vector>

// 파일 데이터 구조체
struct FileData {
    std::string fileName;
    std::string fileExtension;
    std::string content; // 파일 내용 (Desktop/Web 공통)
    std::string filePath; // Desktop 전용 (Web은 비어있음)
};

class FileDialog {
public:
    // 콜백 타입 정의
    using OnFileLoaded = std::function<void(const FileData&)>;

    // 싱글톤 접근
    static FileDialog& Instance();

    // 생성자/소멸자
    FileDialog();
    ~FileDialog();

    // --- Life Cycle Methods ---
    void init(float scale_factor = 1.0f);
    void unit();
    void manageGPU();
    
    // --- Main Logic ---
    // 파일 열기 요청
    void Open(const std::string& key, const std::string& title, const std::string& filters, OnFileLoaded callback);

    // 화면 렌더링 및 업데이트
    void display(const ImVec4& viewportRect);

private:
    OnFileLoaded m_callback = nullptr;
    float m_scaleFactor = 1.0f;
    std::string m_currentKey;

    // --- Web Specific State ---
    // Web 환경 변수들은 컴파일 에러 방지를 위해 ifdef를 쓰지 않고
    // 일반 변수로 두거나, pImpl 패턴을 쓰지만 여기선 간단히 둡니다.
#ifdef __EMSCRIPTEN__
    bool m_webFileReady = false;
    FileData m_webFileData;
    
    // Emscripten용 정적 콜백
    static void WebUploadCallback(std::string const &filename, std::string const &mime_type, std::string_view buffer, void *callback_data);
#endif
};