#version 430

layout(binding = 3) uniform sampler2D ssaoTex;

out vec4 fragcolor;           
in vec2 tex_coord;

void main(void)
{   
	vec4 blurred = vec4(0.0);
	ivec2 texelCoord = ivec2(gl_FragCoord.xy);

	//Basic box filter (mean filter) of ssaoTex

	const int w = 3;		//filter kernel width
	const int hw = w/2;		//filter kernel half-width
	for(int i=-hw; i<=hw; i++)
	{
		for(int j=-hw; j<=hw; j++)
		{
			blurred += texelFetch(ssaoTex, texelCoord + ivec2(i,j), 0);
		}	
	}

	fragcolor = blurred/float(w*w);
}




