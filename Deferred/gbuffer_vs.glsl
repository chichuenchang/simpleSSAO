#version 430            

layout(location = 1) uniform mat4 V;
layout(location = 2) uniform mat4 M;
layout(location = 3) uniform mat4 PVM;

layout(location = 0) in vec3 pos_attrib;
layout(location = 1) in vec2 tex_coord_attrib;
layout(location = 2) in vec3 normal_attrib;		//object space normal

out vec2 tex_coord; 
out vec3 normal; //eye-space
out vec3 position; //eye-space

void main(void)
{
   gl_Position = PVM*vec4(pos_attrib, 1.0);
   tex_coord = tex_coord_attrib;

   //Compute eye-space normal
   normal = vec3(normalize(V*M*vec4(normal_attrib, 0.0))); 

   //Compute world-space vertex position
   position = vec3(V*M*vec4(pos_attrib, 1.0));        
}