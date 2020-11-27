#version 430    

layout(location = 3) uniform mat4 PVM;

const int AMBIENT_SRC = 0;
const int POINT_SRC = 1;

layout(location = 84) uniform int source = AMBIENT_SRC;

out vec2 tex_coord; 


const vec4 quad_pos[4] = vec4[] ( vec4(-1.0, 1.0, 0.0, 1.0), vec4(-1.0, -1.0, 0.0, 1.0), vec4( 1.0, 1.0, 0.0, 1.0), vec4( 1.0, -1.0, 0.0, 1.0) );
const vec2 quad_tex[4] = vec2[] ( vec2(0.0, 1.0), vec2(0.0, 0.0), vec2( 1.0, 1.0), vec2( 1.0, 0.0) );

layout(location = 0) in vec3 pos_attrib;

void main(void)
{
	//Full-screen quad for ambient
	if(source == AMBIENT_SRC)
	{
		gl_Position = quad_pos[ gl_VertexID ]; 
		tex_coord = quad_tex[ gl_VertexID ];
	}
	else //source == POINT_SRC
	{
		gl_Position = PVM*vec4(pos_attrib, 1.0);
	}
}