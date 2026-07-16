#pragma once
#include "math/MathTypes.h"

#define ROUNDED_CORNERS 0x1
#define INVISIBLE_CONTAINER 0x8

#define DEPTH_BIT_OFFSET 0x0
#define DEPTH_MAX 8

#define TYPE_BIT_OFFSET 0x3
#define TYPE_MAX 16

#define TYPE_SPECIFIC_DATA_OFFSET 0x8
#define TYPE_SPECIFIC_MAX 16

#define MAKE_DEPTH(depth) (((depth) & DEPTH_MAX-1) << DEPTH_BIT_OFFSET)
#define MAKE_TYPE(type) (((type) & TYPE_MAX-1) << TYPE_BIT_OFFSET)
#define MAKE_TYPE_SPECIFIC_DATA(data) (((data) & TYPE_SPECIFIC_MAX-1) << TYPE_SPECIFIC_DATA_OFFSET)

//bitfields

//bits 0-2 depth
//bits 3-7 type
//bits 8-11 typeSpecificData

struct UIContainer
{
	Vector4ui bitfields;  // x - bitfields y - children count, 
	Vector4f color;
	Vector4f padding; //padding top, bottom, left, right
	Vector2f relativeContainerSize; // percent size of the canvas
	Vector2f absoluteSize;
	Vector2f anchorPoint;
	Vector2f structPad;
};