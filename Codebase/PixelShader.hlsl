#pragma pack_matrix(row_major)
struct MAT_ATTRIBUTES
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
[[vk::binding(1, 0)]]
StructuredBuffer<MAT_ATTRIBUTES> materials; //indexing offsets by the size of the templated type

[[vk::binding(0, 0)]]
StructuredBuffer<matrix> transforms; //indexing offsets by the size of the templated type

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