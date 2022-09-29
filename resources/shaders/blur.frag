#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;


float Pi = 6.28318530718; // Pi*2

// GAUSSIAN BLUR SETTINGS {{{
float Directions = 4.0; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
float Quality = 3.0; // BLUR QUALITY (Default 4.0 - More is better but slower)
float Size = 8.0; // BLUR SIZE (Radius)
// GAUSSIAN BLUR SETTINGS }}}

void main()
{

  vec2 resolution = vec2(1024, 1024);
  vec2 Radius = Size/resolution.xy;
  vec2 uv = surf.texCoord;
  // Blur calculations
  for( float d=0.0; d<Pi; d+=Pi/Directions)
  {
    for(float i=1.0/Quality; i<=1.0; i+=1.0/Quality)
    {
      color = textureLod(colorTex, uv+vec2(cos(d),sin(d))*Radius*i, 0);
    }
  }
  color /= Quality * Directions;
}