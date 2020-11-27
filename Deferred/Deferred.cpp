#include <windows.h>

#include <GL/glew.h>

#include <GL/freeglut.h>

#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <random>

#include "InitShader.h"
#include "LoadMeshTangents.h"
#include "LoadTexture.h"
#include "imgui_impl_glut.h"
#include "VideoMux.h"
#include "DebugCallback.h"
#include "ShaderLocs.h"
#include "SSAO.h"

GLuint fbo = -1;
GLuint depth_rbo = -1;
const int GSIZE = 3;
GLuint gbuffer[GSIZE] = {-1}; //tex ids for gbuffer

//for SSAO
const int ssaoNumSamples = 64;
int tangentTexSize = 3;
std::vector<glm::vec3> ssaoSamples;
GLuint ssaoTangents = -1;
GLuint ssaoTex[2] = {-1};


MeshData mesh_data[4];
GLuint diffuse_map_id[4] = {-1}; 

GLuint attributeless_vao; //for attributeless rendering


//names of the shader files to load

const int NumShaders = 4;
GLuint shader_program[NumShaders] = {-1};
static const std::string vertex_shader[NumShaders] = {"gbuffer_vs.glsl", "ssao_vs.glsl", "blur_vs.glsl", "lighting_vs.glsl"};
static const std::string fragment_shader[NumShaders] = {"gbuffer_fs.glsl", "ssao_fs.glsl", "blur_fs.glsl","lighting_fs.glsl"};

//names of the mesh and texture files to load
static const std::string mesh_name[] = {"museumhallRD.obj","Lighthouse.obj","Terrain.obj","torus.obj"};
//Texture maps from filter forge: https://www.filterforge.com/filters/
static const std::string diffuse_map_name[] = {"235-diffuse.jpg", "1792-diffuse.jpg", "1857-diffuse.jpg", "6903-diffuse.jpg"};

//indices of current objects in above arrays
int current_mesh = 0;
int current_map = 3;
int current_shader = 0;

float time_sec = 0.0f;
float angle = 0.0f;
float cam_z = 3.0f;
bool recording = false;

//Bind all textures at once or all passes, to simplify display(...)
void BindAllTextures()
{
   for(int i=0; i<GSIZE; i++) //0, 1, 2 = pos, normal, color
   {
      glActiveTexture(GL_TEXTURE0+i); //binding = 0 in shader, no need for glUniform1i(...)
      glBindTexture(GL_TEXTURE_2D, gbuffer[i]);
   }

   for(int i=0; i<2; i++) //3, 4 = ssao0, ssao blurred
   {
      glActiveTexture(GL_TEXTURE0+GSIZE+i);
      glBindTexture(GL_TEXTURE_2D, ssaoTex[i]);
   }

   glActiveTexture(GL_TEXTURE0 + 5);
   glBindTexture(GL_TEXTURE_2D, ssaoTangents);

   glActiveTexture(GL_TEXTURE0 + 6); 
   glBindTexture(GL_TEXTURE_2D, diffuse_map_id[current_map]); 

   glActiveTexture(GL_TEXTURE0 + 7);
   glBindTexture(GL_TEXTURE_2D, depth_rbo);//bind the depth texture

}

