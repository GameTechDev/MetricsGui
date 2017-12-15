void main(
    in  float4 inPosition : Sv_Position,
    in  float3 inColor    : COLOR,
    out float4 outColor   : Sv_Target0)
{
    outColor = float4(inColor.xyz, 1.0);
}
