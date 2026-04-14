#include "VertexTypes.h"
#include <array>
#include "MathTypes.h"
#include "VKUtilities.h"

text_vertex_t::text_vertex_t(Vector2f _p, Vector2f _t, Vector4f _c) :
	POSITION(_p), TEXTURE(_t), COLOR(_c)

{

}

std::array<VertexInputDescription, 3> text_vertex_t::getAttributeDescriptions() {
	std::array<VertexInputDescription, 3> attributeDescriptions;

	attributeDescriptions[0].byteoffset = offsetof(text_vertex_t, POSITION);
	attributeDescriptions[0].format = ComponentFormatType::R32G32_SFLOAT; 
	attributeDescriptions[0].vertexusage = VertexUsage::POSITION;

	attributeDescriptions[1].byteoffset = offsetof(text_vertex_t, TEXTURE);
	attributeDescriptions[1].format = ComponentFormatType::R32G32_SFLOAT; 
	attributeDescriptions[1].vertexusage = VertexUsage::TEX0;

	attributeDescriptions[2].byteoffset = offsetof(text_vertex_t, COLOR);
	attributeDescriptions[2].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	attributeDescriptions[2].vertexusage = VertexUsage::COLOR0;

	return attributeDescriptions;
}

bool text_vertex_t::operator==(const text_vertex_t& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.COLOR == this->COLOR);
}

vertex_type_h::vertex_type_h(Vector4f _p, Vector2f _t, Vector3f _n) :
	POSITION(_p), TEXTURE(_t), NORMAL(_n)
{
}

std::array<VertexInputDescription, 3> vertex_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 3> attributeDescriptions;

	attributeDescriptions[0].byteoffset = offsetof(vertex_type_h, POSITION);
	attributeDescriptions[0].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	attributeDescriptions[0].vertexusage = VertexUsage::POSITION;

	attributeDescriptions[1].byteoffset = offsetof(vertex_type_h, TEXTURE);
	attributeDescriptions[1].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[1].vertexusage = VertexUsage::TEX0;

	attributeDescriptions[2].byteoffset = offsetof(vertex_type_h, NORMAL);
	attributeDescriptions[2].format = ComponentFormatType::R32G32B32_SFLOAT;
	attributeDescriptions[2].vertexusage = VertexUsage::NORMAL;

	return attributeDescriptions;
}

bool vertex_type_h::operator==(const vertex_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.NORMAL == this->NORMAL);
}



basic_vertex_type_h::basic_vertex_type_h(Vector4f _p, Vector4f _t) :
	POSITION(_p), TEXTURE(_t)
{
}


std::array<VertexInputDescription, 2> basic_vertex_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 2> attributeDescriptions;

	attributeDescriptions[0].byteoffset = offsetof(basic_vertex_type_h, POSITION);
	attributeDescriptions[0].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	attributeDescriptions[0].vertexusage = VertexUsage::POSITION;

	attributeDescriptions[1].byteoffset = offsetof(basic_vertex_type_h, TEXTURE);
	attributeDescriptions[1].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	attributeDescriptions[1].vertexusage = VertexUsage::TEX0;

	return attributeDescriptions;
}

bool basic_vertex_type_h::operator==(const basic_vertex_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE);
}


Vector3f DecompressPosition(Vector3s vector, AxisBox& box)
{
	int16_t x = vector.x;
	int16_t y = vector.y;
	int16_t z = vector.z;

	float x1 = ((((float)x * dx) + 1.0f) * 0.5);
	float y1 = ((((float)y * dx) + 1.0f) * 0.5);
	float z1 = ((((float)z * dx) + 1.0f) * 0.5);

	x1 = ((box.max.x - box.min.x) * x1) + box.min.x;
	y1 = ((box.max.y - box.min.y) * y1) + box.min.y;
	z1 = ((box.max.z - box.min.z) * x1) + box.min.z;

	return Vector3f(x1, y1, z1);
}

Vector3s CompressPosition(Vector4f vector, AxisBox& box)
{
	float x = vector.x;
	float y = vector.y;
	float z = vector.z;

	float x1 = ((((x - box.min.x) / (box.max.x - box.min.x)) * 2.0) - 1.0);
	float y1 = ((((y - box.min.y) / (box.max.y - box.min.y)) * 2.0) - 1.0);
	float z1 = ((((z - box.min.z) / (box.max.z - box.min.z)) * 2.0) - 1.0);

	int16_t xs = x1 * 32767;
	int16_t ys = y1 * 32767;
	int16_t zs = z1 * 32767;

	return Vector3s(xs, ys, zs);
}