//Draw the user interface using ImGui
void draw_gui()
{
   const int w = glutGet(GLUT_WINDOW_WIDTH);
   const int h = glutGet(GLUT_WINDOW_HEIGHT);
   const float aspect_ratio = float(w) / float(h);

   ImGui_ImplGlut_NewFrame();

   const int filename_len = 256;
   static char video_filename[filename_len] = "capture.mp4";

   ImGui::InputText("Video filename", video_filename, filename_len);
   ImGui::SameLine();
   if (recording == false)
   {
      if (ImGui::Button("Start Recording"))
      {
         const int w = glutGet(GLUT_WINDOW_WIDTH);
         const int h = glutGet(GLUT_WINDOW_HEIGHT);
         recording = true;
         start_encoding(video_filename, w, h); //Uses ffmpeg
      }
      
   }
   else
   {
      if (ImGui::Button("Stop Recording"))
      {
         recording = false;
         finish_encoding(); //Uses ffmpeg
      }
   }

   //camera controls
   ImGui::SliderFloat("View angle", &angle, -3.141592f, +3.141592f);
   ImGui::SliderFloat("Cam z", &cam_z, -5.0f, +5.0f);

   const ImVec2 isize(aspect_ratio*128, 128);
   const ImVec2 uv0(0.0, 1.0);
   const ImVec2 uv1(1.0, 0.0);

   ImGui::Columns(4);
   ImGui::Image((void*)gbuffer[0], isize, uv0, uv1);
   ImGui::Text("gbuffer[0]");
   ImGui::NextColumn();
   ImGui::Image((void*)gbuffer[1], isize, uv0, uv1);
   ImGui::Text("gbuffer[1]");
   ImGui::NextColumn();
   ImGui::Image((void*)gbuffer[2], isize, uv0, uv1);
   ImGui::Text("gbuffer[2]");
   
   ImGui::Columns(2);
   ImGui::Image((void*)ssaoTex[0], isize, uv0, uv1);
   ImGui::Text("ssaoTex[0]");
   ImGui::NextColumn();
   ImGui::Image((void*)ssaoTex[1], isize, uv0, uv1);
   ImGui::Text("ssaoTex[1]");

   ImGui::Columns(3);
   ImGui::Separator();
   ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Mode");
   static int mode = 0;
   ImGui::RadioButton("Show unfiltered AO", &mode, 0);
   ImGui::RadioButton("Show filtered AO", &mode, 1);
   ImGui::RadioButton("Show Deferred Shading", &mode, 2); 
   glProgramUniform1i(shader_program[3], UniformLoc::Mode, mode);
   ImGui::NextColumn();
   ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Mesh");
   for(int i=0; i<4; i++)
   {
      ImGui::RadioButton(mesh_name[i].c_str(), &current_mesh, i);
   }
   ImGui::NextColumn();
   ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "AO params");
   if(ImGui::Button("Resample kernel"))
   {
      GenerateKernel(ssaoSamples, ssaoNumSamples);
   }
   if(ImGui::Button("Resample tangents"))
   {
      glDeleteTextures(1, &ssaoTangents);
      ssaoTangents = GenerateRandomTangentTexture(tangentTexSize, tangentTexSize);
   }
   static int ssaoNumSamples = 64;
   if(ImGui::SliderInt("ssaoNumSamples", &ssaoNumSamples, 1, 64))
   {
      glProgramUniform1i(shader_program[1], UniformLoc::ssaoNumSamples, ssaoNumSamples);
   }
   if(ImGui::SliderInt("tangentTexSize", &tangentTexSize, 1, 16))
   {
      glDeleteTextures(1, &ssaoTangents);
      ssaoTangents = GenerateRandomTangentTexture(tangentTexSize, tangentTexSize);
   }
   static float ssaoRadius = 0.25f;
   if(ImGui::SliderFloat("ssaoRadius", &ssaoRadius, 0.0f, 1.0f))
   {
      glProgramUniform1f(shader_program[1], UniformLoc::ssaoRadius, ssaoRadius);
   }
   static float ssaoBias = 0.025f;
   if(ImGui::SliderFloat("ssaoBias", &ssaoBias, 0.0f, 0.10f))
   {
      glProgramUniform1f(shader_program[1], UniformLoc::ssaoBias, ssaoBias);
   }
   
   ImGui::Columns(1);

   //ImGui::ShowTestWindow();
   ImGui::Render();
 }

