#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : enable

#include "common.h"

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams UboParams;
};

layout (binding = 1, set = 0) uniform sampler2D colorTex;
layout (binding = 2, set = 0) uniform sampler2D oldColorTex;
layout (binding = 3, set = 0) uniform sampler2D depthTex;
layout (binding = 4, set = 0) uniform sampler2D oldDepthTex;
layout (binding = 5, set = 0) uniform sampler2D velocityTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

vec2 iResolution = vec2(1024,1024);
void main()
{
  if (UboParams.settings.x == 1)
  {
    float weight = 0.97;
    vec3 minColor = vec3(9999.0), maxColor = vec3(-9999.0);
    vec2 velocityUV = textureLod(velocityTex,surf.texCoord,0).xy;
    vec2 reprojectedUV = surf.texCoord + velocityUV;
    vec3 currentFrame = textureLod(colorTex,surf.texCoord,0).xyz;
    vec3 prevFrame = textureLod(oldColorTex,reprojectedUV,0).xyz;
    
    for(int x = -1; x <= 1; ++x)
    {
      for(int y = -1; y <= 1; ++y)
      {   
        vec3 color = textureLod(colorTex,surf.texCoord + vec2(x, y)/1024.0f,0).xyz;
        minColor = min(minColor, color); // Take min and max
        maxColor = max(maxColor, color);
      }
    }
    // Clamp previous color to min/max bounding box
    vec3 previousColorClamped = clamp(prevFrame, minColor, maxColor);
    weight *= max(1.0 - length(velocityUV) / 10.0, 0.0) ;
    if (reprojectedUV.x < 0.0 || reprojectedUV.y < 0.0 || reprojectedUV.x > 1 || reprojectedUV.y > 1) {
        weight = 0.0;
    }
    vec3 c = mix(currentFrame, previousColorClamped, weight);
    //vec3 c = mix(prevFrame,currentFrame,mix(0.05,0.6,0));
    outColor = vec4(c,1.0);
  }
  else
  {
    vec3 currentFrame = textureLod(colorTex,surf.texCoord,0).xyz;
    outColor = vec4(currentFrame,1.0);
  }
  
}
