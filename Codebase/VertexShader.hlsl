#pragma pack_matrix(row_major)
struct VERTEX_IN
{
    float3 pos : POSITION;
    float3 uvw;
    float3 nrm : NORMAL;
};

struct VERTEX_OUT
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
    float2 uv : TEXCOORD;
};


[[vk::push_constant]]
cbuffer SHADER_VARS
{
    float4x4 world;
    float4x4 viewProjection;
};

VERTEX_OUT main(VERTEX_IN inputVertex) : SV_POSITION
{
    VERTEX_OUT result;
    result.posH = float4(inputVertex.pos, 1);
    result.posW = mul(result.posH, world);
    result.posH = mul(float4(result.posW, 1), viewProjection);
    return result;
}

/*
#pragma pack_matrix(row_major)
#define MAX_SUBMESH_PER_DRAW 1024

struct OBJ_ATTRIBUTES
{
    float3  Kd;         // diffuse reflectivity
    float   d;          // dissolve (transparency)
    float3  Ks;         // specular reflectivity
    float   Ns;         // specular exponent
    float3  Ka;         // ambient reflectivity
    float   sharpness;  // local reflection map sharpness
    float3  Tf;         // transmission filter
    float   Ni;         // optical density (index of refraction)
    float3  Ke;         // emissive reflectivity
	int     illum;      // illumination model
};

struct SHADER_VARIABLES
{
    float4 lightDirection;
    float4 lightColor;
    float4 ambientTerm;
    float4 cameraPosition;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 worldMatrix[MAX_SUBMESH_PER_DRAW];
    OBJ_ATTRIBUTES materials[MAX_SUBMESH_PER_DRAW];
};

StructuredBuffer<SHADER_VARIABLES> SceneData;

struct VERTEX_IN
{
    float3 pos : POSITION;
    float3 uvw;
    float3 nrm : NORMAL;
};

// TODO: Part 4b
struct VERTEX_OUT
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
    float2 uv : TEXCOORD;
};

[[vk::push_constant]]
cbuffer MESH_INDEX
{
    uint meshID;
};

VERTEX_OUT main(VERTEX_IN inputVertex) : SV_POSITION
{
    // TODO: Part 1h
    //return float4(inputVertex.pos + float3(0, -0.75f, 0.75f), 1);
	// TODO: Part 2i
	// TODO: Part 4b
    VERTEX_OUT result;
    
    result.posH = float4(inputVertex.pos, 1);
    result.posW = mul(result.posH, SceneData[0].worldMatrix[meshID]);
    result.posH = mul(float4(result.posW, 1), SceneData[0].viewMatrix);
    result.posH = mul(result.posH, SceneData[0].projectionMatrix);
    
    result.nrmW = mul(float4(inputVertex.nrm, 0), SceneData[0].worldMatrix[meshID]);
    result.uv = float2(inputVertex.uvw[0], inputVertex.uvw[1]);
	
    // TODO: Part 4e
	// TODO: Part 4e
    return result;
};

*/