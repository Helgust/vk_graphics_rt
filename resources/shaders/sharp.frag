#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D colorTexture;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

void main()
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

  //out_color = vec4(color, texture(colorTexture, texCoord).a);
  out_color = texture(colorTexture, texCoord);

}