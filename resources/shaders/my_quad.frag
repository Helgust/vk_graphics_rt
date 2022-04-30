#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_fragColor;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

vec3 exposure(vec3 color)
{
    return vec3(1.0) - exp(-color * 1);
}

void main()
{
  vec3 color = textureLod(colorTex, surf.texCoord, 0).rgb;
  color = exposure(color);
  out_fragColor = vec4(color, 1.0);
  //out_fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
