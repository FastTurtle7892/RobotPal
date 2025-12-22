#include "RobotPal/Core/RenderCommand.h"
#include <glad/gles2.h>

void RenderCommand::Init()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    glViewport(x, y, width, height);
}

void RenderCommand::SetClearColor(const glm::vec4 &color)
{
    glClearColor(color.r, color.g, color.b, color.a);
}

void RenderCommand::Clear()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderCommand::SetScissor(int x, int y, int width, int height) 
{
    glScissor(x, y, width, height);
}

void RenderCommand::SetScissorTest(bool enable) 
{
    if (enable) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
}

void RenderCommand::ClearInteger(int value) 
{
    int clearVal = value;
    glClearBufferiv(GL_COLOR, 0, &clearVal);
    
    glClear(GL_DEPTH_BUFFER_BIT);
}

uint64_t RenderCommand::ReadPixelID(int x, int y) 
{
    int pixelData[4] = { -1, 0, 0, 0 };
    
    // RGBA_INTEGER 포맷으로 읽기 (PC fix 적용됨)
    glReadPixels(x, y, 1, 1, GL_RGBA_INTEGER, GL_INT, pixelData);

    // 배경(-1)인 경우 체크 (Low가 -1이면 선택 안됨)
    if (pixelData[0] == -1) return 0; // Flecs에서 0은 null entity

    // [수정] R(Low) + G(High) 채널을 합쳐서 64비트 ID 복원
    uint32_t low = (uint32_t)pixelData[0];
    uint32_t high = (uint32_t)pixelData[1];
    
    uint64_t recoveredID = ((uint64_t)high << 32) | low;
    return recoveredID;
}

void RenderCommand::DrawIndexed(const std::shared_ptr<VertexArray> &vertexArray)
{
    vertexArray->Bind();
    uint32_t count = vertexArray->GetIndexBuffer()->GetCount();
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}

void RenderCommand::DrawArrays(const std::shared_ptr<VertexArray> &vertexArray, uint32_t vertexCount)
{
    vertexArray->Bind();
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}