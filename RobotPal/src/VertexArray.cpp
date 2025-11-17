#include "RobotPal/VertexArray.h"
#include <glad/gles2.h>

static GLenum ShaderDataTypeToOpenGLBaseType(DataType type)
{
	switch (type)
	{
		case DataType::Bool:	return GL_BOOL;
		case DataType::Int:		return GL_INT;
		case DataType::Int2:	return GL_INT;
		case DataType::Int3:	return GL_INT;
		case DataType::Int4:	return GL_INT;
		case DataType::Float:	return GL_FLOAT;
		case DataType::Float2:	return GL_FLOAT;
		case DataType::Float3:	return GL_FLOAT;
		case DataType::Float4:	return GL_FLOAT;
		case DataType::Mat3:	return GL_FLOAT;
		case DataType::Mat4:	return GL_FLOAT;
	}
	return 0;
}

std::shared_ptr<VertexArray> VertexArray::Create()
{
    return std::make_shared<VertexArray>();
}

VertexArray::VertexArray()
{
}

VertexArray::~VertexArray()
{
}

void VertexArray::Bind() const
{
    uint32_t index = 0;
    for (const auto& vertexBuffer : m_VertexBuffers)
    {
        vertexBuffer->Bind();
        const auto& layout = vertexBuffer->GetLayout();
        for (const auto& element : layout)
        {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(index,
                element.GetComponentCount(),
                ShaderDataTypeToOpenGLBaseType(element.Type),
                element.Normalized ? GL_TRUE : GL_FALSE,
                layout.GetStride(),
                (const void*)(uintptr_t)element.Offset);
            index++;
        }
    }

    if (m_IndexBuffer)
        m_IndexBuffer->Bind();
}

void VertexArray::UnBind() const
{
    uint32_t index = 0;
    for (const auto& vertexBuffer : m_VertexBuffers)
    {
        vertexBuffer->UnBind();
        const auto& layout = vertexBuffer->GetLayout();
        for (size_t i = 0; i < layout.GetElements().size(); i++)
        {
            glDisableVertexAttribArray(index);
            index++;
        }
    }

    if (m_IndexBuffer)
        m_IndexBuffer->UnBind();
}

void VertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
{
    m_VertexBuffers.push_back(vertexBuffer);
}

void VertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
{
    m_IndexBuffer = indexBuffer;
}

const std::vector<std::shared_ptr<VertexBuffer>>& VertexArray::GetVertexBuffers() const
{
    return m_VertexBuffers;
}

const std::shared_ptr<IndexBuffer>& VertexArray::GetIndexBuffer() const
{
    return m_IndexBuffer;
}