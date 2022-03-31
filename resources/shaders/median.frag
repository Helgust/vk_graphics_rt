#version 450
#extension GL_ARB_separate_shader_objects : enable
#define KERNEL 3

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;


float R[KERNEL * KERNEL];
float G[KERNEL * KERNEL];
float B[KERNEL * KERNEL];


void sort()
{
  int n = KERNEL * KERNEL;
  for (int i = 0; i < n -1; ++i)
  {
    //   Find position of minimum element
    int minR = i;
    int minG = i;
    int minB = i;

    float temp;
    for (int j = i + 1; j < n; ++j)
    {
      if (R[j] < R[minR])
      {
        minR = j;
      }
      if (G[j] < G[minG])
      {
        minG = j;
      }
      if (B[j] < B[minB])
      {
        minB = j;
      }

    }
    //   Put found minimum element in its place
    temp = R[i];
    R[i] = R[minR];
    R[minR] = temp;

    temp = G[i];
    G[i] = G[minG];
    G[minG] = temp;

    temp = B[i];
    B[i] = B[minB];
    B[minB] = temp;
  }
}


vec4 median(vec2 x)
{
  int s = (KERNEL - 1) / 2;
  vec2 step = 1.0 / textureSize(colorTex, 0);
  vec4 texel;
  vec4 result = vec4(0);

  for (int i = 0; i < KERNEL; ++i)
  {
    for (int j = 0; j < KERNEL; ++j)
    {
      vec2 x_ = x + step * vec2(i-s, j-s);
      texel = textureLod(colorTex, x_, 0);
      R[j+i*KERNEL] =texel.x;
      G[j+i*KERNEL] =texel.y;
      B[j+i*KERNEL] =texel.z;
    }
  }

  sort();
  result = vec4(R[KERNEL * KERNEL / 2],G[KERNEL * KERNEL / 2],B[KERNEL * KERNEL / 2],1.0);
  return result;
}

void main()
{
  color = median(surf.texCoord);
}