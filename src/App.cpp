#include "App.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static App* g_app = nullptr;

App::App(int width, int height)
    : winWidth(width), winHeight(height),
    cloth(20, 20, 0.2f),
    camera(glm::vec3(0.0f, 0.0f, 8.0f)),
    clothShader(nullptr)
{
    g_app = this;
}

static unsigned int LoadTexture2D(const char* path, bool srgb = true)
{
    int w, h, comp;
    stbi_uc* data = stbi_load(path, &w, &h, &comp, 4);
    if (!data) { std::cout << "Failed to load texture: " << path << "\n"; return 0; }

    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum internalFmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

bool App::init()
{
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(winWidth, winHeight, "Cloth Simulator", nullptr, nullptr);
    if (!window) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to init GLAD\n";
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // 천 양면 보이게 (옵션)
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        glViewport(0, 0, w, h);
        g_app->winWidth = w;
        g_app->winHeight = h;
        });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            double mx, my; glfwGetCursorPos(win, &mx, &my);
            glm::vec3 hit = g_app->screenToWorldOnPlane(mx, my, 0.0f);

            // 코너(좌상단, 우상단) 우선 히트테스트
            const auto& P = g_app->cloth.getParticles();
            int w = g_app->cloth.getWidth();
            int tl = 0;
            int tr = w - 1;

            // spacing 추정: (0,0)~(1,0) 거리. w<2면 안전값 사용
            float baseSpacing = (w >= 2) ? glm::length(P[1].pos - P[0].pos) : 0.25f;
            float r = baseSpacing * g_app->cornerHitScale;

            float d0 = glm::length(P[tl].pos - hit);
            float d1 = glm::length(P[tr].pos - hit);

            if (std::min(d0, d1) <= r)
            {
                g_app->dragMode = App::DragMode::Corner;
                g_app->dragCorner = (d0 <= d1 ? tl : tr);
                g_app->dragAnchor = g_app->dragCorner;  // 편의상 동일 인덱스 기록
                g_app->dragging = true;

                g_app->cloth.setParticlePos(g_app->dragCorner, hit, /*movePrev=*/true);
            }
            else
            {
                // 일반 파티클 드래그 (기존 로직)
                int nearest = -1; float best = 1e9f;
                for (int i = 0; i < (int)P.size(); i++)
                {
                    float d = glm::length(P[i].pos - hit);
                    if (d < best) { best = d; nearest = i; }
                }
                g_app->dragMode = App::DragMode::Particle;
                g_app->dragAnchor = nearest;
                g_app->dragging = (nearest >= 0);
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        {
            // 상태 리셋 (코너/일반 공통)
            g_app->dragging = false;
            g_app->dragAnchor = -1;
            g_app->dragCorner = -1;
            g_app->dragMode = App::DragMode::None;
        }
        });

    clothShader = new Shader("shaders/cloth.vert", "shaders/cloth.frag");
    flashShader = new Shader("shaders/flash.vert", "shaders/flash.frag");
    glGenVertexArrays(1, &flashVAO);


    cloth.buildIndices(cloth.getWidth(), cloth.getHeight());
    cloth.initGL();

    clothTex = LoadTexture2D("textures/blue_camo_pattern.png", true);

    return true;
}

