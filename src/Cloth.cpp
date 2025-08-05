#include <Cloth.h>

Cloth::Cloth(int width, int height, float spacing)
    : numWidth(width), numHeight(height), spacing(spacing)
{
    initParticles();
    initSprings();
}

void Cloth::update(float deltaTime)
{
    applyGravity(glm::vec3(0.0f, -9.8f, 0.0f));

    for (int i = 0; i < particles.size(); i++)
    {
        particles[i].updateVerlet(deltaTime);
    }

    satisfyConstraints();
}

void Cloth::applyGravity(const glm::vec3& gravity)
{
    for (int i = 0; i < particles.size(); i++)
    {
        particles[i].applyForce(gravity);
    }
}

void Cloth::satisfyConstraints()
{
    for (int i = 0; i < springs.size(); i++)
    {
        Spring& s = springs[i];
        Particle& p1 = particles[s.p1];
        Particle& p2 = particles[s.p2];

        glm::vec3 delta = p2.pos - p1.pos;
        float dist = glm::length(delta);
        float diff = (dist - s.restLength) / dist;

        glm::vec3 correction = delta * 0.5f * diff;

        if (!p1.isFixed) p1.pos += correction;
        if (!p2.isFixed) p2.pos -= correction;
    }
}

void Cloth::draw()
{
    //TODO: 구현 예정
}

void Cloth::initParticles()
{
    particles.clear();
    particles.reserve(numWidth * numHeight);

    for(int y = 0; y < numHeight; y++)
    {
        for(int x = 0; x < numWidth; x++)
        {
            glm::vec3 pos = glm::vec3(
                (x - numWidth / 2.0f) * spacing,
                -(y - numHeight / 2.0f) * spacing,
                0.0f
            );

            Particle p(pos);

            if(y==0 && (x==0 || x==numWidth-1))
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

    for(int y=0; y<numHeight; y++)
    {
        for(int x=0; x<numWidth; x++)
        {
            int current = getIndex(x, y);

            if(x<numWidth-1)
            {
                int right = getIndex(x+1, y);
                float restLength = spacing;
                springs.push_back(Spring(current, right, restLength));
            }

            if(y<numHeight-1)
            {
                int below = getIndex(x, y+1);
                float restLength = spacing;
                springs.push_back(Spring(current, below, restLength));
            }
        }
    }
}

