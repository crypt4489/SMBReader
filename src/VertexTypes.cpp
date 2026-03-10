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

pospack6_cnorm_c16tex1_bone2_type_h::pospack6_cnorm_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector3f& _n, const Vector2i& _b, const Vector2f& _w) :
	POSITION(_p), TEXTURE(_t), NORMAL(_n), BONES(_b), WEIGHTS(_w)
{
}


std::array<VertexInputDescription, 5> pospack6_cnorm_c16tex1_bone2_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 5> attributeDescriptions;

	attributeDescriptions[0].byteoffset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, POSITION);
	attributeDescriptions[0].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	attributeDescriptions[0].vertexusage = VertexUsage::POSITION;

	attributeDescriptions[1].byteoffset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, TEXTURE);
	attributeDescriptions[1].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[1].vertexusage = VertexUsage::TEX0;

	attributeDescriptions[2].byteoffset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, NORMAL);
	attributeDescriptions[2].format = ComponentFormatType::R32G32B32_SFLOAT;
	attributeDescriptions[2].vertexusage = VertexUsage::NORMAL;

	attributeDescriptions[3].byteoffset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, BONES);
	attributeDescriptions[3].format = ComponentFormatType::R32G32_SINT;
	attributeDescriptions[3].vertexusage = VertexUsage::BONES;

	attributeDescriptions[4].byteoffset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, WEIGHTS);
	attributeDescriptions[4].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[4].vertexusage = VertexUsage::WEIGHTS;

	return attributeDescriptions;
}

bool pospack6_cnorm_c16tex1_bone2_type_h::operator==(const pospack6_cnorm_c16tex1_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.NORMAL == this->NORMAL) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}





pospack6_c16tex1_bone2_type_h::pospack6_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector2i& _b, const Vector2f& _w) :
	POSITION(_p), TEXTURE(_t), BONES(_b), WEIGHTS(_w)
{
}

std::array<VertexInputDescription, 4> pospack6_c16tex1_bone2_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 4> attributeDescriptions;

	attributeDescriptions[0].byteoffset = offsetof(pospack6_c16tex1_bone2_type_h, POSITION);
	attributeDescriptions[0].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	attributeDescriptions[0].vertexusage = VertexUsage::POSITION;

	attributeDescriptions[1].byteoffset = offsetof(pospack6_c16tex1_bone2_type_h, TEXTURE);
	attributeDescriptions[1].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[1].vertexusage = VertexUsage::TEX0;

	attributeDescriptions[2].byteoffset = offsetof(pospack6_c16tex1_bone2_type_h, BONES);
	attributeDescriptions[2].format = ComponentFormatType::R32G32_SINT;
	attributeDescriptions[2].vertexusage = VertexUsage::BONES;

	attributeDescriptions[3].byteoffset = offsetof(pospack6_c16tex1_bone2_type_h, WEIGHTS);
	attributeDescriptions[3].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[3].vertexusage = VertexUsage::WEIGHTS;

	return attributeDescriptions;
}

bool pospack6_c16tex1_bone2_type_h::operator==(const pospack6_c16tex1_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

pospack6_c16tex2_bone2_type_h::pospack6_c16tex2_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector2f& _t2, const Vector2i& _b, const Vector2f& _w) :
	POSITION(_p), TEXTURE(_t), TEXTURE2(_t2), BONES(_b), WEIGHTS(_w)
{
}


std::array<VertexInputDescription, 5> pospack6_c16tex2_bone2_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 5> attributeDescriptions;

	attributeDescriptions[0].byteoffset = offsetof(pospack6_c16tex2_bone2_type_h, POSITION);
	attributeDescriptions[0].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	attributeDescriptions[0].vertexusage = VertexUsage::POSITION;

	attributeDescriptions[1].byteoffset = offsetof(pospack6_c16tex2_bone2_type_h, TEXTURE);
	attributeDescriptions[1].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[1].vertexusage = VertexUsage::TEX0;

	attributeDescriptions[2].byteoffset = offsetof(pospack6_c16tex2_bone2_type_h, TEXTURE2);
	attributeDescriptions[2].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[2].vertexusage = VertexUsage::TEX1;

	attributeDescriptions[3].byteoffset = offsetof(pospack6_c16tex2_bone2_type_h, BONES);
	attributeDescriptions[3].format = ComponentFormatType::R32G32_SINT;
	attributeDescriptions[3].vertexusage = VertexUsage::BONES;

	attributeDescriptions[4].byteoffset = offsetof(pospack6_c16tex2_bone2_type_h, WEIGHTS);
	attributeDescriptions[4].format = ComponentFormatType::R32G32_SFLOAT;
	attributeDescriptions[4].vertexusage = VertexUsage::WEIGHTS;

	return attributeDescriptions;
}
bool pospack6_c16tex2_bone2_type_h::operator==(const pospack6_c16tex2_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.TEXTURE2 == this->TEXTURE2) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

cpospack6_cnorm_c16tex1_bone2_type_h::cpospack6_cnorm_c16tex1_bone2_type_h(
		const Vector3s& _p,
		const Vector2s& _t,
		const int _n,
		const Vector2uc& _b,
		const Vector2uc& _w)
		: POSITION(_p), TEXTURE(_t), NORMAL(_n), BONES(_b), WEIGHTS(_w)
	{
	}

