#include "Cloth.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

// ===== Cloth 클래스 상수 정의 (inline 없이) =====
const float Cloth::kDamping = 0.99f;
const int   Cloth::kConstraintIters = 8;
const float Cloth::kCorrectionFactorStable = 0.22f;
const float Cloth::kCorrectionFactorWarmup = 0.38f;
const int   Cloth::kGravityWarmupFrames = 60;
const float Cloth::kWindStrength = 0.0f;
const glm::vec3 Cloth::kWindDir = glm::vec3(0.0f, 0.0f, 0.0f);

// -------------------- ctor --------------------
Cloth::Cloth(int width, int height, float spacing)
    : numWidth(width), numHeight(height), spacing(spacing)
{
    initParticles();
    initSprings();

    buildIndices(numWidth, numHeight);
    // initGL()은 GL 컨텍스트 준비 이후(App 쪽)에서 호출
}

// -------------------- sim --------------------
void Cloth::update(float deltaTime)
{
    // 중력 이징(초반 튐 완화)
    float t = 1.0f;
    if (frameCount < Cloth::kGravityWarmupFrames)
    {
        t = static_cast<float>(frameCount) / static_cast<float>(Cloth::kGravityWarmupFrames);
    }
    float gravityScale = t;

    applyGravity(glm::vec3(0.0f, -9.8f * gravityScale, 0.0f));

    // 선택: 바람
    if (Cloth::kWindStrength > 0.0f)
    {
        glm::vec3 wdir = (glm::length(Cloth::kWindDir) > 1e-6f)
            ? glm::normalize(Cloth::kWindDir)
            : glm::vec3(0.0f);
        glm::vec3 wind = wdir * (9.8f * Cloth::kWindStrength);
        for (int i = 0; i < static_cast<int>(particles.size()); i++)
        {
            particles[i].applyForce(wind);
        }
    }

    // Verlet + damping
    for (int i = 0; i < static_cast<int>(particles.size()); i++)
    {
        particles[i].updateVerlet(deltaTime, Cloth::kDamping);
    }

    // 제약 반복(수렴)
    for (int iter = 0; iter < Cloth::kConstraintIters; iter++)
    {
        satisfyConstraints();
    }

    frameCount++;
}

void Cloth::applyGravity(const glm::vec3& gravity)
{
    for (int i = 0; i < static_cast<int>(particles.size()); i++)
    {
        particles[i].applyForce(gravity);
    }
}

void Cloth::satisfyConstraints()
{
    // 초반엔 강하게, 이후엔 안정값
    float factor = (frameCount < Cloth::kGravityWarmupFrames)
        ? Cloth::kCorrectionFactorWarmup
        : Cloth::kCorrectionFactorStable;

    for (int i = 0; i < static_cast<int>(springs.size()); i++)
    {
        Spring& s = springs[i];
        Particle& p1 = particles[s.p1];
        Particle& p2 = particles[s.p2];

        glm::vec3 delta = p2.pos - p1.pos;
        float dist = glm::length(delta);
        if (dist < 1e-8f)
        {
            continue;
        }

        float diff = (dist - s.restLength) / dist;
        glm::vec3 correction = delta * (factor * diff);

        if (!p1.isFixed) p1.pos += correction;
        if (!p2.isFixed) p2.pos -= correction;
    }
}

// -------------------- draw entry --------------------
void Cloth::draw()
{
    drawTriangles();
}

// -------------------- mesh & GL --------------------
void Cloth::buildIndices(int w, int h)
{
    gridW = w;
    gridH = h;
    indices.clear();
    indices.reserve((w - 1) * (h - 1) * 6);

    for (int y = 0; y < h - 1; y++)
    {
        for (int x = 0; x < w - 1; x++)
        {
            unsigned int i0 = y * w + x;
            unsigned int i1 = i0 + 1;
            unsigned int i2 = i0 + w;
            unsigned int i3 = i2 + 1;

            // CCW(반시계)로 두 개 삼각형
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i1);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }

    // UV 생성
    uvs.resize(w * h);
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float u = static_cast<float>(x) / static_cast<float>(w - 1);
            float v = static_cast<float>(y) / static_cast<float>(h - 1);
            uvs[y * w + x] = glm::vec2(u, v);
        }
    }
}

