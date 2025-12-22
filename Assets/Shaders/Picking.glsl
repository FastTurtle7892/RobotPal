#type vertex
#version 300 es
precision highp float;
precision highp int;

layout(location = 0) in vec3 a_Position;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

void main() 
{
    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 300 es
precision highp float;
precision highp int;

layout(location = 0) out ivec4 o_EntityID;

uniform int u_EntityID;
void main() 
{
    // R 채널에만 ID를 쓰고 나머지는 0으로 채움
    o_EntityID = ivec4(u_EntityID, 0, 0, 0);
}