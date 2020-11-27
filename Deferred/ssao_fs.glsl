#version 430

layout(location = 0) uniform mat4 P;
layout(location = 15) uniform vec3 ssaoKernel[64];
layout(location = 80) uniform int ssaoNumSamples = 64;
layout(location = 81) uniform float radius = 0.25;
layout(location = 82) uniform float bias = 0.025;

layout(binding = 0) uniform sampler2D pos_gbuffer;
layout(binding = 1) uniform sampler2D normal_gbuffer;
layout(binding = 5) uniform sampler2D ssaoTangentTex;

const float gamma = 1.2;

out vec4 fragcolor;  

in vec2 tex_coord;

float calcAo();

void main(void)
{   
	float ao = pow(calcAo(), gamma);
	fragcolor = vec4(ao, ao, ao, 1.0);
}

float calcAo()
{
	ivec2 texelCoord = ivec2(gl_FragCoord.xy);
	ivec2 size = textureSize(ssaoTangentTex, 0);
	vec3 fragPos   = texelFetch(pos_gbuffer, texelCoord, 0).xyz;
	vec3 normal    = texelFetch(normal_gbuffer, texelCoord, 0).xyz;

	//Repeat randomVec texture over screen-space
	vec3 randomVec = texelFetch(ssaoTangentTex, ivec2(texelCoord.x%size.x, texelCoord.y%size.y), 0).xyz;
	
	//subtract normal component from randomVec to obtain tangent
	vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN       = mat3(tangent, bitangent, normal);

	float occlusion = 0.0;
	for(int i = 0; i < ssaoNumSamples; ++i)
	{
		// get sample position
		vec3 s = TBN * ssaoKernel[i]; // From tangent to view-space
		s = fragPos + s * radius; 
    
		vec4 offset = vec4(s, 1.0);
		offset      = P * offset;    // from view to clip-space
		offset.xyz /= offset.w;               // perspective divide
		offset.xyz  = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0 

		float sceneDepth = texture(pos_gbuffer, offset.xy).z;

		float rangeCheck = smoothstep(0.0, abs(fragPos.z - sceneDepth),  radius);
 
		occlusion       += step(s.z + bias, sceneDepth) * rangeCheck;
	}  
	occlusion = 1.0 - (occlusion / ssaoNumSamples);
	return occlusion;
}