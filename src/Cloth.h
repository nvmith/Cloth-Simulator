#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

// 파티클 구조체
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

    // 파티클에 힘을 적용합니다.
    void applyForce(const glm::vec3& force)
    {
        if (!isFixed)
            acceleration += force;
    }

    // 베를레(Verlet) 통합을 사용하여 위치를 업데이트합니다.
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

// 스프링 구조체
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
    ~Cloth();
    void destroyGL();

    // 앵커(고정점) 인덱스 접근자
    int leftAnchorIndex() const { return getIndex(0, 0); }
    int rightAnchorIndex() const { return getIndex(numWidth - 1, 0); }

    // 파티클 위치 설정
    void setParticlePos(int idx, const glm::vec3& p, bool movePrev = true);

    // 시뮬레이션 상수
    static const float kDamping;
    static const int   kConstraintIters;
    static const float kCorrectionFactorStable;
    static const float kCorrectionFactorWarmup;
    static const int   kGravityWarmupFrames;
    static const float kWindStrength;
    static const glm::vec3 kWindDir;

    // 시뮬레이션
    void update(float deltaTime);
    void applyGravity(const glm::vec3& gravity);
    void satisfyConstraints();

    // 렌더링
    void draw();

    // GPU 헬퍼
    void buildIndices(int w, int h);
    void initGL();
    void updateGPU();
    void drawTriangles();

    // OBJ 익스포트
    bool exportOBJ(const std::string& objPath, const std::string& mtlName, const char* texPath, float uvScale = 1.0f);

    // 접근자
    const std::vector<Particle>& getParticles() const { return particles; }
    int getWidth() const { return numWidth; }
    int getHeight() const { return numHeight; }

    // 노멀 계산
    void computeNormals();

    // 고정 파티클 관리
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

    void applyRadialImpulse(const glm::vec3& center,
        const glm::vec3& dir,
        float strength, float radius);

private:
    // 그리드 정보
    int numWidth;
    int numHeight;
    float spacing;

    // 데이터
    std::vector<Particle> particles;
    std::vector<Spring>   springs;

    // 메시 (인덱스/UV)
    std::vector<unsigned int> indices;
    std::vector<glm::vec2>    uvs;
    int gridW = 0;
    int gridH = 0;

    // GL 핸들
    unsigned int vao = 0;
    unsigned int vboPos = 0;
    unsigned int ebo = 0;
    unsigned int vboUV = 0;
    unsigned int vboNormal = 0;

    // 시뮬레이션 상태
    int frameCount = 0;

    // 유틸리티
    int getIndex(int x, int y) const { return y * numWidth + x; }
    void initParticles();
    void initSprings();
};