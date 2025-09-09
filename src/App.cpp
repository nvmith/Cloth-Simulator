#include "App.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <filesystem>
#include <algorithm> 
#include <cmath>
#include <cstdio>
#include <string>
#include <cstdlib>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static App* g_app = nullptr;

App::App(int width, int height)
    : winWidth(width), winHeight(height),
    cloth(20, 20, 0.2f),
    camera(glm::vec3(0.0f, 0.0f, 8.0f))
{
    g_app = this;
}

unsigned int App::loadTexture2D(const char* path, bool srgb)
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
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to init GLAD\n";
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // 천 양면
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        glViewport(0, 0, w, h);
        g_app->winWidth = w;
        g_app->winHeight = h;
        });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int) {
        if (ImGui::GetIO().WantCaptureMouse) return;
        if (g_app->freezeTimer > 0.0f) return;

        auto& cloth = g_app->cloth;
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(win, &mx, &my);
        glm::vec3 hit = g_app->screenToWorldOnPlane(mx, my, 0.0f);

        const auto& P = cloth.getParticles();
        int W = cloth.getWidth();
        int H = cloth.getHeight();
        if (W < 1 || H < 1) return;

        // 코너 4개 인덱스 정의 (중복 코드 제거)
        const int TL = 0;
        const int TR = W - 1;
        const int BL = (H - 1) * W;
        const int BR = (H - 1) * W + (W - 1);
        const int cornerIndices[4] = { TL, TR, BL, BR };

        // 히트 반경 (spacing 근사: 가로/세로 중 존재하는 쪽 사용)
        float spacingX = (W >= 2) ? glm::length(P[1].pos - P[0].pos) : 0.25f;
        float spacingY = (H >= 2) ? glm::length(P[W].pos - P[0].pos) : spacingX;
        float baseSpacing = (spacingX > 0.0f && spacingY > 0.0f) ? std::min(spacingX, spacingY) : spacingX;
        float r = baseSpacing * g_app->cornerHitScale;

        auto nearestCorner = [&](int& outIdx, float& outDist) {
            outIdx = -1; outDist = 1e9f;
            for (int i = 0; i < 4; i++) {
                int idx = cornerIndices[i];
                float d = glm::length(P[idx].pos - hit);
                if (d < outDist) { outDist = d; outIdx = idx; }
            }
            };

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            // 1) 코너 먼저 체크 -> 코너 안이면 Corner 드래그
            int cornerIdx; float cornerDist;
            nearestCorner(cornerIdx, cornerDist);
            if (cornerIdx >= 0 && cornerDist <= r) {
                g_app->dragMode = App::DragMode::Corner;
                g_app->dragCorner = cornerIdx;
                g_app->dragAnchor = cornerIdx;
                g_app->dragging = true;

                // 드래그 시작 시 살짝 따라오도록 pos/prev 동기화
                cloth.setParticlePos(cornerIdx, hit, /*movePrev=*/true);
                return;
            }

            // 2) 일반 파티클 드래그
            int nearest = -1; float best = 1e9f;
            for (int i = 0; i < (int)P.size(); i++) {
                float d = glm::length(P[i].pos - hit);
                if (d < best) { best = d; nearest = i; }
            }
            g_app->dragMode = App::DragMode::Particle;
            g_app->dragAnchor = nearest;
            g_app->dragging = (nearest >= 0);
        }
        else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        {
            // 상태 리셋
            g_app->dragging = false;
            g_app->dragAnchor = -1;
            g_app->dragCorner = -1;
            g_app->dragMode = App::DragMode::None;
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        {
            if (g_app->dragging && g_app->dragMode == App::DragMode::Corner && g_app->dragCorner >= 0)
            {
                auto& cloth = g_app->cloth;
                int idx = g_app->dragCorner;

                bool fixed = cloth.isParticleFixed(idx);
                if (fixed)
                {
                    cloth.setParticleFixed(idx, false);
                }
                else
                {
                    cloth.setParticleFixed(idx, true);
                }
            }
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) 
        {
            if (g_app->dragMode == App::DragMode::Corner && cloth.isParticleFixed(g_app->dragCorner)) 
            {
                g_app->dragging = false;
                g_app->dragAnchor = -1;
                g_app->dragCorner = -1;
                g_app->dragMode = App::DragMode::None;
            }
        }
        });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(imguiGlslVersion);

    clothShader = new Shader("shaders/cloth.vert", "shaders/cloth.frag");
    flashShader = new Shader("shaders/flash.vert", "shaders/flash.frag");
    gizmoShader = new Shader("shaders/gizmo.vert", "shaders/gizmo.frag");
    glGenVertexArrays(1, &flashVAO);
    glGenVertexArrays(1, &gizmoVAO);
    glGenBuffers(1, &gizmoVBO);

    glBindVertexArray(gizmoVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    cloth.buildIndices(cloth.getWidth(), cloth.getHeight());
    cloth.initGL();

    clothTex = loadTexture2D(currentTexPath.c_str(), true);

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

        dt = std::min(dt, 1.0f / 30.0f);

        // freeze 감소
        if (freezeTimer > 0.0f) freezeTimer = std::max(0.0f, freezeTimer - dt);

        processInput(dt);

        // 드래그 (freeze 중에는 차단)
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

        // 물리 업데이트 (freeze 중에는 스킵)
        if (freezeTimer <= 0.0f)
        {
            cloth.update(dt);
        }
        cloth.updateGPU();

        // 플래시 감쇠
        flash = std::max(0.0f, flash - dt * 6.0f);

        // --- 렌더 ---
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 행렬 계산 (중복 코드 제거)
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)winWidth / (float)winHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), modelAngle, glm::vec3(0, 1, 0));

        // 천 렌더링
        clothShader->use();
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

        clothShader->setMat4("projection", projection);
        clothShader->setMat4("view", view);
        clothShader->setMat4("model", model);
        clothShader->setVec3("uLightDir", glm::normalize(glm::vec3(0.3f, 1.0f, 0.4f)));
        glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);
        clothShader->setVec3("uViewPos", camPos);
        clothShader->setVec3("uAlbedoColor", glm::vec3(0.85f));
        clothShader->setVec3("uAmbient", glm::vec3(0.06f));
        clothShader->setFloat("uSpecularStrength", 0.25f);
        clothShader->setFloat("uShininess", 48.0f);
        cloth.drawTriangles();

        // --- 코너 표시 Gizmo ---
        const auto& P = cloth.getParticles();
        int W = cloth.getWidth();
        int H = cloth.getHeight();

        // 코너 인덱스 재사용
        const int TL = 0;
        const int TR = W - 1;
        const int BL = (H - 1) * W;
        const int BR = (H - 1) * W + (W - 1);
        int cornerIdx[4] = { TL, TR, BL, BR };

        // 코너 위치 업데이트
        glm::vec3 corners[4] = { P[TL].pos, P[TR].pos, P[BL].pos, P[BR].pos };
        glBindBuffer(GL_ARRAY_BUFFER, gizmoVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(corners), corners);

        // 깊이 무시하고 항상 위에 그리기
        glDisable(GL_DEPTH_TEST);

        gizmoShader->use();
        gizmoShader->setMat4("projection", projection); // 재사용
        gizmoShader->setMat4("view", view); // 재사용
        gizmoShader->setMat4("model", model); // 재사용

        glBindVertexArray(gizmoVAO);

        // 색상 규칙: 고정(빨강) > 드래그 중(노랑) > 비고정(흰색)
        for (int i = 0; i < 4; i++) {
            int idx = cornerIdx[i];

            bool fixed = cloth.isParticleFixed(idx);
            bool draggingThisCorner =
                (dragging && dragMode == DragMode::Corner && dragCorner == idx);

            gizmoShader->setFloat("uSize", draggingThisCorner ? 16.0f : 12.0f);

            glm::vec3 color;
            if (fixed)               color = glm::vec3(1.0f, 0.25f, 0.25f); // 빨강
            else if (draggingThisCorner) color = glm::vec3(1.0f, 0.92f, 0.20f); // 노랑
            else                         color = glm::vec3(1.0f, 1.0f, 1.0f);  // 흰색

            gizmoShader->setVec3("uColor", color);
            glDrawArrays(GL_POINTS, i, 1);
        }

        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);

        // 화면 전체 플래시/정지 오버레이
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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (imguiEnabled)
        {
            ImGui::Begin("Pattern Prompt Builder");

            static char promptBuf[1024] = "";
            static char negBuf[1024] = "";

            auto CountUtf8Codepoints = [](const char* s) {
                int count = 0;
                const unsigned char* p = (const unsigned char*)s;
                while (*p) { if ((*p & 0xC0) != 0x80) ++count; ++p; }
                return count;
                };

            // ---------- Prompt ----------
            ImGui::TextUnformatted("Prompt");
            ImGui::InputTextMultiline("##prompt", promptBuf, IM_ARRAYSIZE(promptBuf),
                ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 8),
                ImGuiInputTextFlags_AllowTabInput);

            {
                const int maxBytes = IM_ARRAYSIZE(promptBuf) - 1;
                const int curBytes = (int)strlen(promptBuf);
                const int curChars = CountUtf8Codepoints(promptBuf);

                float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                char counter[128];
                snprintf(counter, sizeof(counter), "%d chars %d / %d bytes", curChars, curBytes, maxBytes);

                ImVec4 col = (curBytes >= maxBytes) ? ImVec4(1, 0.3f, 0.3f, 1)
                    : (curBytes > (int)(maxBytes * 0.9f)) ? ImVec4(1, 0.7f, 0.2f, 1)
                    : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
                ImVec2 sz = ImGui::CalcTextSize(counter);
                ImGui::SetCursorPosX(right - sz.x);
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextUnformatted(counter);
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PopStyleColor();
            }

            ImGui::Separator();

            // ---------- Negative Prompt ----------
            ImGui::TextUnformatted("Negative Prompt");
            ImGui::InputTextMultiline("##negprompt", negBuf, IM_ARRAYSIZE(negBuf),
                ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 4),
                ImGuiInputTextFlags_AllowTabInput);

            {
                const int maxBytes = IM_ARRAYSIZE(negBuf) - 1;
                const int curBytes = (int)strlen(negBuf);
                const int curChars = CountUtf8Codepoints(negBuf);

                float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                char counter[128];
                snprintf(counter, sizeof(counter), "%d chars %d / %d bytes", curChars, curBytes, maxBytes);

                ImVec4 col = (curBytes >= maxBytes) ? ImVec4(1, 0.3f, 0.3f, 1)
                    : (curBytes > (int)(maxBytes * 0.9f)) ? ImVec4(1, 0.7f, 0.2f, 1)
                    : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
                ImVec2 sz = ImGui::CalcTextSize(counter);
                ImGui::SetCursorPosX(right - sz.x);
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextUnformatted(counter);
                ImGui::PopStyleColor();
            }

            if (ImGui::Button("Run Pattern Generation")) {
                g_app->generateAndLoadTextureFromPrompt(promptBuf, negBuf);
            }

            ImGui::End();
        }


        ImGui::Render();
        glDisable(GL_DEPTH_TEST);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // 리소스 정리
    if (clothTex) glDeleteTextures(1, &clothTex);
    if (flashVAO) glDeleteVertexArrays(1, &flashVAO);
    if (gizmoVBO) glDeleteBuffers(1, &gizmoVBO);
    if (gizmoVAO) glDeleteVertexArrays(1, &gizmoVAO);
    delete gizmoShader;
    delete flashShader;
    delete clothShader;
    glfwTerminate();
}

