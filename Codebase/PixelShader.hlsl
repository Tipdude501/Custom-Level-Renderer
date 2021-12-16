#pragma pack_matrix(row_major)
//#define MAX_INSTANCE_PER_DRAW 1024
//[[vk::binding(0, 0)]]
//cbuffer INSTANCE_SHADER_DATA
//{
//    int materialIndex;
//    int textureIndex;
//    int padding[14];
//    matrix viewProjection;
//    matrix matrices[MAX_INSTANCE_PER_DRAW];
//};

//#define MAX_INSTANCE_PER_DRAW 1024
//struct SHADER_VARIABLES
//{
//    int materialIndex;
//    int textureIndex;
//    int padding[14];
//    matrix viewProjection;
//    matrix matrices[MAX_INSTANCE_PER_DRAW];
//};
//StructuredBuffer<SHADER_VARIABLES> sv;

[[vk::binding(0, 0)]]
StructuredBuffer<matrix> transforms;

[[vk::push_constant]]
cbuffer INSTANCE_DATA
{
    matrix viewProjection;
    int transformOffset;
    int materialIndex;
    //float4 lightDirection;
    //float4 lightColor2;
    //float4 ambientTerm2;
    //float4 cameraPosition;
};

struct VERTEX_IN
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
    float2 uv : TEXCOORD;
};

float4 main(VERTEX_IN input) : SV_TARGET
{
    float4 ambientTerm = { 0.25f, 0.25f, 0.35f };
    float4 lightDirection = { -1.0f, -1.0f, 2.0f };
    float4 lightColor = { 0.9f, 0.9f, 1.0f, 1.0f };
	
    float4 finalColor = 0;
    input.nrmW = normalize(input.nrmW);
	
    // TODO: Part 4c
    finalColor = saturate(dot(-lightDirection, float4(input.nrmW, 0)));
    finalColor = saturate(finalColor + ambientTerm);
    finalColor *= lightColor; 
    
    return finalColor;
}