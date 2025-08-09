#version 330 core
out vec4 FragColor;

void main()
{
    // 일단 단색으로 그려서 '보이는지'부터 확인
    FragColor = vec4(0.85, 0.85, 0.9, 1.0);
}