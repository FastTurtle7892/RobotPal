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

// 4채널 정수 출력 (R: Low, G: High, B: 0, A: 0)
layout(location = 0) out ivec4 o_EntityID;

// 64비트 ID를 쪼개서 받음
uniform int u_EntityID_Low;
uniform int u_EntityID_High;

void main() 
{
    o_EntityID = ivec4(u_EntityID_Low, u_EntityID_High, 0, 0);
}