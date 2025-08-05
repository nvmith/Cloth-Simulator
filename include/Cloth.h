#pragma once

#include <vector>
#include <glm/glm.hpp>

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

    void updateVerlet(float deltaTime)
    {
        if (isFixed)
            return;

        glm::vec3 temp = pos;
        pos += (pos - prevPos) + acceleration * deltaTime * deltaTime;
        prevPos = temp;
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
    void update(float deltaTime);
    void applyGravity(const glm::vec3& gravity);
    void satisfyConstraints();
    void draw();

    const std::vector<Particle>& getParticles() const { return particles; }

private:
    int numWidth;
    int numHeight;
    float spacing;

    std::vector<Particle> particles;
    std::vector<Spring> springs;

    int getIndex(int x, int y) const { return y * numWidth + x; }

    void initParticles();
    void initSprings();
};
