#pragma once
#include "math/MathTypes.h"

struct UIContainer
{
	Vector2ui bitfields;  // x - bitfields y - children count
	Vector2f relativeContainerSize; // percent size of the canvas
	Vector2f absoluteSize;
	Vector2f anchorPoint;
	Vector4f color;
};