#version 330 core
layout (location = 0) in vec3 aPos;     // VBO 0번
layout (location = 1) in vec2 aTex;     // VBO 1번 (있어도/없어도 OK)

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}