void Cloth::initGL()
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // 위치 VBO
    glGenBuffers(1, &vboPos);
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);

    std::vector<glm::vec3> posInit;
    posInit.resize(particles.size());
    for (size_t i = 0; i < particles.size(); i++)
    {
        posInit[i] = particles[i].pos;
    }
    glBufferData(GL_ARRAY_BUFFER, posInit.size() * sizeof(glm::vec3), posInit.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    // UV VBO
    if (!uvs.empty())
    {
        glGenBuffers(1, &vboUV);
        glBindBuffer(GL_ARRAY_BUFFER, vboUV);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), uvs.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    }

    // EBO
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void Cloth::updateGPU()
{
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);

    static std::vector<glm::vec3> uploadBuf;
    uploadBuf.resize(particles.size());
    for (size_t i = 0; i < particles.size(); i++)
    {
        uploadBuf[i] = particles[i].pos;
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, uploadBuf.size() * sizeof(glm::vec3), uploadBuf.data());
}

void Cloth::drawTriangles()
{
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(vao);

    // 필요 시 와이어 체크
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, (void*)0);

    // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
}

// -------------------- init helpers --------------------
void Cloth::initParticles()
{
    particles.clear();
    particles.reserve(numWidth * numHeight);

    for (int y = 0; y < numHeight; y++)
    {
        for (int x = 0; x < numWidth; x++)
        {
            glm::vec3 pos = glm::vec3(
                (x - numWidth / 2.0f) * spacing,
                -(y - numHeight / 2.0f) * spacing,
                0.0f
            );

            Particle p(pos);

            // 윗변 좌/우 고정 (U자 자연스러운 처짐)
            if (y == 0 && (x == 0 || x == numWidth - 1))
            {
                p.isFixed = true;
            }

            particles.push_back(p);
        }
    }
}

void Cloth::initSprings()
{
    springs.clear();
    springs.reserve(numWidth * numHeight * 6);

    for (int y = 0; y < numHeight; y++)
    {
        for (int x = 0; x < numWidth; x++)
        {
            int current = getIndex(x, y);

            // Structural (가로/세로)
            if (x < numWidth - 1)
                springs.emplace_back(current, getIndex(x + 1, y), spacing);
            if (y < numHeight - 1)
                springs.emplace_back(current, getIndex(x, y + 1), spacing);

            // Shear (대각)
            if (x < numWidth - 1 && y < numHeight - 1)
                springs.emplace_back(current, getIndex(x + 1, y + 1), spacing * std::sqrt(2.0f));
            if (x > 0 && y < numHeight - 1)
                springs.emplace_back(current, getIndex(x - 1, y + 1), spacing * std::sqrt(2.0f));

            // Bend (2칸)
            if (x < numWidth - 2)
                springs.emplace_back(current, getIndex(x + 2, y), spacing * 2.0f);
            if (y < numHeight - 2)
                springs.emplace_back(current, getIndex(x, y + 2), spacing * 2.0f);
        }
    }
}

// -------------------- Export OBJ --------------------
bool Cloth::exportOBJ(const char* objPath, const char* mtlName, const char* texName) const
{
    std::ofstream ofs(objPath);
    if (!ofs.is_open())
    {
        return false;
    }

    std::string mtlFile = mtlName ? mtlName : "cloth.mtl";
    ofs << "mtllib " << mtlFile << "\n";
    ofs << "o Cloth\n";

    // v
    for (const auto& p : particles)
    {
        ofs << "v " << std::fixed << std::setprecision(6)
            << p.pos.x << " " << p.pos.y << " " << p.pos.z << "\n";
    }
    // vt
    for (const auto& t : uvs)
    {
        ofs << "vt " << std::fixed << std::setprecision(6)
            << t.x << " " << (1.0f - t.y) << "\n"; // Unity 편의상 V 뒤집기
    }

    ofs << "usemtl clothMat\n";
    ofs << "s off\n";

    // f (OBJ는 1-based index, v/vt 동일 인덱스 사용)
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        unsigned int i0 = indices[i + 0] + 1;
        unsigned int i1 = indices[i + 1] + 1;
        unsigned int i2 = indices[i + 2] + 1;

        ofs << "f "
            << i0 << "/" << i0 << " "
            << i1 << "/" << i1 << " "
            << i2 << "/" << i2 << "\n";
    }

    ofs.close();

    // .mtl 생성
    std::string mtlPath = std::string(objPath);
    size_t slash = mtlPath.find_last_of("/\\");
    std::string dir = (slash == std::string::npos) ? "" : mtlPath.substr(0, slash + 1);
    std::ofstream mtl(dir + mtlFile);
    if (mtl.is_open())
    {
        mtl << "newmtl clothMat\n";
        mtl << "Kd 1.000000 1.000000 1.000000\n";
        if (texName && std::string(texName).size() > 0)
        {
            mtl << "map_Kd " << texName << "\n";
        }
        mtl.close();
    }

    return true;
}

void Cloth::setParticlePos(int idx, const glm::vec3& p, bool movePrev)
{
    if (idx < 0 || idx >= (int)particles.size()) return;
    particles[idx].pos = p;
    if (movePrev) particles[idx].prevPos = p;
}