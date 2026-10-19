#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec3 projective_uv;
layout(push_constant) uniform Frame { vec2 extent; } frame;
layout(location=0) out vec4 vertex_color;
layout(location=1) out vec3 texture_coordinate;
void main() {
    // Game vertices are already projected, with a top-left pixel origin.
    // Vulkan's positive-height viewport maps NDC -1 to the top edge.
    gl_Position=vec4(position.xy/frame.extent*2.0-1.0,position.z,1.0);
    vertex_color=color;
    texture_coordinate=projective_uv;
}
