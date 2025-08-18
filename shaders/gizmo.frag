#version 330 core
out vec4 FragColor;
uniform vec3 uColor;

void main() {
    // gl_PointCoord(0~1)로 원형 마스크
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    if (dot(p, p) > 1.0) discard;
    FragColor = vec4(uColor, 1.0);
}