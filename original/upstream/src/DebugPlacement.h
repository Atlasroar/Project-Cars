#pragma once
#include <float.h>
namespace cars {
// Center and four footprint corners, sampled from the loaded terrain renderer.
inline bool debugChestGround(float actorHeight,const float* heights) {
    if (!_finite(actorHeight)) return false;
    float low=heights[0],high=heights[0];
    for (int i=0;i<5;++i) {
        if (!_finite(heights[i]) || heights[i]<-10000 || heights[i]>10000) return false;
        if (heights[i]<low) low=heights[i];
        if (heights[i]>high) high=heights[i];
    }
    return high-low<=0.6f && heights[0]>=actorHeight-4 && heights[0]<=actorHeight+4;
}
}
