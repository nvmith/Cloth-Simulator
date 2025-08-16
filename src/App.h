#pragma once
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Cloth.h"
#include "Camera.h"
#include "Shader.h"

class App
{
public:
    App(int width, int height);
    bool init();
    void run();

private:
    // 윈도우
    GLFWwindow* window = nullptr;
    int winWidth;
    int winHeight;

    // 시뮬레이션
    Cloth  cloth;
    Camera camera;
    Shader* clothShader;

    // 드래그 상태
    bool dragging = false;
    int dragAnchor = -1;

    enum class DragMode { None, Particle, Corner };
    DragMode dragMode = DragMode::None;
    int  dragCorner = -1;          // 좌/우 상단 고정점 인덱스 중 선택된 것
    float cornerHitScale = 0.75f;  // 코너 히트 반경 = spacing * 0.75

    // 내부 동작
    void processInput(float dt);
    glm::vec3 screenToWorldOnPlane(double sx, double sy, float planeZ);

    unsigned int clothTex = 0;
    float modelAngle = 0.0f;

    // 플래시
    Shader* flashShader = nullptr;
    unsigned int flashVAO = 0;
    float flash = 0.0f;

    // 캡처 타이머
    float freezeTimer = 0.0f;
    float captureHoldSec = 1.5f;

    float texScale = 2.0f;
    std::string currentTexPath = "textures/blue_camo_pattern.png";
};
