#include "Cloth.h"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cmath>

namespace fs = std::filesystem;

const float Cloth::kDamping = 0.99f;
const int   Cloth::kConstraintIters = 8;
const float Cloth::kCorrectionFactorStable = 0.22f;
const float Cloth::kCorrectionFactorWarmup = 0.38f;
const int   Cloth::kGravityWarmupFrames = 60;
const float Cloth::kWindStrength = 0.0f;
const glm::vec3 Cloth::kWindDir = glm::vec3(0.0f, 0.0f, 0.0f);

Cloth::Cloth(int width, int height, float spacing)
    : numWidth(width), numHeight(height), spacing(spacing)
{
    initParticles();
    initSprings();

    buildIndices(numWidth, numHeight);
}

bool Cloth::exportOBJ(const std::string& objPath, const std::string& mtlName, const char* texPath, float uvScale)
{
    try {
        fs::path objP = fs::path(objPath);
        if (objP.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(objP.parent_path(), ec);
        }
        fs::path mtlP = objP.parent_path() / mtlName;

        computeNormals();

        std::ofstream obj(objP, std::ios::out | std::ios::trunc);
        if (!obj) return false;

        std::string texFileOnly;
        if (!mtlName.empty()) {
            std::ofstream mtl(mtlP, std::ios::out | std::ios::trunc);
            if (!mtl) return false;

            mtl << "newmtl clothMat\n";
            mtl << "Ka 0.000 0.000 0.000\n";
            mtl << "Kd 1.000 1.000 1.000\n";
            mtl << "Ks 0.020 0.020 0.020\n";
            mtl << "Ns 10.0\n";

            if (texPath && texPath[0] != '\0') {
                texFileOnly = fs::path(texPath).filename().string();
                mtl << "map_Kd -s " << uvScale << " " << uvScale << " 1 " << texFileOnly << "\n";
            }
        }

        obj << "# cloth export\n";
        if (!mtlName.empty()) obj << "mtllib " << mtlName << "\n";
        obj << "usemtl clothMat\n";
        obj << std::fixed << std::setprecision(6);

        for (const auto& p : particles) {
            obj << "v " << p.pos.x << " " << p.pos.y << " " << p.pos.z << "\n";
        }

        if (!uvs.empty()) {
            for (const auto& t : uvs) {
                obj << "vt " << (t.x * uvScale) << " " << (t.y * uvScale) << "\n";
            }
        }
        else {
            for (const auto& p : particles) {
                obj << "vt " << (p.uv.x * uvScale) << " " << (p.uv.y * uvScale) << "\n";
            }
        }

        for (const auto& p : particles) {
            obj << "vn " << p.normal.x << " " << p.normal.y << " " << p.normal.z << "\n";
        }

        obj << "s off\n";

        // f (indices: 0-base → 1-base)
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const unsigned int a = indices[i] + 1;
            const unsigned int b = indices[i + 1] + 1;
            const unsigned int c = indices[i + 2] + 1;
            obj << "f " << a << "/" << a << "/" << a << " "
                << b << "/" << b << "/" << b << " "
                << c << "/" << c << "/" << c << "\n";
        }
        
        if (!texFileOnly.empty()) {
            const fs::path src = fs::path(texPath);
            const fs::path dst = objP.parent_path() / texFileOnly;
            std::error_code ec;
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

void Cloth::update(float deltaTime)
{
    float t = 1.0f;
    if (frameCount < Cloth::kGravityWarmupFrames)
    {
        t = static_cast<float>(frameCount) / static_cast<float>(Cloth::kGravityWarmupFrames);
    }
    float gravityScale = t;

    applyGravity(glm::vec3(0.0f, -9.8f * gravityScale, 0.0f));

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

    for (int i = 0; i < static_cast<int>(particles.size()); i++)
    {
        particles[i].updateVerlet(deltaTime, Cloth::kDamping);
    }

    for (int iter = 0; iter < Cloth::kConstraintIters; iter++)
    {
        satisfyConstraints();
    }

    computeNormals();

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

void Cloth::draw()
{
    drawTriangles();
}

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

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i1);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }

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

    glGenBuffers(1, &vboPos);
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    std::vector<glm::vec3> posInit(particles.size());
    for (size_t i = 0; i < particles.size(); i++)
        posInit[i] = particles[i].pos;
    glBufferData(GL_ARRAY_BUFFER, posInit.size() * sizeof(glm::vec3), posInit.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    if (!uvs.empty())
    {
        glGenBuffers(1, &vboUV);
        glBindBuffer(GL_ARRAY_BUFFER, vboUV);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), uvs.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    }

    glGenBuffers(1, &vboNormal);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormal);
    std::vector<glm::vec3> normalsInit(particles.size(), glm::vec3(0.0f));
    glBufferData(GL_ARRAY_BUFFER, normalsInit.size() * sizeof(glm::vec3), normalsInit.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void Cloth::updateGPU()
{
    if (vboPos)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vboPos);
        static std::vector<glm::vec3> posBuf;
        posBuf.resize(particles.size());
        for (size_t i = 0; i < particles.size(); i++)
            posBuf[i] = particles[i].pos;

        glBufferSubData(GL_ARRAY_BUFFER, 0, posBuf.size() * sizeof(glm::vec3), posBuf.data());
    }

    if (vboNormal)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vboNormal);
        static std::vector<glm::vec3> nrmBuf;
        nrmBuf.resize(particles.size());
        for (size_t i = 0; i < particles.size(); i++)
            nrmBuf[i] = particles[i].normal;

        glBufferSubData(GL_ARRAY_BUFFER, 0, nrmBuf.size() * sizeof(glm::vec3), nrmBuf.data());
    }
}

void Cloth::drawTriangles()
{
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(vao);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, (void*)0);

    glBindVertexArray(0);
}

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

            // 가로/세로
            if (x < numWidth - 1)
                springs.emplace_back(current, getIndex(x + 1, y), spacing);
            if (y < numHeight - 1)
                springs.emplace_back(current, getIndex(x, y + 1), spacing);

            // 대각
            if (x < numWidth - 1 && y < numHeight - 1)
                springs.emplace_back(current, getIndex(x + 1, y + 1), spacing * std::sqrt(2.0f));
            if (x > 0 && y < numHeight - 1)
                springs.emplace_back(current, getIndex(x - 1, y + 1), spacing * std::sqrt(2.0f));

            // 2칸
            if (x < numWidth - 2)
                springs.emplace_back(current, getIndex(x + 2, y), spacing * 2.0f);
            if (y < numHeight - 2)
                springs.emplace_back(current, getIndex(x, y + 2), spacing * 2.0f);
        }
    }
}

void Cloth::computeNormals()
{
    for (auto& p : particles) p.normal = glm::vec3(0.0f);

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        unsigned int i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
        const glm::vec3& p0 = particles[i0].pos;
        const glm::vec3& p1 = particles[i1].pos;
        const glm::vec3& p2 = particles[i2].pos;

        glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
        if (glm::dot(n, n) > 1e-12f) n = glm::normalize(n);

        particles[i0].normal += n;
        particles[i1].normal += n;
        particles[i2].normal += n;
    }
    for (auto& p : particles)
        p.normal = (glm::dot(p.normal, p.normal) > 1e-12f) ? glm::normalize(p.normal) : glm::vec3(0, 0, 1);
}

void Cloth::setParticlePos(int idx, const glm::vec3& p, bool movePrev)
{
    if (idx < 0 || idx >= (int)particles.size()) return;
    particles[idx].pos = p;
    if (movePrev) particles[idx].prevPos = p;
}