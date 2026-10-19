#version 450
layout(set=0,binding=0) uniform sampler2D image;
layout(set=1,binding=0) uniform sampler2D coverage;
layout(push_constant) uniform Material {
    vec2 extent;uint color_mode;uint alpha_mode;
    vec4 constant_color;vec4 fog_color;vec2 fog_range;
    uint fog_mode;uint alpha_function;float alpha_reference;uint chroma;
    uvec2 padding;vec4 mask_bounds;uint mask_enabled;uint texture_sampling;
} material;
layout(location=0) in vec4 vertex_color;
layout(location=1) in vec3 texture_coordinate;
layout(location=2) in float distance_to_camera;
layout(location=3) in vec4 previous_coordinate;
layout(location=0) out vec4 output_color;
layout(location=1) out float output_reciprocal_depth;
layout(location=2) out vec4 output_motion;
vec4 keyed_texel(ivec2 p) {
    ivec2 size=textureSize(image,0);
    p.x=(material.texture_sampling&2u)!=0u?(p.x%size.x+size.x)%size.x:clamp(p.x,0,size.x-1);
    p.y=(material.texture_sampling&4u)!=0u?(p.y%size.y+size.y)%size.y:clamp(p.y,0,size.y-1);
    vec4 value=texelFetch(image,p,0);
    uvec3 key=uvec3((material.chroma>>16)&255u,(material.chroma>>8)&255u,material.chroma&255u);
    if(all(equal(uvec3(round(value.rgb*255.0)),key)))value.a=0.0;
    return value;
}
vec4 sample_material(vec2 uv) {
    if(material.chroma==0u)return texture(image,uv);
    vec2 pixel=uv*vec2(textureSize(image,0));
    if((material.texture_sampling&1u)==0u)return keyed_texel(ivec2(floor(pixel)));
    vec2 f=fract(pixel-0.5);ivec2 p=ivec2(floor(pixel-0.5));
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
    output_reciprocal_depth=texture_coordinate.z;
    output_motion=vec4(0,0,0,1);
    if(previous_coordinate.w>=0.99999 && previous_coordinate.z>0.0)
        output_motion=vec4(previous_coordinate.xy/previous_coordinate.z-gl_FragCoord.xy,1,0);
}
