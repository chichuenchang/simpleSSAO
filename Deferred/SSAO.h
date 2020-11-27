#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

//See http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
// https://learnopengl.com/Advanced-Lighting/SSAO

void GenerateKernel(std::vector<glm::vec3>& samples, int n);
void GenerateRandomTangents(std::vector<glm::vec3>& tangents, int n);

GLuint GenerateRandomTangentTexture(int w, int h);