// glut display callback function.
// This function gets called every time the scene gets redisplayed 
void display()
{
   BindAllTextures();

   const int w = glutGet(GLUT_WINDOW_WIDTH);
   const int h = glutGet(GLUT_WINDOW_HEIGHT);
   const float aspect_ratio = float(w) / float(h);

   glm::mat4 V = glm::lookAt(glm::vec3(0.0f, 0.0f, -5.0f- cam_z), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f));
   glm::mat4 P = glm::perspective(3.141592f / 4.0f, aspect_ratio, 0.1f, 100.0f);

   ////////
   //PASS 1: Gbuffer
   ////////
   glUseProgram(shader_program[0]);

   glUniformMatrix4fv(UniformLoc::V, 1, false, glm::value_ptr(V));

   glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Render to FBO, all gbuffer textures
   const GLenum gbuffer_draw_buffers[GSIZE] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
   glDrawBuffers(GSIZE, gbuffer_draw_buffers);

   //clear the FBO
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   static std::uniform_real_distribution<float> urand(-1.0f, 1.0f); // random floats between 0.0 - 1.0
   std::default_random_engine generator;

   glBindVertexArray(mesh_data[current_mesh].mVao);
  
   //for(int i=0; i<25; i++)
   //{
   //   //randomly place some meshes in the scene
   //   float randScale = 0.2f + 0.4f*abs(urand(generator));
   //   glm::vec3 randPos(urand(generator), urand(generator), urand(generator));
   //   glm::mat4 M = glm::translate(0.5f*randPos)*glm::scale(glm::vec3(randScale*mesh_data[current_mesh].mScaleFactor));
   //   glUniformMatrix4fv(UniformLoc::M, 1, false, glm::value_ptr(M));
   //   {
   //      glm::mat4 PVM = P*V*M;
   //      glUniformMatrix4fv(UniformLoc::PVM, 1, false, glm::value_ptr(PVM));
   //   }
   //   mesh_data[current_mesh].DrawMesh();
   //}

  // std::cout << urand(generator) << std::endl;
	   //randomly place some meshes in the scene
	   //float randScale = 0.2f + 0.4f*abs(urand(generator));
	   glm::vec3 randPos(urand(generator), urand(generator), urand(generator));
	   glm::mat4 M = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f))*glm::scale(glm::vec3(10.0f*mesh_data[current_mesh].mScaleFactor));
	   glUniformMatrix4fv(UniformLoc::M, 1, false, glm::value_ptr(M));
	   {
		   glm::mat4 PVM = P * V*M;
		   glUniformMatrix4fv(UniformLoc::PVM, 1, false, glm::value_ptr(PVM));
	   }
	   mesh_data[current_mesh].DrawMesh();
	   
	   glBindVertexArray(mesh_data[current_mesh+2].mVao);
	   M = glm::translate(glm::vec3(0.0f, -7.5f, 0.0f))*glm::scale(glm::vec3(50.0f*mesh_data[current_mesh].mScaleFactor));
	   glUniformMatrix4fv(UniformLoc::M, 1, false, glm::value_ptr(M));
	   {
		   glm::mat4 PVM = P * V*M;
		   glUniformMatrix4fv(UniformLoc::PVM, 1, false, glm::value_ptr(PVM));
	   }
	   mesh_data[current_mesh+2].DrawMesh();

	   

   ////////
   //PASS 2 : SSAO
   ////////
         
   glUseProgram(shader_program[1]);
   //glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Do not render the next pass to FBO.
   glDrawBuffer(GL_COLOR_ATTACHMENT0+GSIZE); // Render to ssao[0] 
   glClear(GL_COLOR_BUFFER_BIT);

   glUniformMatrix4fv(UniformLoc::P, 1, false, glm::value_ptr(P));

   //SSAO variables
   glUniform3fv(UniformLoc::ssaoKernel, ssaoNumSamples, glm::value_ptr(ssaoSamples.data()[0]));

   glDepthMask(GL_FALSE); //disable writes to depth buffer. Keep the cleared depth values when drawing the quad.
   glBindVertexArray(attributeless_vao);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // draw quad
   glBindVertexArray(0);
   glDepthMask(GL_TRUE); //enable writes to depth buffer

   ////////
   //PASS 3 : Blur
   ////////
         
   glUseProgram(shader_program[2]);
   //glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Do not render the next pass to FBO.
   glDrawBuffer(GL_COLOR_ATTACHMENT0+GSIZE+1); // Render to ssao[1] 
   glClear(GL_COLOR_BUFFER_BIT); //Clear the back buffer

   glDepthMask(GL_FALSE); //disable writes to depth buffer. Keep the cleared depth values when drawing the quad.
   glBindVertexArray(attributeless_vao);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // draw quad
   glBindVertexArray(0);
   glDepthMask(GL_TRUE); //enable writes to depth buffer
   

   ////////
   //PASS 4 : lighting
   ////////
   
   //This next block is not working. I wanted to blit the fbo depth buffer to the default depth buffer, but blit doesn't work if formats are different 
   //Instead I bind depth as a texture and do a depth test in the fragment shader.

   //glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
   //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
   //glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

   const int AMBIENT_SRC = 0;
   const int POINT_SRC = 1;

   glUseProgram(shader_program[3]);
   glBindFramebuffer(GL_FRAMEBUFFER, 0); // Do not render the next pass to FBO.
   glDrawBuffer(GL_BACK); // Render to back 
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Clear the back buffer

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE); //additively blend lights together

   //First, draw ambient
   glUniform1i(UniformLoc::deferredLightType, AMBIENT_SRC);//ambient light

   glBindVertexArray(attributeless_vao);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // draw full-screen quad (ambient light everywhere)

   //Next draw point light volumes (spheres)
   glDepthMask(GL_FALSE); //don't write light volume depth to depth buffer. Light volumes shouldn't hide each other
   glDisable(GL_DEPTH_TEST); //we are depth testing in fragment shader
   glCullFace(GL_BACK);
   glEnable(GL_CULL_FACE); //Only draw front faces of light volume to prevent double lighting
   
   const int n_point_lights = 80;
   glUniform1i(UniformLoc::deferredLightType, POINT_SRC); //point light
   const int sphere_mesh = 2;
   glBindVertexArray(mesh_data[sphere_mesh].mVao);
   //random light positions and sizes
   for(int i=0; i<n_point_lights; i++)
   {
      float randScale = 0.5f + 0.4f*abs(urand(generator));
      glm::vec3 randPos = 0.95f*glm::normalize(glm::vec3(urand(generator), urand(generator), urand(generator)));
      randPos.y += 3.0*(glm::fract(0.0025f*time_sec*i)-0.5f);
      glm::mat4 M = glm::translate(randPos)*glm::scale(glm::vec3(randScale*mesh_data[sphere_mesh].mScaleFactor));
      {
         glm::mat4 PVM = P*V*M;
         glUniformMatrix4fv(UniformLoc::PVM, 1, false, glm::value_ptr(PVM));
      }
      //send eye-space light position as a uniform 
      glUniform3fv(UniformLoc::lightPos, 1, glm::value_ptr(V*M*glm::vec4(randPos, 1.0f)));

      mesh_data[sphere_mesh].DrawMesh();
   }

   glDisable(GL_CULL_FACE);
   glBindVertexArray(0);
   glDisable(GL_BLEND);
   glDepthMask(GL_TRUE); //enable writes to depth buffer
   glEnable(GL_DEPTH_TEST);

   draw_gui();

   if (recording == true)
   {
      glFinish();

      glReadBuffer(GL_BACK);
      read_frame_to_encode(&rgb, &pixels, w, h);
      encode_frame(rgb);
   }

   glutSwapBuffers();
}

