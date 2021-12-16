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
//StructuredBuffer<MAT_ATTRIBUTES> materials;   //indexing offsets by the size of the templated type

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


//[[vk::binding(0, 0)]] //item in layout, which layout
//attribute specifier sequence (cpp 11)
struct VERTEX_IN
{
    float3 pos : POSITION;
    float3 uvw;
    float3 nrm : NORMAL;
    //int instanceId : SV_InstanceID;
};

struct VERTEX_OUT
{
    float4 posH : SV_POSITION;
    float3 nrmW : NORMAL;
    float3 posW : WORLD;
    float2 uv : TEXCOORD;
};

//int instanceId : SV_InstanceID;

VERTEX_OUT main(VERTEX_IN input, int instanceId : SV_InstanceID) : SV_POSITION
{
    VERTEX_OUT result;
    
    matrix world = transforms[transformOffset + instanceId];
    //matrix world = transforms[0];
    result.posW = mul(float4(input.pos, 1), world);
    result.posH = mul(float4(result.posW, 1), viewProjection);
    result.nrmW = mul(float4(input.nrm, 0), world);
    result.uv = float2(input.uvw[0], input.uvw[1]);
    
	return result;
}