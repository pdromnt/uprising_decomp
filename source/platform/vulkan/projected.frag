#version 450
layout(set=0,binding=0) uniform sampler2D material;
layout(location=0) in vec4 vertex_color;
layout(location=1) in vec3 texture_coordinate;
layout(location=0) out vec4 output_color;
void main() {
    // Preserve projected s/w,t/w,1/w interpolation from the existing device.
    vec4 texel=texture(material,texture_coordinate.xy/texture_coordinate.z);
    output_color=vertex_color*texel;
    if(output_color.a==0.0) discard;
}
