#include "SSAO.h"

#include <random>

void GenerateKernel(std::vector<glm::vec3>& samples, int n)
{
   //Use normal normal distribution here since multivariate normal is spherically symmetric
   static std::normal_distribution<float> randomFloats(0.0f, 1.0f); // random floats with mean 0.0 and variance 1.0
   static std::default_random_engine generator;

   samples.clear();

   for (unsigned int i = 0; i < n; ++i)
   {
      glm::vec3 sample(
         randomFloats(generator), 
         randomFloats(generator), 
         fabs(randomFloats(generator))
      );
      sample  = glm::normalize(sample);
      sample *= randomFloats(generator);
      float scale = (float)i / 64.0f;

      scale = glm::mix(0.1f, 1.0f, scale * scale);
      sample *= scale; 
      samples.push_back(sample);  
   }
}


void GenerateRandomTangents(std::vector<glm::vec3>& tangents, int n)
{
   std::uniform_real_distribution<float> randomFloats(-1.0f, 1.0f); // random floats between -1.0, 1.0
   static std::default_random_engine generator;

   tangents.clear();
   for (int i = 0; i < n; ++i) 
   {
      glm::vec3 tangent(
         randomFloats(generator),
         randomFloats(generator),
         0.0f
      );
      tangent = glm::normalize(tangent);
      tangents.push_back(tangent);
   }
}


GLuint GenerateRandomTangentTexture(int w, int h)
{
   GLuint tex_id;
   std::vector<glm::vec3> tangents;
   GenerateRandomTangents(tangents, w*h);

   glGenTextures(1, &tex_id);
   glBindTexture(GL_TEXTURE_2D, tex_id);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, tangents.data());
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   return tex_id;
}