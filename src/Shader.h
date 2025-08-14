#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

class Shader
{
public:
    unsigned int ID = 0;

    Shader(const char* vertexPath, const char* fragmentPath)
        : vPath(vertexPath), fPath(fragmentPath)
    {
        std::string vertexCode, fragmentCode;
        if (!readFile(vertexPath, vertexCode) || !readFile(fragmentPath, fragmentCode)) {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ\n";
        }

        const char* vSrc = vertexCode.c_str();
        const char* fSrc = fragmentCode.c_str();

        unsigned int v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vSrc, nullptr);
        glCompileShader(v);
        checkCompileErrors(v, "VERTEX", vPath);

        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fSrc, nullptr);
        glCompileShader(f);
        checkCompileErrors(f, "FRAGMENT", fPath);

        ID = glCreateProgram();
        glAttachShader(ID, v);
        glAttachShader(ID, f);
        glLinkProgram(ID);
        checkLinkErrors(ID);

        glDeleteShader(v);
        glDeleteShader(f);
    }

    void use() const { glUseProgram(ID); }

    void setInt(const std::string& name, int v)        const { glUniform1i(loc(name), v); }
    void setBool(const std::string& name, bool v)      const { glUniform1i(loc(name), v ? 1 : 0); }
    void setFloat(const std::string& name, float v)    const { glUniform1f(loc(name), v); }

    void setVec2(const std::string& name, const glm::vec2& v) const { glUniform2fv(loc(name), 1, &v[0]); }
    void setVec3(const std::string& name, const glm::vec3& v) const { glUniform3fv(loc(name), 1, &v[0]); }
    void setVec4(const std::string& name, const glm::vec4& v) const { glUniform4fv(loc(name), 1, &v[0]); }

    void setMat3(const std::string& name, const glm::mat3& m) const { glUniformMatrix3fv(loc(name), 1, GL_FALSE, &m[0][0]); }
    void setMat4(const std::string& name, const glm::mat4& m) const { glUniformMatrix4fv(loc(name), 1, GL_FALSE, &m[0][0]); }

    void setTexture2D(const std::string& samplerName, int unit, unsigned int texId) const
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texId);
        setInt(samplerName, unit);
    }

private:
    std::string vPath, fPath;
    mutable std::unordered_map<std::string, int> uniformCache;

    static bool readFile(const char* path, std::string& out)
    {
        try {
            std::ifstream file(path);
            std::stringstream ss; ss << file.rdbuf();
            out = ss.str();
            return true;
        }
        catch (...) { return false; }
    }

    int loc(const std::string& name) const
    {
        auto it = uniformCache.find(name);
        if (it != uniformCache.end()) return it->second;
        int location = glGetUniformLocation(ID, name.c_str());
        uniformCache[name] = location;
        if (location == -1) {
            std::cout << "[Shader] Warning: uniform '" << name << "' not found.\n";
        }
        return location;
    }

    static void checkCompileErrors(unsigned int shader, const char* stage, const std::string& path)
    {
        int success = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[2048]; glGetShaderInfoLog(shader, 2048, nullptr, log);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR [" << stage << "]\n"
                << "Path: " << path << "\n"
                << log << "\n-- --------------------------------------------------- --\n";
        }
    }

    static void checkLinkErrors(unsigned int program)
    {
        int success = 0; glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char log[2048]; glGetProgramInfoLog(program, 2048, nullptr, log);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR\n"
                << log << "\n-- --------------------------------------------------- --\n";
        }
    }
};
