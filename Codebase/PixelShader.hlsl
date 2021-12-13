#pragma pack_matrix(row_major)
#define MAX_INSTANCE_PER_DRAW 1024
struct MATERIAL
{
    float3 Kd; // diffuse reflectivity
    float d; // dissolve (transparency)
    float3 Ks; // specular reflectivity
    float Ns; // specular exponent
    float3 Ka; // ambient reflectivity
    float sharpness; // local reflection map sharpness
    float3 Tf; // transmission filter
    float Ni; // optical density (index of refraction)
    float3 Ke; // emissive reflectivity
    int illum; // illumination model
};
struct SHADER_VARIABLES
{
    float4 lightDirection;
    float4 lightColor;
    float4 ambientTerm;
    float4 cameraPosition;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 worldMatrix[MAX_INSTANCE_PER_DRAW];
    MATERIAL materials[MAX_INSTANCE_PER_DRAW];
};
//StructuredBuffer<SHADER_VARIABLES> SceneData;

struct VERTEX_IN
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
    float2 uv : TEXCOORD;
};

float4 main(VERTEX_IN input) : SV_TARGET
{
    //return float4(0.5f, 0.5f, 0.75f, 0); 
   
    float4 finalColor = 1;
    float4 ambientTerm = { 0.25f, 0.25f, 0.35f };
    float4 lightDirection = { -1.0f, -1.0f, 2.0f };
    float4 lightColor = { 0.9f, 0.9f, 1.0f, 1.0f };
        
    input.nrmW = normalize(input.nrmW);
	
    // TODO: Part 4c
    finalColor = saturate(dot(-lightDirection, float4(input.nrmW, 0)));
    finalColor = saturate(finalColor + ambientTerm);
    finalColor *= lightColor * 0.5f;
    
    return finalColor;
}


/*
// TODO: Part 3e
[[vk::push_constant]]
cbuffer MESH_INDEX
{
    uint meshID;
};
// an ultra simple hlsl pixel shader
// TODO: Part 4b
struct VERTEX_IN
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
    float2 uv : TEXCOORD;
};

float4 main(VERTEX_IN input) : SV_TARGET
{
    float4 finalColor = 0;
    input.nrmW = normalize(input.nrmW);
	
    // TODO: Part 4c
    finalColor = saturate(dot(-SceneData[0].lightDirection, float4(input.nrmW, 0)));
    finalColor = saturate(finalColor + SceneData[0].ambientTerm);
    finalColor *= SceneData[0].lightColor * float4(SceneData[0].materials[meshID].Kd, 1);
	
    // TODO: Part 4g (half-vector or reflect method your choice)
    float3 viewDir = normalize((float3) SceneData[0].cameraPosition - input.posW);
    float3 halfVec = normalize((float3) (-SceneData[0].lightDirection) + viewDir);
    float intensity =
    max(
        pow(
            saturate(dot(input.nrmW, halfVec)),
            SceneData[0].materials[meshID].Ns
        ),
        0
    );
    float3 reflectedLight = (float3) SceneData[0].lightColor * SceneData[0].materials[meshID].Ks * intensity;
    finalColor += float4(reflectedLight, 1);
    
    return finalColor;
}
*/