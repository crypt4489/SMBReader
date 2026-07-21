#pragma once
#include "math/MathTypes.h"

#define ROUNDED_CORNERS 0x1
#define BORDERS 0x2
#define INVISIBLE_CONTAINER 0x8

#define DEPTH_BIT_OFFSET 0x0
#define DEPTH_MAX 8

#define TYPE_BIT_OFFSET 0x3
#define TYPE_MAX 16

#define TYPE_SPECIFIC_DATA_OFFSET 0x8
#define TYPE_SPECIFIC_MAX 16

#define CONTAINER_ORIENTATION_DATA_OFFSET 12
#define CONTAINER_ORIENTATION_MAX 2

#define HORIZONTAL_ORIENTATION 0
#define VERTICAL_ORIENTATION 1


#define MAKE_DEPTH(depth) (((depth) & DEPTH_MAX-1) << DEPTH_BIT_OFFSET)
#define MAKE_TYPE(type) (((type) & TYPE_MAX-1) << TYPE_BIT_OFFSET)
#define MAKE_TYPE_SPECIFIC_DATA(data) (((data) & TYPE_SPECIFIC_MAX-1) << TYPE_SPECIFIC_DATA_OFFSET)
#define MAKE_ORIENTATION(orient) (((orient) & CONTAINER_ORIENTATION_MAX-1) << CONTAINER_ORIENTATION_DATA_OFFSET)

#define NO_PARENT 4294967295
#define NOT_A_CHILD NO_PARENT

//bitfields

//bits 0-2 depth
//bits 3-7 type
//bits 8-11 typeSpecificData
//bit 12 orientation 

struct UIContainer
{
	Vector4ui bitfields;  // x - bitfields y - children count, z - parentIndex, w - childId  
	Vector4f color;
	Vector4f padding; //padding top, bottom, left, right
	Vector2f relativeContainerSize; // percent size of the canvas
	Vector2f structPad;
};

struct UIRetainedContainer
{
	Vector2f absoluteSize;
	Vector2f anchorPoint;
};