#version 450
#extension GL_GOOGLE_include_directive : require
#include "pixel_filter.glsl"
layout(set=0,binding=0) uniform sampler2D image;
layout(set=1,binding=0) uniform sampler2D coverage;
layout(push_constant) uniform Material {
    vec2 extent;uint color_mode;uint alpha_mode;
    vec4 constant_color;vec4 fog_color;vec2 fog_range;
    uint fog_mode;uint alpha_function;float alpha_reference;uint chroma;
    uvec2 padding;vec4 mask_bounds;uint mask_enabled;uint texture_sampling;
    vec2 jitter;float depth_scale;uint projection_padding;
} material;
layout(location=0) in vec4 vertex_color;
layout(location=1) in vec3 texture_coordinate;
layout(location=2) in float distance_to_camera;
layout(location=3) in vec4 previous_coordinate;
layout(location=0) out vec4 output_color;
layout(location=1) out float output_reciprocal_depth;
layout(location=2) out vec4 output_motion;
layout(location=3) out vec4 output_scene;
vec4 keyed_texel(ivec2 p) {
    ivec2 size=textureSize(image,0),origin=ivec2(0);
    if(material.padding.y!=0u){
        uint tile=material.padding.y-1u;
        origin=ivec2(tile&31u,tile>>5u)*32;size=ivec2(32);p-=origin;
    }
    p.x=(material.texture_sampling&2u)!=0u?(p.x%size.x+size.x)%size.x:clamp(p.x,0,size.x-1);
    p.y=(material.texture_sampling&4u)!=0u?(p.y%size.y+size.y)%size.y:clamp(p.y,0,size.y-1);
    vec4 value=texelFetch(image,p+origin,0);
    uvec3 key=uvec3((material.chroma>>16)&255u,(material.chroma>>8)&255u,material.chroma&255u);
    if(material.chroma!=0u && all(equal(uvec3(round(value.rgb*255.0)),key)))value.a=0.0;
    return value;
}
vec4 sample_material(vec2 uv) {
    if((material.texture_sampling&8u)!=0u){
        vec2 pixel=uv*vec2(textureSize(image,0));
        vec2 step_size=max(abs(dFdx(pixel)),abs(dFdy(pixel)));
        pixel=pixel_filter_coord(pixel,1.0/max(step_size,vec2(0.00001)))-0.5;
        ivec2 p=ivec2(floor(pixel));vec2 f=fract(pixel);
        vec4 a=keyed_texel(p),b=keyed_texel(p+ivec2(1,0));
        vec4 c=keyed_texel(p+ivec2(0,1)),d=keyed_texel(p+ivec2(1,1));
        // Filter coverage and premultiplied ink together, so transparent
        // padding/chroma colors cannot tint or darken the glyph's edges.
        a.rgb*=a.a;b.rgb*=b.a;c.rgb*=c.a;d.rgb*=d.a;
        vec4 result=mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
        if(result.a>0.0)result.rgb/=result.a;
        return result;
    }
    if(material.chroma==0u && material.padding.y==0u)return texture(image,uv);
    vec2 pixel=uv*vec2(textureSize(image,0));
    if(material.chroma==0u){
        uint tile=material.padding.y-1u;
        vec2 local=pixel-vec2(uvec2(tile&31u,tile>>5u)*32u);
        // Hardware filtering is safe inside the tile; only its boundary needs
        // explicit taps to repeat/clamp within this original32x32 texture.
        if(all(greaterThanEqual(local,vec2(.5))) && all(lessThanEqual(local,vec2(31.5))))return texture(image,uv);
    }
    if((material.texture_sampling&1u)==0u)return keyed_texel(ivec2(floor(pixel)));
    vec2 f=fract(pixel-0.5);ivec2 p=ivec2(floor(pixel-0.5));
    // Projective interpolation/atlas translation can leave a few float ULPs at
    // an exact texel center. Do not turn a transparent edge into visible ink.
    f=mix(f,vec2(0),lessThan(f,vec2(1.0/4096.0)));
    f=mix(f,vec2(1),greaterThan(f,vec2(1.0-1.0/4096.0)));
    return mix(mix(keyed_texel(p),keyed_texel(p+ivec2(1,0)),f.x),
               mix(keyed_texel(p+ivec2(0,1)),keyed_texel(p+ivec2(1,1)),f.x),f.y);
}
void main() {
    if(material.mask_enabled!=0){
        vec2 uv=(gl_FragCoord.xy-material.mask_bounds.xy)/(material.mask_bounds.zw-material.mask_bounds.xy);
        if(any(lessThan(uv,vec2(0))) || any(greaterThan(uv,vec2(1))) || texture(coverage,vec2(uv.x,1.0-uv.y)).a<0.5)discard;
    }
    // Preserve projected s/w,t/w,1/w interpolation from the existing device.
    vec4 texel=sample_material(texture_coordinate.xy/texture_coordinate.z);
    vec3 rgb=vertex_color.rgb;
    if(material.color_mode==1)rgb=material.constant_color.rgb;
    if(material.color_mode==2)rgb=texel.rgb;
    if(material.color_mode==3)rgb=texel.rgb*(1.0-vertex_color.rgb);
    if(material.color_mode==4)rgb=texel.rgb*vertex_color.rgb;
    float alpha=vertex_color.a;
    if(material.alpha_mode==0)alpha=0.0;
    if(material.alpha_mode==2)alpha=texel.a;
    if(material.alpha_mode==3)alpha=material.constant_color.a;
    if(material.alpha_mode==4)alpha*=texel.a;
    if(material.chroma!=0){alpha=texel.a;if(alpha<=0.0)discard;}
    float reference=material.alpha_reference;uint test=material.alpha_function;
    bool accepted=test==7 || (test==1 && alpha<reference) || (test==2 && alpha==reference) ||
        (test==3 && alpha<=reference) || (test==4 && alpha>reference) ||
        (test==5 && alpha!=reference) || (test==6 && alpha>=reference);
    if(!accepted)discard;
    if(material.fog_mode!=0){
        float visibility=material.fog_mode==1?
            clamp((material.fog_range.y-distance_to_camera)/(material.fog_range.y-material.fog_range.x),0.0,1.0):
            exp(-pow(material.fog_range.x*distance_to_camera,2.0));
        rgb=mix(material.fog_color.rgb,rgb,visibility);
    }
    output_color=vec4(rgb,alpha);
    output_scene=output_color;
    output_reciprocal_depth=(material.padding.x&4u)!=0u?0.0:clamp(texture_coordinate.z*material.depth_scale,0.0,1.0);
    output_motion=vec4(0,0,0,1);
    if(previous_coordinate.w>=0.99999 && previous_coordinate.z>0.0)
        output_motion=vec4(previous_coordinate.xy/previous_coordinate.z-(gl_FragCoord.xy-material.jitter),1,0);
    // The game leaves blending enabled for opaque terrain too. Only actual
    // translucency rejects valid solid history. A no-write, zero-alpha blended
    // effect contributes no color and must preserve the underlying history.
    if((material.padding.x&1u)!=0u)output_motion.w=(material.padding.x&2u)!=0u && alpha<=0.0?0.0:1.0;
    else if((material.padding.x&2u)!=0u && alpha<0.99999)output_motion.w=1.0;
}
