#version 330 core
layout (location = 0) in vec3 aPos;      // VAO: vboPos
layout (location = 1) in vec2 aUV;       // VAO: vboUV
layout (location = 2) in vec3 aNormal;   // VAO: vboNormal

out vec2 vUV;
out vec3 vNormalW;   // world-space normal
out vec3 vWorldPos;  // world-space position

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 wPos = model * vec4(aPos, 1.0);
    vWorldPos = wPos.xyz;

    // normal matrix
    mat3 normalMat = mat3(transpose(inverse(model)));
    vNormalW = normalize(normalMat * aNormal);

    vUV = aUV;

    gl_Position = projection * view * wPos;
}
