#pragma pack_matrix(row_major)
struct MAT_ATTRIBUTES
{
    float3 Kd;          // diffuse reflectivity
    float d;            // dissolve (transparency)
    float3 Ks;          // specular reflectivity
    float Ns;           // specular exponent
    float3 Ka;          // ambient reflectivity
    float sharpness;    // local reflection map sharpness
    float3 Tf;          // transmission filter
    float Ni;           // optical density (index of refraction)
    float3 Ke;          // emissive reflectivity
    int illum;          // illumination model
};
[[vk::binding(1, 0)]]
StructuredBuffer<MAT_ATTRIBUTES> materials; //indexing offsets by the size of the templated type

[[vk::binding(0, 0)]]
StructuredBuffer<matrix> transforms; //indexing offsets by the size of the templated type

struct SCENE_DATA
{
    matrix viewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 ambientTerm;
    float4 cameraPosition;
};
[[vk::binding(2, 0)]]
StructuredBuffer<SCENE_DATA> sceneData;

[[vk::push_constant]]
cbuffer INSTANCE_DATA
{
    int transformOffset;
    int materialIndex;
};


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

VERTEX_OUT main(VERTEX_IN input, int instanceId : SV_InstanceID) : SV_POSITION
{
    VERTEX_OUT result;
    
    matrix world = transforms[transformOffset + instanceId];
    result.posW = mul(float4(input.pos, 1), world);
    result.posH = mul(float4(result.posW, 1), sceneData[0].viewProjection);
    result.nrmW = mul(float4(input.nrm, 0), world);
    result.uv = float2(input.uvw[0], input.uvw[1]);
    
	return result;
}