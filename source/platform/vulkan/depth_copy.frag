#version 450
layout(set=0,binding=0) uniform sampler2D world_depth;
layout(push_constant) uniform View {vec2 extent;vec2 jitter;} view;
void main(){
    ivec2 size=textureSize(world_depth,0);
    // The world is jittered; late billboards use unjittered screen coordinates.
    ivec2 p=ivec2(floor(gl_FragCoord.xy*vec2(size)/view.extent+view.jitter));
    gl_FragDepth=texelFetch(world_depth,clamp(p,ivec2(0),size-1),0).r;
}
