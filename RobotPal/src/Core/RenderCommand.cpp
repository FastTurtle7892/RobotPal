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

int RenderCommand::ReadPixelInteger(int x, int y) 
{
    // [수정] 4채널 데이터를 받기 위해 배열 사용 (R, G, B, A)
    int pixelData[4] = { -1, 0, 0, 0 };
    
    // [수정] GL_RGBA_INTEGER 포맷으로 읽기
    glReadPixels(x, y, 1, 1, GL_RGBA_INTEGER, GL_INT, pixelData);
    
    // 첫 번째 값(R 채널)이 Entity ID임
    return pixelData[0];
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