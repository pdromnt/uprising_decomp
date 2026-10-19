#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec3 projective_uv;
layout(location=3) in float fog_distance;
layout(location=4) in vec4 previous_projection;
layout(push_constant) uniform Material {
    vec2 extent;uint color_mode;uint alpha_mode;
    vec4 constant_color;vec4 fog_color;vec2 fog_range;
    uint fog_mode;uint alpha_function;float alpha_reference;uint chroma;
    uvec2 padding;vec4 mask_bounds;uint mask_enabled;uint texture_sampling;
    vec2 jitter;float depth_scale;uint projection_padding;
} material;
layout(location=0) out vec4 vertex_color;
layout(location=1) out vec3 texture_coordinate;
layout(location=2) out float distance_to_camera;
layout(location=3) out vec4 previous_coordinate;
void main() {
    // Game vertices are already projected, with a top-left pixel origin.
    // Vulkan's positive-height viewport maps NDC -1 to the top edge.
    gl_Position=vec4((position.xy+material.jitter)/material.extent*2.0-1.0,position.z,1.0);
    gl_PointSize=1.0;
    vertex_color=color;
    texture_coordinate=projective_uv;
    distance_to_camera=fog_distance;
    previous_coordinate=previous_projection;
}
