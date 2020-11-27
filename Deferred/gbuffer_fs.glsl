#version 430

layout(binding = 6) uniform sampler2D diffuse_map;

layout(location = 0) out vec4 p;     
layout(location = 1) out vec4 n;
layout(location = 2) out vec4 diffuse_color;

in vec2 tex_coord;
in vec3 normal;		//eye-space normal
in vec3 position;	//eye-space fragment position

void main(void)
{   
	p = vec4(position, 1.0);
	n = vec4(normalize(normal), 1.0);
	diffuse_color = texture(diffuse_map, tex_coord);
}