void App::run()
{
    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        if (freezeTimer > 0.0f) freezeTimer = std::max(0.0f, freezeTimer - dt);

        processInput(dt);

        // 드래그
        if (freezeTimer <= 0.0f && dragging)
        {
            double mx, my; glfwGetCursorPos(window, &mx, &my);
            glm::vec3 p = screenToWorldOnPlane(mx, my, 0.0f);

            if (dragMode == DragMode::Corner && dragCorner >= 0)
            {
                cloth.setParticlePos(dragCorner, p, true);
            }
            else if (dragMode == DragMode::Particle && dragAnchor >= 0 && !cloth.isParticleFixed(dragAnchor))
            {
                cloth.setParticlePos(dragAnchor, p, true);
            }
        }

        if (freezeTimer <= 0.0f) 
        {
            cloth.update(dt);
        }
        cloth.updateGPU();

        flash = std::max(0.0f, flash - dt * 6.0f);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        clothShader->use();

        clothShader->setFloat("uFlash", flash);

        if (clothTex != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, clothTex);
            clothShader->setInt("uAlbedo", 0);
            clothShader->setInt("uUseTexture", 1);

            clothShader->setFloat("uTexScale", texScale);
        }
        else {
            clothShader->setInt("uUseTexture", 0);
        }

        // 행렬
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)winWidth / (float)winHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), modelAngle, glm::vec3(0, 1, 0));
        clothShader->setMat4("projection", proj);
        clothShader->setMat4("view", view);
        clothShader->setMat4("model", model);

        // 라이팅
        clothShader->setVec3("uLightDir", glm::normalize(glm::vec3(0.3f, 1.0f, 0.4f)));
        glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);
        clothShader->setVec3("uViewPos", camPos);
        clothShader->setVec3("uAlbedoColor", glm::vec3(0.85f));
        clothShader->setVec3("uAmbient", glm::vec3(0.06f));
        clothShader->setFloat("uSpecularStrength", 0.25f);
        clothShader->setFloat("uShininess", 48.0f);

        // 텍스처
        if (clothTex != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, clothTex);
            clothShader->setInt("uAlbedo", 0);
            clothShader->setInt("uUseTexture", 1);
            clothShader->setFloat("uTexScale", texScale);
        }
        else {
            clothShader->setInt("uUseTexture", 0);
        }

        cloth.drawTriangles();

        float overlay = flash;
        if (freezeTimer > 0.0f) overlay = std::max(overlay, 0.12f);

        if (overlay > 0.0f) {
            glDisable(GL_DEPTH_TEST);
            flashShader->use();
            flashShader->setFloat("uFlash", overlay);
            glBindVertexArray(flashVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete clothShader;
    glfwTerminate();
}

void App::processInput(float dt)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, dt);

    for (int k = GLFW_KEY_1; k <= GLFW_KEY_9; k++) {
        if (glfwGetKey(window, k) == GLFW_PRESS) {
            texScale = float(k - GLFW_KEY_0);
        }
    }

    const float rotSpeed = 1.2f;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) modelAngle -= rotSpeed * dt;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) modelAngle += rotSpeed * dt;

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        cloth.resetToRest();
        modelAngle = 0.0f;
    }

    static int pO = GLFW_RELEASE;
    int cO = glfwGetKey(window, GLFW_KEY_O);
    if (cO == GLFW_PRESS && pO == GLFW_RELEASE) {
        std::filesystem::create_directories("out");
        cloth.exportOBJ("out/cloth.obj", "cloth.mtl",
            (clothTex != 0 ? currentTexPath.c_str() : nullptr),
            texScale);
        g_app->flash = 1.0f;
        g_app->freezeTimer = g_app->captureHoldSec;
        g_app->dragging = false;
        char title[128];
        snprintf(title, sizeof(title), "Cloth Simulator  |  tile=%.2f", texScale);
        glfwSetWindowTitle(window, title);
    }
    pO = cO;
}



glm::vec3 App::screenToWorldOnPlane(double sx, double sy, float planeZ)
{
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)winWidth / winHeight, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 invVP = glm::inverse(proj * view);

    float x = (float)((sx / winWidth) * 2.0 - 1.0);
    float y = (float)(1.0 - (sy / winHeight) * 2.0);

    glm::vec4 p0 = invVP * glm::vec4(x, y, 0.0, 1.0);
    glm::vec4 p1 = invVP * glm::vec4(x, y, 1.0, 1.0);
    p0 /= p0.w; p1 /= p1.w;

    glm::vec3 ro = glm::vec3(p0);
    glm::vec3 rd = glm::normalize(glm::vec3(p1 - p0));

    float denom = rd.z;
    if (fabsf(denom) < 1e-6f) return ro;

    float t = (planeZ - ro.z) / denom;
    return ro + rd * t;
}