Vector3s CompressPosition(Vector3f vector, AxisBox& box)
{
	float x = vector.x;
	float y = vector.y;
	float z = vector.z;

	float x1 = ((((x - box.min.x) / (box.max.x - box.min.x)) * 2.0) - 1.0);
	float y1 = ((((y - box.min.y) / (box.max.y - box.min.y)) * 2.0) - 1.0);
	float z1 = ((((z - box.min.z) / (box.max.z - box.min.z)) * 2.0) - 1.0);

	int16_t xs = x1 * 32767;
	int16_t ys = y1 * 32767;
	int16_t zs = z1 * 32767;

	return Vector3s(xs, ys, zs);
}

int32_t CompressNormal(Vector3f normal)
{
	int32_t reg = 0;

	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	int16_t xs = (1023.5 * x);
	int16_t ys = (1023.5 * y);
	int16_t zs = (511.5 * z);

	reg = ((zs & 0x3ff) << 22) | ((ys & 0x7ff) << 11) | ((xs & 0x7ff));

	return reg;
}

Vector2s CompressTexCoords(Vector2f in)
{
	int16_t x = ((in.x / 16.0f) * 32767.5);
	int16_t y = ((in.y / 16.0f) * 32767.5);

	return Vector2s(x, y);
}

Vector2f DecompressTexCoords(Vector2s in)
{
	float x = in.x * dx * 16.0f;
	float y = in.y * dx * 16.0f;
	return Vector2f(x, y);
}

int32_t CompressTangent(Vector4f tangent)
{
	int wc = (tangent.w > 0.0) ? 1 : 0;

	int xc = (tangent.x * 511.5);
	int yc = (tangent.y * 511.5);
	int zc = (tangent.z * 511.5);

	int32_t ret = ((zc & 0x3ff) << 22) | ((yc & 0x3ff) << 12) | ((xc & 0x3ff) << 2) | (wc & 1);

	return ret;
}

Vector4f DecompressTangent(int32_t ctangent)
{
	float w = (ctangent & 1) ? 1.0f : -1.0f;

	int zi = (int)(ctangent & 0xffc00000);
	int yi = (int)(ctangent & 0x003ff000) << 10;
	int xi = (int)(ctangent & 0x00000ffc) << 20;

	float x = (float)(xi) / 2143289344.0f;
	float y = (float)(yi) / 2143289344.0f;
	float z = (float)(zi) / 2143289344.0f;

	return Vector4f(x, y, z, w);
}

int32_t CompressColor(Vector4f color)
{
	int r = (int)(color.x * 255.5);
	int g = (int)(color.y * 255.5);
	int b = (int)(color.z * 255.5);
	int a = (int)(color.w * 255.5);

	return (a << 24) | (b << 16) | (g << 8) | (r);
}





