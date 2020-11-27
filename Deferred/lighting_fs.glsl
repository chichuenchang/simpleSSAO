#version 430

layout(location = 10) uniform int mode;
layout(location = 86) uniform vec3 lightPos;

layout(binding = 0) uniform sampler2D pos_gbuffer;
layout(binding = 1) uniform sampler2D normal_gbuffer;
layout(binding = 2) uniform sampler2D diffuse_gbuffer;
layout(binding = 3) uniform sampler2D ssaoTex0; //unfiltered ssao
layout(binding = 4) uniform sampler2D ssaoTex;
layout(binding = 7) uniform sampler2D depthTex;

const int AMBIENT_SRC = 0;
const int POINT_SRC = 1;

layout(location = 84) uniform int source = AMBIENT_SRC;

out vec4 fragcolor;           
in vec2 tex_coord;

//light and material colors
const vec4 La = vec4(0.75);
const vec4 kd = vec4(0.35);
const vec4 ks = vec4(0.35);

vec4 ambient_light();
vec4 point_light();

//mode = 0: draw ssaoTex0 (unfiltered SSAO)
//mode = 1: draw ssaoTex (filtered SSAO)
//mode = 2: lighting

void main(void)
{   
	//Since blitting the depth buffer didn't work, just assign it here.
	//We want light volumes to be depth-tested against scene depth.
	float d = texelFetch(depthTex, ivec2(gl_FragCoord), 0).r;
	gl_FragDepth = d;

	if(source == AMBIENT_SRC)
	{
		if(mode == 0)
		{
			fragcolor = texelFetch(ssaoTex0, ivec2(gl_FragCoord), 0);
			return;
		}
		else if (mode == 1)
		{
			fragcolor = texelFetch(ssaoTex, ivec2(gl_FragCoord), 0);
			return;
		}
		else
		{
			fragcolor = ambient_light();
			return;
		}
	}

	if(source == POINT_SRC)
	{
		//Do our own depth testing of light volumes here.
		if(gl_FragCoord.z > d)
		{
			discard;
		}
		if(mode == 2) //lighting
		{
			fragcolor = point_light();	
			return;
		}
		else
		{
			fragcolor = vec4(0.0);
			return;
		}
	}
}

vec4 ambient_light()
{
	ivec2 texelCoord = ivec2(gl_FragCoord.xy);
	float ao = texelFetch(ssaoTex, texelCoord, 0).r;
	vec4 amb_color = texelFetch(diffuse_gbuffer, texelCoord, 0);

	//ambient term
	vec4 amb = ao*La*amb_color;

	return vec4(amb.rgb, 1.0);
}

vec4 point_light()
{
	//Make some interesting light color here. In reality, send light color as a uniform var.
	vec4 Ld = vec4(0.5*sin(10.0*lightPos)+vec3(0.5), 1.0);

	const vec4 Ls = 0.5*Ld;

	const vec3 eye = vec3(0.0); //eye position is origin of camera space
	

	ivec2 texelCoord = ivec2(gl_FragCoord.xy);
	vec3 p = texelFetch(pos_gbuffer, texelCoord, 0).xyz;
	vec3 n = texelFetch(normal_gbuffer, texelCoord, 0).xyz; // unit normal vector

	float d = distance(lightPos, p);
	vec3 l = normalize(lightPos-p);
	vec3 v = normalize(eye-p); // unit view vector
	vec3 r = reflect(-l, n); // unit reflection vector

	//compute phong lighting in eye space
	float ao = texelFetch(ssaoTex, texelCoord, 0).r;

	//diffuse term
	vec4 diff = Ld*max(dot(n,l), 0.0);
	vec4 diff_color = texelFetch(diffuse_gbuffer, texelCoord, 0);
	diff = ao*diff_color*diff;

	//specular term 
	vec4 spec = ks*Ls*pow(max(0.0, dot(r, v)), 150.0);
	
	return (diff+spec)/(20.0*d*d); //HACK: fake attenuation. Won't really it light volumes
}
