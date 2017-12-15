void main(
    in  float2 inPosition  : POSITION,
    in  float3 inColor     : COLOR,
    out float4 outPosition : SV_POSITION,
    out float3 outColor    : COLOR)
{
    outPosition = float4(inPosition.xy, 0.0, 1.0);
    outColor    = inColor;
}
