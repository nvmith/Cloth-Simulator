#version 330 core
in vec2 vUV;
in vec3 vNormalW;
in vec3 vWorldPos;
out vec4 FragColor;

uniform sampler2D uAlbedo;
uniform int   uUseTexture;
uniform float uTexScale;

uniform vec3 uAlbedoColor;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform vec3 uAmbient;
uniform float uSpecularStrength;
uniform float uShininess;

void main()
{
    vec3 N = normalize(vNormalW);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), uShininess);

    vec3 base = (uUseTexture == 1)
        ? texture(uAlbedo, vUV * uTexScale).rgb
        : uAlbedoColor;

    vec3 color = base * (uAmbient + diff * (1.0 - uAmbient))
               + vec3(1.0) * (uSpecularStrength * spec);
    FragColor = vec4(color, 1.0);
}