void App::generateAndLoadTextureFromPrompt(const std::string& prompt,
    const std::string& negative)
{
    if (prompt.empty()) {
        std::cout << "No prompt. Aborting texture generation\n";
        return;
    }

    // venv 우선
    std::filesystem::path venvPy = std::filesystem::path("venv") / "Scripts" / "python.exe";
    std::string py = std::filesystem::exists(venvPy) ? venvPy.string() : "python";

    std::filesystem::path script = "gen_pattern.py";
    std::filesystem::path out = std::filesystem::path("textures") / "generated.png";
    if (!std::filesystem::exists(script)) {
        std::cerr << "Failed to find gen_pattern.py. Check working directory.\n";
        return;
    }
    std::filesystem::create_directories(out.parent_path());

    auto sanitize = [](std::string s) {
        for (auto& ch : s) {
            if (ch == '\"') ch = '\'';    // 큰따옴표 -> 작은따옴표
            if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
        }

        while (!s.empty() && s.back() == '\\') s.pop_back();
        return s;
        };

    std::string p = sanitize(prompt);
    std::string n = sanitize(negative);

    // 명령 구성
    std::string cmd =
        "cmd /C chcp 65001 >NUL & "
        "\"" + py + "\" "
        "\"" + script.string() + "\" "
        "\"" + p + "\" "
        "--seamless-psd --rgb ";

    if (!n.empty()) {
        cmd += "--neg \"" + n + "\" ";
    }

    cmd += "--out \"" + out.string() + "\" 2>&1";

    std::cout << "명령 실행: " << cmd << "\n";
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        std::cerr << "[Error] Python script execution failed (" << rc << ")\n";
        return;
    }

    reloadTexture(out.string());
    lastPrompt = prompt;
    lastNegPrompt = negative;
    std::cout << "Successfully loaded new texture\n";
}


