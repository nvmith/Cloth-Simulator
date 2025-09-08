#pragma once
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Cloth.h"
#include "Camera.h"
#include "Shader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

class App
{
public:
    App(int width, int height);
    bool init();
    void run();

private:
    unsigned int loadTexture2D(const char* path, bool srgb);

    // 윈도우
    GLFWwindow* window = nullptr;
    int winWidth;
    int winHeight;

    // 시뮬레이션
    Cloth  cloth;
    Camera camera;
    Shader* clothShader = nullptr;

    // 드래그 상태
    bool dragging = false;
    int  dragAnchor = -1;

    enum class DragMode { None, Particle, Corner };
    DragMode dragMode = DragMode::None;
    int      dragCorner = -1;
    float    cornerHitScale = 2.0f;

    // 내부 동작
    void generateAndLoadTextureFromPrompt(const std::string& prompt,
        const std::string& negative);
    void processInput(float dt);
    glm::vec3 screenToWorldOnPlane(double sx, double sy, float planeZ);

    // 렌더/머터리얼
    unsigned int clothTex = 0;
    float   texScale = 2.0f;
    float   modelAngle = 0.0f;
    std::string currentTexPath = "textures/generated.png";

    // 기즈모(고정핀)
    Shader* gizmoShader = nullptr;
    unsigned int gizmoVAO = 0, gizmoVBO = 0;

    // 화면 전체 플래시
    Shader* flashShader = nullptr;
    unsigned int flashVAO = 0;
    float flash = 0.0f;

    // 캡처(미리보기) 타이머
    float freezeTimer = 0.0f;
    float captureHoldSec = 1.5f;

    // 실행 중 재생성(G)용 마지막 프롬프트
    std::string lastPrompt;
    std::string lastNegPrompt;

    // 마지막 이미지 리로드
    void reloadTexture(const std::string& path);

    bool imguiEnabled = true;
    const char* imguiGlslVersion = "#version 330"; // OpenGL 3.3 core

    void runPatternGen(const char* prompt);

    bool rightDownPrev = false;

    bool suppressRightClickWind = false;
};