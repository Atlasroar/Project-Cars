// Matches Kenshi's Basic_Coloured_Texture_VP output. UV.z stores birth time,
// UV.w selects wheel (0) or belt (1); fading is independent of geometry updates.
struct TrailInput {float4 position:SV_Position;float4 colour:COLOR;float4 uv:TEXCOORD0;};
float4 tread_fp(TrailInput i,uniform float trailTime):SV_Target {
    float age=max(0,trailTime-i.uv.z);
    float life=1-smoothstep(30,90,age);
    float u=i.uv.x, v=i.uv.y;
    float edge=smoothstep(0,.10,u)*(1-smoothstep(.90,1,u));
    float aa=max(fwidth(v),.035);
    float wheelPhase=abs(frac(v+abs(u-.5)*1.15)-.5);
    float wheel=1-smoothstep(.20-aa,.20+aa,wheelPhase);
    wheel*=smoothstep(.025,.055,abs(u-.5));
    float barPhase=abs(frac(v)-.5);
    float bars=1-smoothstep(.15-aa,.15+aa,barPhase);
    float rails=smoothstep(.29,.34,abs(u-.5))*(1-smoothstep(.42,.47,abs(u-.5)));
    float belt=max(bars,rails*.6);
    float pattern=lerp(wheel,belt,step(.5,i.uv.w));
    // A faint disturbed-ground strip connects the deeper tread impressions.
    float opacity=edge*(.045+.28*pattern)*i.colour.a*life;
    // ZERO / ONE_MINUS_SOURCE_COLOUR absorbs light without emitting it.
    // Clean prints preserve the old neutral darkening; blood absorbs more
    // green/blue than red. The per-segment coating never retints old marks.
    float3 absorption=lerp(float3(1,1,1),float3(.65,2.8,2.65),saturate(i.colour.r));
    return float4(saturate(opacity*absorption),opacity);
}