// glut idle callback.
//This function gets called between frames
void idle()
{
	glutPostRedisplay();

   const int time_ms = glutGet(GLUT_ELAPSED_TIME);
   time_sec = 0.001f*time_ms;
}

bool reload_shader(int i)
{
   bool success = true;
   GLuint new_shader = InitShader(vertex_shader[i].c_str(), fragment_shader[i].c_str());

   if(new_shader == -1) // loading failed
   {
      success = false;
   }
   else
   {
      if(shader_program[i] != -1)
      {
         glDeleteProgram(shader_program[i]);
      }
      shader_program[i] = new_shader;
      success = true;
   }
   return success;
}

void reload_shaders()
{
   bool all_loaded = true;
   for(int i=0; i<NumShaders; i++)
   {
      bool success = reload_shader(i);
      all_loaded = all_loaded && success;
   }

   if(all_loaded)
   {
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   }
   else
   {
      glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
   }
}

// Display info about the OpenGL implementation provided by the graphics driver.
// Your version should be > 4.0 for CGT 521 
void printGlInfo()
{
   std::cout << "Vendor: "       << glGetString(GL_VENDOR)                    << std::endl;
   std::cout << "Renderer: "     << glGetString(GL_RENDERER)                  << std::endl;
   std::cout << "Version: "      << glGetString(GL_VERSION)                   << std::endl;
   std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)  << std::endl;

   int max_locs, max_f_comp, max_v_comp, max_blocks, max_f_block_size, max_v_block_size;
   glGetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &max_locs); //must be at least 1024
												  //max int, float or bool values in uniform storage in fragment shader (at least 1024)
   glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_f_comp);
   glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &max_v_comp); //probably same as above
   glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &max_blocks); //at least 12
   glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &max_f_block_size); //at least 12
   glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_v_block_size); //at least 16384 bytes
}

