#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

struct Particle
{
    glm::vec3 pos;
    glm::vec3 prevPos;
    glm::vec3 acceleration;
    glm::vec3 restPos;
    bool isFixed = false;
    glm::vec2 uv;

    glm::vec3 normal = glm::vec3(0.0f);

    Particle(const glm::vec3& p)
        : pos(p), prevPos(p), restPos(p), acceleration(0.0f) {
    }

    void applyForce(const glm::vec3& force)
    {
        if (!isFixed)
            acceleration += force;
    }

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

class Cloth
{
public:
    Cloth(int width, int height, float spacing);

    int leftAnchorIndex() const { return getIndex(0, 0); }
    int rightAnchorIndex() const { return getIndex(numWidth - 1, 0); }

    void setParticlePos(int idx, const glm::vec3& p, bool movePrev = true);

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

    // Export(OBJ)
    bool exportOBJ(const std::string& objPath, const std::string& mtlName, const char* texPath, float uvScale = 1.0f);

    // Accessors
    const std::vector<Particle>& getParticles() const { return particles; }
    int getWidth() const { return numWidth; }
    int getHeight() const { return numHeight; }

    void computeNormals();

    bool isParticleFixed(int idx) const
    {
        return (idx >= 0 && idx < (int)particles.size()) ? particles[idx].isFixed : false;
    }
    void setParticleFixed(int idx, bool fixed)
    {
        if (idx < 0 || idx >= (int)particles.size()) return;
        particles[idx].isFixed = fixed;
        if (fixed) particles[idx].prevPos = particles[idx].pos;
    }
    void toggleParticleFixed(int idx)
    {
        setParticleFixed(idx, !isParticleFixed(idx));
    }

    void clearAllFixed()
    {
        for (auto& p : particles)
            p.isFixed = false;
    }

    void resetToRest()
    {
        for (auto& p : particles)
        {
            p.pos = p.prevPos = p.restPos;
            p.acceleration = glm::vec3(0.0f);
        }
        resetInitialFixed();
    }

    void resetInitialFixed()
    {
        for (auto& p : particles)
            p.isFixed = false;

        int L = 0;
        particles[L].isFixed = true;

        int R = getWidth() - 1;
        particles[R].isFixed = true;
    }

private:
    // Grid
    int numWidth;
    int numHeight;
    float spacing;

    // Data
    std::vector<Particle> particles;
    std::vector<Spring>   springs;

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
    unsigned int vboNormal = 0;

    // Sim state
    int frameCount = 0;

    // Utils
    int getIndex(int x, int y) const { return y * numWidth + x; }
    void initParticles();
    void initSprings();
};
