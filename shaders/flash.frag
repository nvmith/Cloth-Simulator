#version 330 core
out vec4 FragColor;
uniform float uFlash; // 0..1
void main() {
    FragColor = vec4(vec3(1.0), clamp(uFlash, 0.0, 1.0));
}