void App::processInput(float dt)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }

    int rightNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    bool clicked = (rightNow == GLFW_PRESS) && !rightDownPrev;
    rightDownPrev = (rightNow == GLFW_PRESS);

    if (clicked && freezeTimer <= 0.0f && !dragging) {
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        glm::vec3 hit = screenToWorldOnPlane(mx, my, 0.0f);

        // 카메라 -> 히트 방향
        glm::vec3 camPos = glm::vec3(glm::inverse(camera.GetViewMatrix())[3]);
        glm::vec3 dir = glm::normalize(hit - camPos);

        // 튜닝 파라미터
        float impulseStrength = 0.6f;
        float impulseRadius = 2.0f;

        cloth.applyRadialImpulse(hit, dir, impulseStrength, impulseRadius);
    }

    static int prevEsc = GLFW_RELEASE;
    int escKey = glfwGetKey(window, GLFW_KEY_ESCAPE);
    if (escKey == GLFW_PRESS && prevEsc == GLFW_RELEASE) {
        glfwSetWindowShouldClose(window, true);
    }
    prevEsc = escKey;

    // ----- 이동/컨트롤 키 -----
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, dt);

    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        cornerHitScale = std::max(0.1f, cornerHitScale - dt);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        cornerHitScale = std::min(3.0f, cornerHitScale + dt);

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

    // ----- O: Export + 플래시 + 프리뷰(일시정지) -----
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
        std::snprintf(title, sizeof(title), "Cloth Simulator  |  tile=%.2f", texScale);
        glfwSetWindowTitle(window, title);
    }
    pO = cO;

    // ----- G / Shift+G: 재생성 / 파일만 리로드 -----
    static int pG = GLFW_RELEASE;
    int  cG = glfwGetKey(window, GLFW_KEY_G);
    bool shift = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
        (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (cG == GLFW_PRESS && pG == GLFW_RELEASE) {
        if (shift) {
            // 파일만 리로드
            std::string gen = "textures/generated.png";
            if (std::filesystem::exists(gen)) reloadTexture(gen);
            else std::cerr << "[Shift+G] textures/generated.png not found.\n";
        }
        else {
            // 최근 prompt/negative 재생성
            std::string prompt = lastPrompt.empty() ? "seamless repeating pattern" : lastPrompt;
            g_app->generateAndLoadTextureFromPrompt(prompt, lastNegPrompt);
        }
    }
    pG = cG;
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

void App::reloadTexture(const std::string& path)
{
    if (!std::filesystem::exists(path)) {
        std::cerr << "Reload failed: file not found -> " << path << "\n";
        return;
    }

    unsigned int newTex = loadTexture2D(path.c_str(), /*srgb=*/true);
    if (!newTex) {
        std::cerr << "Reload failed: loadTexture2D returned 0 for " << path << "\n";
        return;
    }

    if (clothTex) glDeleteTextures(1, &clothTex);
    clothTex = newTex;
    currentTexPath = path;

    // (선택) 애니소트로픽 필터링 최대치로
#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
    glBindTexture(GL_TEXTURE_2D, clothTex);
    GLfloat maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    if (maxAniso > 0.0f) glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
#endif

    char title[256];
    std::snprintf(title, sizeof(title),
        "Cloth Simulator  |  tex=%s  |  tile=%.2f",
        currentTexPath.c_str(), texScale);
    glfwSetWindowTitle(window, title);

    std::cout << "Texture reloaded: " << currentTexPath << "\n";
}

void App::runPatternGen(const char* prompt)
{
    std::string exe = "venv/Scripts/python.exe";
    std::string script = "gen_pattern.py";
    std::string out = "textures/generated.png";

    std::ostringstream oss;
    oss << "\"" << exe << "\" \"" << script << "\" "
        << "\"" << prompt << "\" --out \"" << out << "\"";

    int ret = std::system(oss.str().c_str());
    if (ret == 0) {
        // 성공하면 텍스처 리로드
        reloadTexture(out);
    }
    else {
        std::cout << "[Error] pattern gen failed" << std::endl;
    }
}
