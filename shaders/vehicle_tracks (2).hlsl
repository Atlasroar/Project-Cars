// Rigid skeletal shoes/rollers, retaining the native deferred object fragment shader.
#pragma pack_matrix(row_major)
void track_vs(float4 position : POSITION, float3 normal : NORMAL,
              float2 uv : TEXCOORD0, float4 tangent : TANGENT, uint4 blendIdx : BLENDINDICES,
              out float4 oPosition : SV_Position, out float2 oTexCoord : TEXCOORD0,
              out float3 oNormal : TEXCOORD1, out float3 oBinormal : TEXCOORD2,
              out float3 oTangent : TEXCOORD3, out float3 oCameraDir : TEXCOORD4,
              out float3 oWorldPos : TEXCOORD5, out float4 oColour : COLOR0,
              uniform float3x4 worldMatrix3x4Array[60], uniform float4x4 viewProjectionMatrix,
              uniform float4x4 worldMatrix, uniform float3 cameraDir, uniform float2 tiling) {
    float3x4 bone=worldMatrix3x4Array[(int)blendIdx.x];
    oWorldPos=mul(bone,position);
    oPosition=mul(viewProjectionMatrix,float4(oWorldPos,1));oTexCoord=uv*tiling;
    oNormal=normalize(mul((float3x3)bone,normal));
    oTangent=normalize(mul((float3x3)bone,tangent.xyz));
    oBinormal=normalize(cross(oTangent,oNormal))*tangent.w;
    oCameraDir=mul((float3x3)worldMatrix,cameraDir);oColour=float4(1,1,1,1);
}
