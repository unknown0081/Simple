#include "../RegisterNum.h"
#include "ShadowmapFunctions.hlsli"

struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD;
    float4 boneWeights : WEIGHTS;
    uint4 boneIndices : BONES;
};

// BONE の最大数
static const int MAX_BONES = 256;

// 3d 空間に出す全てのモデルが持つ
cbuffer ObjectConstant : register(_objectConstant)
{
    row_major float4x4 world;
    float4 materialColorKd;
    float4 materialColorKs;
    float4 materialColorKa;
    row_major float4x4 boneTransforms[MAX_BONES];
}

// 影の情報
cbuffer ShadowConstant : register(_shadowConstant)
{
    row_major float4x4 lightViewProjection;
}