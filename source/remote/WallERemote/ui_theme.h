#pragma once
#include <stdint.h>

namespace ui_theme {
// Transgender pride flag: light blue, pink, white, pink, light blue.
constexpr uint16_t BLUE=0x5E7F, PINK=0xF557, WHITE=0xFFFF;
constexpr uint16_t stripe(int y,int height){
 int band=y*5/height;
 return band==0||band==4?BLUE:band==2?WHITE:PINK;
}
constexpr bool rounded(int x,int y,int width,int height,int radius){
 if(x<0||y<0||x>=width||y>=height)return false;
 int dx=x<radius?radius-x:x>=width-radius?x-(width-radius-1):0;
 int dy=y<radius?radius-y:y>=height-radius?y-(height-radius-1):0;
 return !dx||!dy||dx*dx+dy*dy<=radius*radius;
}
static_assert(stripe(0,100)==BLUE&&stripe(20,100)==PINK&&stripe(40,100)==WHITE&&stripe(60,100)==PINK&&stripe(80,100)==BLUE);
static_assert(!rounded(0,0,112,96,20)&&rounded(56,48,112,96,20));
}
