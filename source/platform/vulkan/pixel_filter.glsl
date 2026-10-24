// Bilinear sampling of an integer-prescaled image, without allocating it.
// Exact integer scales keep solid pixels; fractional boundaries interpolate.
vec2 pixel_filter_coord(vec2 p,vec2 scale){
    vec2 n=max(floor(scale+0.0001),vec2(1)),d=fract(p)-0.5;
    return floor(p)+0.5+sign(d)*max(abs(d)-(0.5-0.5/n),vec2(0))*n;
}
vec4 pixel_filter_sample(sampler2D source,vec2 p,vec2 scale){
    ivec2 size=textureSize(source,0);
    vec2 point=pixel_filter_coord(p,scale)-0.5,f=fract(point);
    ivec2 b=ivec2(floor(point));
    return mix(mix(texelFetch(source,clamp(b,ivec2(0),size-1),0),
                   texelFetch(source,clamp(b+ivec2(1,0),ivec2(0),size-1),0),f.x),
               mix(texelFetch(source,clamp(b+ivec2(0,1),ivec2(0),size-1),0),
                   texelFetch(source,clamp(b+ivec2(1,1),ivec2(0),size-1),0),f.x),f.y);
}
