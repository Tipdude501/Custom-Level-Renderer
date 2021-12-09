#pragma pack_matrix(row_major)

// an ultra simple hlsl vertex shader
// TODO: Part 1c
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

// TODO: Part 2b
[[vk::push_constant]]
cbuffer SHADER_VARS
{
    float4x4 world;
    float4x4 viewProjection;
};


// TODO: Part 2f, Part 3b


VERTEX_OUT main(VERTEX_IN inputVertex)
{
    // TODO: Part 2d
	// TODO: Part 2f, Part 3b
    VERTEX_OUT result;
    result.position = mul(inputVertex.position, world);
    result.position = mul(result.position, viewProjection);
    result.color = inputVertex.color;
    return result;
    
    //result.position = inputVertex.position;
    //result.position.x = world[0][0];
}