#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#include "colorSpaces.inc.glsl"
layout(location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D colorTexture;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

vec3 sharpness_filter()
{
  float amount = 0.1f;
  float neighbor = amount * -1.0f;
  float center   = amount *  4.0f + 1.0f;
  vec2 texSize   = textureSize(colorTexture, 0).xy;
  vec2 texCoord  = gl_FragCoord.xy / texSize;
  vec3 color =
        texture(colorTexture, (texCoord + vec2( 0,  1) / texSize)).rgb
      * neighbor
      + texture(colorTexture, (texCoord + vec2(-1,  0) / texSize)).rgb
      * neighbor
      + texture(colorTexture, (texCoord + vec2( 0,  0) / texSize)).rgb
      * center
      + texture(colorTexture, (texCoord + vec2( 1,  0) / texSize)).rgb
      * neighbor
      + texture(colorTexture, (texCoord + vec2( 0, -1)/ texSize)).rgb
      * neighbor;
      
  return color;
}

vec3 uncharted2Tonemap(vec3 x) {
  float A = 0.15;
  float B = 0.50;
  float C = 0.10;
  float D = 0.20;
  float E = 0.02;
  float F = 0.30;
  float W = 11.2;
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 uncharted2(vec3 color) {
  const float W = 11.2;
  float exposureBias = 2.0;
  vec3 curr = uncharted2Tonemap(exposureBias * color);
  vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
  return rgb_to_srgb(curr * whiteScale);
}

void main()
{
  
  //vec3 result_color =  sharpness_filter();
  vec2 texSize   = textureSize(colorTexture, 0).xy;
  vec2 texCoord  = gl_FragCoord.xy / texSize;
  vec4 result_color =  texture(colorTexture, texCoord);

  out_color = vec4(uncharted2(result_color.xyz),result_color.w);

}