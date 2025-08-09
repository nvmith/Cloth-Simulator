#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Particle
{
    glm::vec3 pos;
    glm::vec3 prevPos;
    glm::vec3 acceleration;
    bool isFixed = false;

    Particle(const glm::vec3& position)
        : pos(position), prevPos(position), acceleration(0.0f)
    {
    }

    void applyForce(const glm::vec3& force)
    {
        if (!isFixed)
            acceleration += force;
    }

    // Verlet + Damping
    void updateVerlet(float deltaTime, float damping)
    {
        if (isFixed)
            return;

        glm::vec3 vel = (pos - prevPos) * damping;
        glm::vec3 next = pos + vel + acceleration * deltaTime * deltaTime;

        prevPos = pos;
        pos = next;
        acceleration = glm::vec3(0.0f);
    }
};

struct Spring
{
    int p1, p2;
    float restLength;

    Spring(int i1, int i2, float length)
        : p1(i1), p2(i2), restLength(length)
    {
    }
};

// =========================
// Cloth
// =========================
class Cloth
{
public:
    Cloth(int width, int height, float spacing);

    int leftAnchorIndex() const { return getIndex(0, 0); }
    int rightAnchorIndex() const { return getIndex(numWidth - 1, 0); }

    // 고정 입자 위치를 강제로 세팅 (prevPos도 같이 옮겨서 튐 방지)
    void setParticlePos(int idx, const glm::vec3& p, bool movePrev = true);

    // ---- 시뮬레이션 튜닝값 (선언만; 값은 Cloth.cpp에서 정의) ----
    static const float kDamping;
    static const int   kConstraintIters;
    static const float kCorrectionFactorStable;
    static const float kCorrectionFactorWarmup;
    static const int   kGravityWarmupFrames;
    static const float kWindStrength;
    static const glm::vec3 kWindDir;

    // Simulation
    void update(float deltaTime);
    void applyGravity(const glm::vec3& gravity);
    void satisfyConstraints();

    // Render entry
    void draw();

    // GPU helpers
    void buildIndices(int w, int h);
    void initGL();
    void updateGPU();
    void drawTriangles();

    // Export(OBJ) — Unity 용
    bool exportOBJ(const char* objPath, const char* mtlName, const char* texName) const;

    // Accessors
    const std::vector<Particle>& getParticles() const { return particles; }
    int getWidth() const { return numWidth; }
    int getHeight() const { return numHeight; }

private:
    // Grid
    int numWidth;
    int numHeight;
    float spacing;

    // Data
    std::vector<Particle> particles;
    std::vector<Spring>   springs;    // structural + shear + bend(2-step)

    // Mesh (indices / uvs)
    std::vector<unsigned int> indices;
    std::vector<glm::vec2>    uvs;
    int gridW = 0;
    int gridH = 0;

    // GL handles
    unsigned int vao = 0;
    unsigned int vboPos = 0;
    unsigned int ebo = 0;
    unsigned int vboUV = 0;

    // Sim state
    int frameCount = 0;

    // Utils
    int getIndex(int x, int y) const { return y * numWidth + x; }
    void initParticles();
    void initSprings();
};