void CreateBitTangentFromNormal(Vector4f* pos, Vector2f* uvs, uint16_t* indices, int totalIndexCount, int totalVertCount, Vector4f* tangents, Vector3f* outNormals, RingAllocator* tempAllocator)
{

	Vector3f* totalTangents = (Vector3f*)tempAllocator->CAllocate(sizeof(Vector3f) * totalVertCount, 16);
	float* signBitTangents = (float*)tempAllocator->CAllocate(sizeof(float) * totalVertCount, 4);
	Vector3f* normals = (Vector3f*)tempAllocator->CAllocate(sizeof(Vector3f) * totalVertCount, 16);

	for (int lead = 0; lead < totalIndexCount - 2; lead++)
	{
		uint16_t index1 = indices[lead];
		uint16_t index2 = indices[lead + 1];
		uint16_t index3 = indices[lead + 2];

		Vector3f vert1 = Vector3f(pos[index1].x, pos[index1].y, pos[index1].z);
		Vector3f vert2 = Vector3f(pos[index2].x, pos[index2].y, pos[index2].z);
		Vector3f vert3 = Vector3f(pos[index3].x, pos[index3].y, pos[index3].z);


		Vector3f e1 = vert2 - vert1;
		Vector3f e2 = vert3 - vert1;

		Vector3f normal = Cross(e1, e2);

		float area2 = Dot(normal, normal);

		if (area2 <= 0.0f) {
			continue;
		}

		normals[index1] = normals[index1] + normal;
		normals[index2] = normals[index2] + normal;
		normals[index3] = normals[index3] + normal;

		Vector2f uv1 = uvs[index1];
		Vector2f uv2 = uvs[index2];
		Vector2f uv3 = uvs[index3];

		Vector2f duv1 = uv2 - uv1;
		Vector2f duv2 = uv3 - uv1;

		float f_det = (duv1.x * duv2.y - duv2.x * duv1.y);



		float f = 1.0f / f_det;

		Vector3f tangent;
		tangent = Normalize((e1 * duv2.y - e2 * duv1.y) * f);

		Vector3f bitangent = Normalize((e2 * duv1.x - e1 * duv2.x) * f);


		Vector3f n = Normalize(normal);
		float sign = (Dot(Cross(n, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;

		totalTangents[index1] = totalTangents[index1] + tangent;
		totalTangents[index2] = totalTangents[index2] + tangent;
		totalTangents[index3] = totalTangents[index3] + tangent;

		signBitTangents[index1] += sign;
		signBitTangents[index2] += sign;
		signBitTangents[index3] += sign;
	}


	for (int lead = 0; lead < totalVertCount; lead++)
	{
		Vector3f normal = normals[lead];

		normal = Normalize(normal);

		Vector3f tangent = totalTangents[lead];

		tangent = Normalize(tangent - (normal * Dot(normal, tangent)));

		float handedness = signBitTangents[lead] < 0.0f ? -1.0f : 1.0f;

		Vector4f outTangent = Vector4f(tangent.x, tangent.y, tangent.z, handedness);

		tangents[lead] = outTangent;

		outNormals[lead] = normal;
	}
}

int CompressMeshFromVertexStream(VertexInputDescription* inputDesc, int descCount, int vertexStride, int vertexCount,
	AxisBox& box, void* vertexStream, void* memoryOut, int* compressedSize, int* vertexFlags)
{
	char* streamStart = (char*)vertexStream;
	int locationInVertexStream = 0;

	char* dataStreamOut = (char*)memoryOut;
	int locationInStreamOut = 0;

	enum VertexPosition
	{
		PPOSITION = 8,
		PCOLOR0 = 7,
		BONESWEIGHTS = 0,
		PTEX0 = 1,
		PTEX1 = 2,
		PTEX2 = 3,
		PTEX3 = 4,
		PNORMAL = 5,
		PTANGENT = 6,
	};

	std::array<int, 9> positionalOffsets;

	memset(positionalOffsets.data(), 0, sizeof(int) * positionalOffsets.size());

	*vertexFlags |= COMPRESSED;

	int compressedLSize = 0;

	for (int j = 0; j < descCount; j++)
	{
		VertexInputDescription* desc = &inputDesc[j];

		size_t size = 0;

		int positionIndex = 0;

		switch (desc->vertexusage)
		{
		case VertexUsage::POSITION:
		{
			size = sizeof(Vector3s);
			positionIndex = PPOSITION;
			*vertexFlags |= POSITION;
			break;
		}
		case VertexUsage::NORMAL:
		{
			size = sizeof(int32_t);
			positionIndex = PNORMAL;
			*vertexFlags |= NORMAL;
			break;
		}
		case VertexUsage::COLOR0:
		{
			size = sizeof(int32_t);
			positionIndex = PCOLOR0;
			*vertexFlags |= COLOR;
			break;
		}
		case VertexUsage::TEX0:
		{
			size = sizeof(Vector2s);
			positionIndex = PTEX0;
			*vertexFlags |= TEXTURE0;
			break;
		}
		case VertexUsage::TEX1:
		{
			size = sizeof(Vector2s);
			positionIndex = PTEX1;
			*vertexFlags |= TEXTURE1;
			break;
		}
		case VertexUsage::TEX2:
		{
			size = sizeof(Vector2s);
			positionIndex = PTEX2;
			*vertexFlags |= TEXTURE2;
			break;
		}
		case VertexUsage::TANGENTS:
		{
			size = sizeof(int32_t);
			positionIndex = PTANGENT;
			*vertexFlags |= TANGENT;
			break;
		}
		}

		positionalOffsets[positionIndex] = size;
	
		compressedLSize += size;
	}

	int prev = compressedLSize;

	if (compressedSize)
	{
		*compressedSize = compressedLSize;
	}

	for (int i = 8; i >= 0; i--)
	{
		prev -= positionalOffsets[i];
		positionalOffsets[i] = prev;
	}


	for (int i = 0; i < vertexCount; i++)
	{
		for (int j = 0; j < descCount; j++)
		{
			VertexInputDescription* desc = &inputDesc[j];
			char* genericInput = streamStart + locationInVertexStream + desc->byteoffset;
			switch (desc->vertexusage)
			{
			case VertexUsage::POSITION:
			{
				switch (desc->format)
				{
				case ComponentFormatType::R32G32B32A32_SFLOAT:
				{

					Vector4f* pos = (Vector4f*)genericInput;
					Vector3s pack = CompressPosition(*pos, box);
					memcpy(dataStreamOut + locationInStreamOut + positionalOffsets[PPOSITION], &pack, sizeof(Vector3s));
					break;
				}
				}
				break;
			}
			case VertexUsage::NORMAL:
			{
				switch (desc->format)
				{
				case ComponentFormatType::R32G32B32_SFLOAT:
				{
					Vector3f* pos = (Vector3f*)genericInput;
					int32_t pack = CompressNormal(*pos);
					memcpy(dataStreamOut + locationInStreamOut + positionalOffsets[PNORMAL], &pack, sizeof(int32_t));
					break;
				}
				}
				break;
			}
			case VertexUsage::COLOR0:
			{
				switch (desc->format)
				{

				case ComponentFormatType::R32G32B32A32_SFLOAT:
				{
					Vector4f* color = (Vector4f*)genericInput;
					int32_t pack = CompressColor(*color);
					memcpy(dataStreamOut + locationInStreamOut + positionalOffsets[PCOLOR0], &pack, sizeof(int32_t));
					break;
				}
				}
				break;
			}
			case VertexUsage::TEX0:
			{
				switch (desc->format)
				{

				case ComponentFormatType::R32G32_SFLOAT:
				{
					Vector2s packed = CompressTexCoords(*((Vector2f*)genericInput));
					memcpy(dataStreamOut + locationInStreamOut + positionalOffsets[PTEX0], &packed, sizeof(Vector2s));
					break;
				}
				}
				break;
			}
			case VertexUsage::TEX1:
			{
				switch (desc->format)
				{

				case ComponentFormatType::R32G32_SFLOAT:
				{
					Vector2s packed = CompressTexCoords(*((Vector2f*)genericInput));
					memcpy(dataStreamOut + locationInStreamOut + positionalOffsets[PTEX1], &packed, sizeof(Vector2s));
					break;
				}
				}
				break;
			}
			case VertexUsage::TEX2:
			{
				switch (desc->format)
				{

				case ComponentFormatType::R32G32_SFLOAT:
				{
					Vector2s packed = CompressTexCoords(*((Vector2f*)genericInput));
					memcpy(dataStreamOut + locationInStreamOut + positionalOffsets[PTEX2], &packed, sizeof(Vector2s));
					break;
				}
				}
				break;
			}
			case VertexUsage::TANGENTS:
			{
				switch (desc->format)
				{
				case ComponentFormatType::R32G32B32A32_SFLOAT:
				{
					int32_t packed = CompressTangent(*((Vector4f*)genericInput));
					memcpy(dataStreamOut + locationInStreamOut + positionalOffsets[PTANGENT], &packed, sizeof(int32_t));
					break;
				}
				}
				break;
			}
			}
		}

		locationInVertexStream += vertexStride;
		locationInStreamOut += compressedLSize;
	}

	return locationInStreamOut;
}

int GetCompressedSize(VertexInputDescription* inputDesc, int descCount)
{
	int compressedSize = 0;

	for (int j = 0; j < descCount; j++)
	{
		VertexInputDescription* desc = &inputDesc[j];

		size_t size = 0;


		switch (desc->vertexusage)
		{
		case VertexUsage::POSITION:
		{
			size = sizeof(Vector3s);
			break;
		}
		case VertexUsage::NORMAL:
		{
			size = sizeof(int32_t);
			break;
		}
		case VertexUsage::COLOR0:
		{
			size = sizeof(int32_t);
			break;
		}
		case VertexUsage::TEX0:
		{
			size = sizeof(Vector2s);
			break;
		}
		case VertexUsage::TEX1:
		{
			size = sizeof(Vector2s);
			break;
		}
		case VertexUsage::TEX2:
		{
			size = sizeof(Vector2s);
			break;
		}
		case VertexUsage::TANGENTS:
		{
			size = sizeof(int32_t);
			break;
		}
		}

		
		compressedSize += size;
	}

	return compressedSize;
}