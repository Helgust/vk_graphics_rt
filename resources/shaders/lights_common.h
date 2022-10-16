#ifndef Lights_common_H
#define Lights_common_H

struct Light { //deprecated pos and radius
  vec4  dir;
  vec4  color;
  vec4  pos; 
  vec4 radius_lightDist_dummies; // x-radius, y-lightDist zw - dumy things //
};

#endif