std::array<VertexInputDescription, 5> cpospack6_cnorm_c16tex1_bone2_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 5> a;

	a[0].byteoffset = offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, BONES);
	a[0].format = ComponentFormatType::R8G8_UINT;
	a[0].vertexusage = VertexUsage::BONES;

	a[1].byteoffset = offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, WEIGHTS);
	a[1].format = ComponentFormatType::R8G8_UINT;
	a[1].vertexusage = VertexUsage::WEIGHTS;

	a[2].byteoffset = offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, TEXTURE);
	a[2].format = ComponentFormatType::R16G16_SINT;
	a[2].vertexusage = VertexUsage::TEX0;

	a[3].byteoffset = offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, NORMAL);
	a[3].format = ComponentFormatType::R32_SINT;
	a[3].vertexusage = VertexUsage::NORMAL;

	a[4].byteoffset = offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, POSITION);
	a[4].format = ComponentFormatType::R16G16B16_SINT;
	a[4].vertexusage = VertexUsage::POSITION;

	return a;
}

bool cpospack6_cnorm_c16tex1_bone2_type_h::operator==(const cpospack6_cnorm_c16tex1_bone2_type_h& v) const
{
	return POSITION == v.POSITION &&
		TEXTURE == v.TEXTURE &&
		NORMAL == v.NORMAL &&
		WEIGHTS == v.WEIGHTS &&
		BONES == v.BONES;
}


cpospack6_c16tex1_bone2_type_h::cpospack6_c16tex1_bone2_type_h(
	const Vector3s& _p,
	const Vector2s& _t,
	const Vector2uc& _b,
	const Vector2uc& _w)
	: POSITION(_p), TEXTURE(_t), BONES(_b), WEIGHTS(_w)
{
}

std::array<VertexInputDescription, 4> cpospack6_c16tex1_bone2_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 4> a;

	a[0].byteoffset = offsetof(cpospack6_c16tex1_bone2_type_h, BONES);
	a[0].format = ComponentFormatType::R8G8_UINT;
	a[0].vertexusage = VertexUsage::BONES;

	a[1].byteoffset = offsetof(cpospack6_c16tex1_bone2_type_h, WEIGHTS);
	a[1].format = ComponentFormatType::R8G8_UINT;
	a[1].vertexusage = VertexUsage::WEIGHTS;

	a[2].byteoffset = offsetof(cpospack6_c16tex1_bone2_type_h, TEXTURE);
	a[2].format = ComponentFormatType::R16G16_SINT;
	a[2].vertexusage = VertexUsage::TEX0;

	a[3].byteoffset = offsetof(cpospack6_c16tex1_bone2_type_h, POSITION);
	a[3].format = ComponentFormatType::R16G16B16_SINT;
	a[3].vertexusage = VertexUsage::POSITION;

	return a;
}

bool cpospack6_c16tex1_bone2_type_h::operator==(const cpospack6_c16tex1_bone2_type_h& v) const
{
	return POSITION == v.POSITION &&
		TEXTURE == v.TEXTURE &&
		WEIGHTS == v.WEIGHTS &&
		BONES == v.BONES;
}

cpospack6_c16tex2_bone2_type_h::cpospack6_c16tex2_bone2_type_h(const Vector3s& _p, const Vector2s& _t, const Vector2s& _t2, const Vector2uc& _b, const Vector2uc& _w) :
	POSITION(_p), TEXTURE(_t), TEXTURE2(_t2), BONES(_b), WEIGHTS(_w)
{
}




std::array<VertexInputDescription, 5> cpospack6_c16tex2_bone2_type_h::getAttributeDescriptions()
{
	std::array<VertexInputDescription, 5> attributeDescriptions;

	attributeDescriptions[0].byteoffset = offsetof(cpospack6_c16tex2_bone2_type_h, BONES);
	attributeDescriptions[0].format = ComponentFormatType::R8G8_UINT;
	attributeDescriptions[0].vertexusage = VertexUsage::BONES;

	attributeDescriptions[1].byteoffset = offsetof(cpospack6_c16tex2_bone2_type_h, WEIGHTS);
	attributeDescriptions[1].format = ComponentFormatType::R8G8_UINT;
	attributeDescriptions[1].vertexusage = VertexUsage::WEIGHTS;

	attributeDescriptions[2].byteoffset = offsetof(cpospack6_c16tex2_bone2_type_h, TEXTURE);
	attributeDescriptions[2].format = ComponentFormatType::R16G16_SINT;
	attributeDescriptions[2].vertexusage = VertexUsage::TEX0;

	attributeDescriptions[3].byteoffset = offsetof(cpospack6_c16tex2_bone2_type_h, TEXTURE2);
	attributeDescriptions[3].format = ComponentFormatType::R16G16_SINT;
	attributeDescriptions[3].vertexusage = VertexUsage::TEX1;

	attributeDescriptions[4].byteoffset = offsetof(cpospack6_c16tex2_bone2_type_h, POSITION);
	attributeDescriptions[4].format = ComponentFormatType::R16G16B16_SINT;
	attributeDescriptions[4].vertexusage = VertexUsage::POSITION;


	return attributeDescriptions;
}

bool cpospack6_c16tex2_bone2_type_h::operator==(const cpospack6_c16tex2_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.TEXTURE2 == this->TEXTURE2) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