void initOpenGl()
{
   //Initialize glew so that new OpenGL function names can be used
   glewInit();

   RegisterCallback();

   glEnable(GL_DEPTH_TEST);

   reload_shaders();

   //Load meshes and textures
   for(int i=0; i<4; i++)
   {
      mesh_data[i] = LoadMesh(mesh_name[i]); //Helper function: Uses Open Asset Import library.
   }

   for(int i=0; i<4; i++)
   {
      diffuse_map_id[i] = LoadTexture(diffuse_map_name[i].c_str()); //Helper function: Uses FreeImage library
   }

   glGenVertexArrays(1, &attributeless_vao); //generate empty vao for attributeless rendering

   const int w = glutGet(GLUT_WINDOW_WIDTH);
   const int h = glutGet(GLUT_WINDOW_HEIGHT);

   //Create a texture object and set initial wrapping and filtering state
   glGenTextures(GSIZE, gbuffer);

   for(int i=0; i<GSIZE; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gbuffer[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   //Create renderbuffer for depth.
   //glGenRenderbuffers(1, &depth_rbo);
   //glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
   //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);

   glGenTextures(1, &depth_rbo);
   glBindTexture(GL_TEXTURE_2D, depth_rbo);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
   glBindTexture(GL_TEXTURE_2D, 0);

   //Create the framebuffer object
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   //attach the texture we just created to color attachment 1
   for(int i=0; i<GSIZE; i++)
   {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i, GL_TEXTURE_2D, gbuffer[i], 0);
   }

   //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo); // attach depth renderbuffer
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_rbo, 0);

   //SSAO data
   GenerateKernel(ssaoSamples, ssaoNumSamples);
   ssaoTangents = GenerateRandomTangentTexture(tangentTexSize,tangentTexSize);

   glGenTextures(2, ssaoTex);

   //Attach SSAO textures to FBO
   for(int i=0; i<2; i++)
   {
      glBindTexture(GL_TEXTURE_2D, ssaoTex[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, 0);

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+GSIZE+i, GL_TEXTURE_2D, ssaoTex[i], 0);
   }

   //unbind the fbo
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

// glut callbacks need to send keyboard and mouse events to imgui
void keyboard(unsigned char key, int x, int y)
{
   ImGui_ImplGlut_KeyCallback(key);
   std::cout << "key : " << key << ", x: " << x << ", y: " << y << std::endl;

   switch(key)
   {
      case 'r':
      case 'R':
         reload_shaders();     
      break;
   }
}

void keyboard_up(unsigned char key, int x, int y)
{
   ImGui_ImplGlut_KeyUpCallback(key);
}

void special_up(int key, int x, int y)
{
   ImGui_ImplGlut_SpecialUpCallback(key);
}

void passive(int x, int y)
{
   ImGui_ImplGlut_PassiveMouseMotionCallback(x,y);
}

void special(int key, int x, int y)
{
   ImGui_ImplGlut_SpecialCallback(key);
}

void motion(int x, int y)
{
   ImGui_ImplGlut_MouseMotionCallback(x, y);
}

void mouse(int button, int state, int x, int y)
{
   ImGui_ImplGlut_MouseButtonCallback(button, state);
}


int main (int argc, char **argv)
{
   //Configure initial window state using freeglut

   #if _DEBUG
   glutInitContextFlags(GLUT_DEBUG);
   #endif
   glutInitContextVersion(4, 3);

   glutInit(&argc, argv); 
   glutInitDisplayMode (GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
   glutInitWindowPosition (5, 5);
   glutInitWindowSize (1280, 720);
   int win = glutCreateWindow (argv[0]); //no OpenGL functions before this point

   printGlInfo();

   //Register callback functions with glut. 
   glutDisplayFunc(display); 
   glutKeyboardFunc(keyboard);
   glutSpecialFunc(special);
   glutKeyboardUpFunc(keyboard_up);
   glutSpecialUpFunc(special_up);
   glutMouseFunc(mouse);
   glutMotionFunc(motion);
   glutPassiveMotionFunc(motion);

   glutIdleFunc(idle);

   initOpenGl();
   ImGui_ImplGlut_Init(); // initialize the imgui system

   //Enter the glut event loop.
   glutMainLoop();
   glutDestroyWindow(win);
   return 0;		
}


