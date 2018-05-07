#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inTangent;
layout (location = 4) in vec3 inBiTangent;

layout (binding = 0) uniform UBO
{
	mat4 MVP;
	mat4 model;
	mat4 normal;
	vec4 lightPos;
} ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outLightVec;
layout (location = 2) out vec3 outLightVecB;
layout (location = 3) out vec3 outViewVec;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	outUV = inUV;
	gl_Position = ubo.MVP * vec4(inPos, 1.0);

	vec3 pos = vec3(ubo.model *  vec4(inPos, 1.0));
    // (t)angent-(b)inormal-(n)ormal matrix
    mat3 tbnMatrix;
    tbnMatrix[0] =  mat3(ubo.normal) * inTangent;
    tbnMatrix[1] =  mat3(ubo.normal) * inBiTangent;
    tbnMatrix[2] =  mat3(ubo.normal) * inNormal;

	outLightVec.xyz = vec3(ubo.lightPos.xyz - pos) * tbnMatrix;

	vec3 lightDist = ubo.lightPos.xyz - inPos;
	outLightVecB.x = dot(inTangent, lightDist);
	outLightVecB.y = dot(inBiTangent, lightDist);
	outLightVecB.z = dot(inNormal, lightDist);

	outViewVec.x = dot(inTangent, inPos);
	outViewVec.y = dot(inBiTangent, inPos);
	outViewVec.z = dot(inNormal, inPos);
}