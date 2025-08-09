#include "App.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

static App* g_app = nullptr;

App::App(int width, int height)
    : winWidth(width), winHeight(height),
    cloth(20, 20, 0.2f),
    camera(glm::vec3(0.0f, 0.0f, 8.0f)),
    clothShader(nullptr)
{
    g_app = this;
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

            const auto& P = g_app->cloth.getParticles();
            int L = 0;
            int R = g_app->cloth.getWidth() - 1;
            float dL = glm::length(P[L].pos - hit);
            float dR = glm::length(P[R].pos - hit);
            g_app->dragAnchor = (dL < dR ? L : R);
            g_app->dragging = true;
        }
        else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        {
            g_app->dragging = false;
            g_app->dragAnchor = -1;
        }
        });

    clothShader = new Shader("shaders/cloth.vert", "shaders/cloth.frag");

    cloth.buildIndices(cloth.getWidth(), cloth.getHeight());
    cloth.initGL();

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

        processInput(dt);

        if (dragging && dragAnchor >= 0)
        {
            double mx, my; glfwGetCursorPos(window, &mx, &my);
            glm::vec3 p = screenToWorldOnPlane(mx, my, 0.0f);
            cloth.setParticlePos(dragAnchor, p, true);
        }

        cloth.update(dt);
        cloth.updateGPU();

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        clothShader->use();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)winWidth / winHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        clothShader->setMat4("projection", proj);
        clothShader->setMat4("view", view);
        clothShader->setMat4("model", glm::mat4(1.0f));

        cloth.drawTriangles();

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
