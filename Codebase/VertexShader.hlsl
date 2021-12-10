#pragma pack_matrix(row_major)

struct VERTEX_IN
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct VERTEX_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};


[[vk::push_constant]]
cbuffer SHADER_VARS
{
    float4x4 world;
    float4x4 viewProjection;
};

VERTEX_OUT main(VERTEX_IN inputVertex)
{
    VERTEX_OUT result;
    result.position = mul(inputVertex.position, world);
    result.position = mul(result.position, viewProjection);
    result.color = inputVertex.color;
    return result;
}