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

#define NO_PARENT 4095
#define NOT_A_CHILD 1023

#define PARENT_OFFSET 0
#define MAX_PARENT_INDEX 4096
#define MAKE_PARENT_INDEX(x) (((x) & MAX_PARENT_INDEX-1) << PARENT_OFFSET)

#define CHILD_INDEX_OFFSET 12
#define MAX_CHILD_INDEX 1024
#define MAKE_CHILD_INDEX(x) (((x) & MAX_CHILD_INDEX-1) << CHILD_INDEX_OFFSET)

#define CHILD_COUNT_OFFSET 22
#define MAX_CHILD_COUNT 1024
#define MAKE_CHILD_COUNT(x) (((x) & MAX_CHILD_COUNT-1) << CHILD_COUNT_OFFSET)

#define MAKE_DEPTH(depth) (((depth) & DEPTH_MAX-1) << DEPTH_BIT_OFFSET)
#define MAKE_TYPE(type) (((type) & TYPE_MAX-1) << TYPE_BIT_OFFSET)
#define MAKE_TYPE_SPECIFIC_DATA(data) (((data) & TYPE_SPECIFIC_MAX-1) << TYPE_SPECIFIC_DATA_OFFSET)
#define MAKE_ORIENTATION(orient) (((orient) & CONTAINER_ORIENTATION_MAX-1) << CONTAINER_ORIENTATION_DATA_OFFSET)

#define PACK_PARENT_CHILD_INFO(parentIndex, childIndex, childCount) MAKE_CHILD_COUNT(childCount) | MAKE_CHILD_INDEX(childIndex) | MAKE_PARENT_INDEX(parentIndex)

#define TEXT_BUFFER_LOCATION_OFFSET 0
#define MAX_TEXT_BUFFER_LOCATION (524288)
#define MAKE_TEXT_BUFFER_LOCATION(x) (((x) & MAX_TEXT_BUFFER_LOCATION-1) << TEXT_BUFFER_LOCATION_OFFSET)

#define TEXT_SIZE_OFFSET 19
#define MAX_TEXT_SIZE 4096
#define MAKE_TEXT_SIZE(x) (((x) & MAX_TEXT_SIZE-1) << TEXT_SIZE_OFFSET)

#define HAS_TEXT_OFFSET 31
#define MAKE_HAS_TEXT(x) ((x) << HAS_TEXT_OFFSET)

#define PACK_TEXT_DATA(offset, count) (unsigned int)(MAKE_HAS_TEXT(1) | MAKE_TEXT_SIZE(count) | MAKE_TEXT_BUFFER_LOCATION(offset))

#define MAKE_COLOR_COMPONENT(c) ((float)(c)/256.0)
#define MAKE_COLOR(r, g, b, a) { MAKE_COLOR_COMPONENT(r), MAKE_COLOR_COMPONENT(g), MAKE_COLOR_COMPONENT(b), (float)(a)}
#define PACK_COLOR_10_11_10_1(r, g, b, a) ((((unsigned int)(MAKE_COLOR_COMPONENT(r) * (float)(1<<10))) << 22) | \
										   (((unsigned int)(MAKE_COLOR_COMPONENT(g) * (float)(1<<11))) << 11) | \
										   (((unsigned int)(MAKE_COLOR_COMPONENT(b) * (float)(1<<10))) << 1) | \
										   (((unsigned int)a) & 1))

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
	
	/*
		For Container 
		x = packedBorderColor1, y = packedBorderColor2
	*/

	Vector4ui packedData;

};

struct UIRetainedContainer
{
	Vector2f absoluteSize;
	Vector2f anchorPoint;
	Vector4ui retainedHoverData;
};

struct UITextVertex
{
	Vector2f pos;
	Vector2f texCoords;
};