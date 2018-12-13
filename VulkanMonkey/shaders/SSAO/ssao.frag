#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set = 0, binding = 0) uniform sampler2D samplerDepth;
layout (set = 0, binding = 1) uniform sampler2D samplerNormal;
layout (set = 0, binding = 2) uniform sampler2D samplerNoise;
layout (set = 0, binding = 3) uniform UniformBufferObject { vec4 samples[64]; } kernel;
layout (set = 0, binding = 4) uniform UniformBufferPVM { mat4 projection; mat4 view; vec4 size; } pvm;
layout(push_constant) uniform Position { vec4 offset; } pos;


layout (location = 0) in vec2 inUV;

layout (location = 0) out float outColor;

const int KERNEL_SIZE = 8;
const float RADIUS = 0.5f;
const float bias = 0;

vec3 getViewPosFromDepth(vec2 UV, float depth)
{
	vec2 revertedUV = (UV - pos.offset.xy) / pos.offset.zw; // floating window correction
	vec4 ndcPos;
	ndcPos.xy = revertedUV * 2.0 - 1.0;
	ndcPos.z = depth;
	ndcPos.w = 1.0;
	
	vec4 clipPos = inverse(pvm.projection) * ndcPos;
	return (clipPos / clipPos.w).xyz;
}

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = getViewPosFromDepth(inUV, texture(samplerDepth, inUV).x);
	vec4 normal = pvm.view * texture(samplerNormal, inUV);

	// Get a random vector using a noise lookup
	ivec2 texDim = textureSize(samplerDepth, 0); 
	ivec2 noiseDim = textureSize(samplerNoise, 0);
	const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * inUV;  
	vec3 randomVec = texture(samplerNoise, noiseUV).xyz * 2.0 - 1.0;
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal.xyz * dot(randomVec, normal.xyz));
	vec3 bitangent = cross(normal.xyz, tangent);
	mat3 TBN = mat3(tangent, bitangent, normal.xyz);

	// Calculate occlusion value
	float occlusion = 0.0f;
	for(int i = 0; i < KERNEL_SIZE; i++)
	{
		vec3 direction = TBN * kernel.samples[i].xyz * RADIUS;
		vec4 newViewPos = vec4(fragPos + direction, 1.0);
		vec4 samplePosition = pvm.projection * newViewPos;
		samplePosition.xy /= samplePosition.w;
		samplePosition.xy = samplePosition.xy * 0.5f + 0.5f;
		samplePosition.xy *= pos.offset.zw; // floating window size correction
		samplePosition.xy += pos.offset.xy; // floating window position correction
		
		float currentDepth = newViewPos.z;
		float sampledDepth = getViewPosFromDepth(inUV, texture(samplerDepth, samplePosition.xy).x).z;

		// Range check

		float rangeCheck = smoothstep(0.0f, 1.0f, RADIUS / abs(currentDepth - sampledDepth));
		occlusion += (sampledDepth >= currentDepth - bias ? 0.0f : 1.0f) * rangeCheck;           
	}
	occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));
	
	outColor = occlusion;
}