#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include "Cloth.h"

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

GLuint vao, vbo;

void setupBuffers(int pointCount)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, pointCount * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void updateBuffers(const std::vector<Particle>& particles)
{
    std::vector<float> positions;
    positions.reserve(particles.size() * 3);

    for (int i = 0; i < particles.size(); i++)
    {
        positions.push_back(particles[i].pos.x);
        positions.push_back(particles[i].pos.y);
        positions.push_back(particles[i].pos.z);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, positions.size() * sizeof(float), positions.data());
}

int main()
{
    if (!glfwInit())
    {
        std::cout << "GLFW 초기화 실패\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Cloth Simulator", nullptr, nullptr);
    if (!window)
    {
        std::cout << "GLFW 윈도우 생성 실패\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "GLAD 초기화 실패\n";
        return -1;
    }

    glEnable(GL_PROGRAM_POINT_SIZE);
    glPointSize(5.0f);

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    Cloth cloth(30, 20, 0.05f);

    std::cout << "총 파티클 수: " << cloth.getParticles().size() << std::endl;

    setupBuffers(cloth.getParticles().size());

    std::cout << "Particle count: " << cloth.getParticles().size() << "\n";

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        cloth.update(0.016f);  // 60fps 기준
        updateBuffers(cloth.getParticles());

        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, cloth.getParticles().size());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
