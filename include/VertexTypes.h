#pragma once
#include <array>
#include "glm/glm.hpp"
#include "VKUtilities.h"
typedef struct text_vertex_t
{
	text_vertex_t() = default;
	text_vertex_t(glm::vec2 _p, glm::vec2 _t, glm::vec4 _c) :
		POSITION(_p), TEXTURE(_t), COLOR(_c)

	{

	}

	glm::vec2 POSITION;
	glm::vec2 TEXTURE;
	glm::vec4 COLOR;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(text_vertex_t);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(text_vertex_t, POSITION);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(text_vertex_t, TEXTURE);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(text_vertex_t, COLOR);

		return attributeDescriptions;
	}

	bool operator==(const text_vertex_t& v)
	{
		return (v.POSITION == this->POSITION) &&
			(v.TEXTURE == this->TEXTURE) &&
			(v.COLOR == this->COLOR);
	}

} TextVertex;

typedef struct vertex_type_h
{
	glm::vec4 POSITION;
	glm::vec2 TEXTURE;
	glm::vec3 NORMAL;

	vertex_type_h() = default;
	vertex_type_h(glm::vec4 _p, glm::vec2 _t, glm::vec3 _n) :
		POSITION(_p), TEXTURE(_t), NORMAL(_n)
	{}

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(vertex_type_h);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(vertex_type_h, POSITION);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(vertex_type_h, TEXTURE);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(vertex_type_h, NORMAL);

		return attributeDescriptions;
	}

	bool operator==(const vertex_type_h& v)
	{
		return (v.POSITION == this->POSITION) &&
			(v.TEXTURE == this->TEXTURE) &&
			(v.NORMAL == this->NORMAL);
	}

} Vertex;

typedef struct basic_vertex_type_h
{
	glm::vec4 POSITION;
	glm::vec4 TEXTURE;

	basic_vertex_type_h() = default;
	basic_vertex_type_h(glm::vec4 _p, glm::vec4 _t) :
		POSITION(_p), TEXTURE(_t)
	{
	}

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(basic_vertex_type_h);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(basic_vertex_type_h, POSITION);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(basic_vertex_type_h, TEXTURE);

		return attributeDescriptions;
	}

	bool operator==(const basic_vertex_type_h& v)
	{
		return (v.POSITION == this->POSITION) &&
			(v.TEXTURE == this->TEXTURE);
	}

} BasicVertex;



typedef struct pospack6_cnorm_c16tex1_bone2_type_h
{
	glm::vec4 POSITION;
	glm::vec2 TEXTURE;
	glm::vec3 NORMAL;
	glm::ivec2 BONES;
	glm::vec2 WEIGHTS;

	pospack6_cnorm_c16tex1_bone2_type_h() = default;
	pospack6_cnorm_c16tex1_bone2_type_h(const glm::vec4& _p, const glm::vec2& _t, const glm::vec3& _n, const glm::ivec2& _b, const glm::vec2& _w) :
		POSITION(_p), TEXTURE(_t), NORMAL(_n), BONES(_b), WEIGHTS(_w)
	{
	}

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(pospack6_cnorm_c16tex1_bone2_type_h);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, POSITION);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, TEXTURE);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, NORMAL);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SINT;
		attributeDescriptions[3].offset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, BONES);

		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 4;
		attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[4].offset = offsetof(pospack6_cnorm_c16tex1_bone2_type_h, WEIGHTS);

		return attributeDescriptions;
	}

	bool operator==(const pospack6_cnorm_c16tex1_bone2_type_h& v)
	{
		return (v.POSITION == this->POSITION) &&
			(v.TEXTURE == this->TEXTURE) &&
			(v.NORMAL == this->NORMAL) &&
			(v.WEIGHTS == this->WEIGHTS) &&
			(v.BONES == this->BONES);
	}

} Vertex_PosPack6_CNorm_C16Tex1_Bone2;



typedef struct pospack6_c16tex1_bone2_type_h
{
	glm::vec4 POSITION;
	glm::vec2 TEXTURE;
	glm::ivec2 BONES;
	glm::vec2 WEIGHTS;

	pospack6_c16tex1_bone2_type_h() = default;
	pospack6_c16tex1_bone2_type_h(const glm::vec4& _p, const glm::vec2& _t, const glm::ivec2& _b, const glm::vec2& _w) :
		POSITION(_p), TEXTURE(_t),  BONES(_b), WEIGHTS(_w)
	{
	}

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(pospack6_c16tex1_bone2_type_h);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(pospack6_c16tex1_bone2_type_h, POSITION);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(pospack6_c16tex1_bone2_type_h, TEXTURE);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SINT;
		attributeDescriptions[2].offset = offsetof(pospack6_c16tex1_bone2_type_h, BONES);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(pospack6_c16tex1_bone2_type_h, WEIGHTS);

		return attributeDescriptions;
	}

	bool operator==(const pospack6_c16tex1_bone2_type_h& v)
	{
		return (v.POSITION == this->POSITION) &&
			(v.TEXTURE == this->TEXTURE) &&
			(v.WEIGHTS == this->WEIGHTS) &&
			(v.BONES == this->BONES);
	}

} Vertex_PosPack6_C16Tex1_Bone2;


typedef struct pospack6_c16tex2_bone2_type_h
{
	glm::vec4 POSITION;
	glm::vec2 TEXTURE;
	glm::vec2 TEXTURE2;
	glm::ivec2 BONES;
	glm::vec2 WEIGHTS;

	pospack6_c16tex2_bone2_type_h() = default;
	pospack6_c16tex2_bone2_type_h(const glm::vec4& _p, const glm::vec2& _t, const glm::vec2& _t2, const glm::ivec2& _b, const glm::vec2& _w) :
		POSITION(_p), TEXTURE(_t), TEXTURE2(_t2), BONES(_b), WEIGHTS(_w)
	{
	}

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(pospack6_c16tex2_bone2_type_h);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(pospack6_c16tex2_bone2_type_h, POSITION);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(pospack6_c16tex2_bone2_type_h, TEXTURE);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(pospack6_c16tex2_bone2_type_h, TEXTURE2);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SINT;
		attributeDescriptions[3].offset = offsetof(pospack6_c16tex2_bone2_type_h, BONES);

		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 4;
		attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[4].offset = offsetof(pospack6_c16tex2_bone2_type_h, WEIGHTS);

		return attributeDescriptions;
	}

	bool operator==(const pospack6_c16tex2_bone2_type_h& v)
	{
		return (v.POSITION == this->POSITION) &&
			(v.TEXTURE == this->TEXTURE) &&
			(v.TEXTURE2 == this->TEXTURE2) &&
			(v.WEIGHTS == this->WEIGHTS) &&
			(v.BONES == this->BONES);
	}

} Vertex_PosPack6_C16Tex2_Bone2;



#pragma pack(push, 2)
typedef struct cpospack6_cnorm_c16tex1_bone2_type_h
{
	glm::u8vec2  BONES;
	glm::u8vec2  WEIGHTS;
	int NORMAL;
	glm::i16vec2 TEXTURE;
	glm::i16vec3 POSITION;

	cpospack6_cnorm_c16tex1_bone2_type_h() = default;
	cpospack6_cnorm_c16tex1_bone2_type_h(
		const glm::i16vec3& _p,
		const glm::i16vec2& _t,
		const int& _n,
		const glm::u8vec2& _b,
		const glm::u8vec2& _w)
		: POSITION(_p), TEXTURE(_t), NORMAL(_n), BONES(_b), WEIGHTS(_w)
	{
	}

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(cpospack6_cnorm_c16tex1_bone2_type_h);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> a(5);

		a[0] = { 0, 0, VK_FORMAT_R8G8_UINT,        offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, BONES) };
		a[1] = { 0, 1, VK_FORMAT_R8G8_UINT,        offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, WEIGHTS) };
		a[2] = { 0, 2, VK_FORMAT_R16G16_SINT,      offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, TEXTURE) };
		a[3] = { 0, 3, VK_FORMAT_R32_SINT,		   offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, NORMAL) };
		a[4] = { 0, 4, VK_FORMAT_R16G16B16_SINT,   offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, POSITION) };

		return a;
	}

	bool operator==(const cpospack6_cnorm_c16tex1_bone2_type_h& v) const
	{
		return POSITION == v.POSITION &&
			TEXTURE == v.TEXTURE &&
			NORMAL == v.NORMAL &&
			WEIGHTS == v.WEIGHTS &&
			BONES == v.BONES;
	}

} CVertex_PosPack6_CNorm_C16Tex1_Bone2;


// ==============================
// PosPack6 + C16Tex1 + Bone2
// ==============================

typedef struct cpospack6_c16tex1_bone2_type_h
{
	glm::u8vec2  BONES;
	glm::u8vec2  WEIGHTS;
	glm::i16vec2 TEXTURE;
	glm::i16vec3 POSITION;

	cpospack6_c16tex1_bone2_type_h() = default;
	cpospack6_c16tex1_bone2_type_h(
		const glm::i16vec3& _p,
		const glm::i16vec2& _t,
		const glm::u8vec2& _b,
		const glm::u8vec2& _w)
		: POSITION(_p), TEXTURE(_t), BONES(_b), WEIGHTS(_w)
	{
	}

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(cpospack6_c16tex1_bone2_type_h);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> a(4);

		a[0] = { 0, 0, VK_FORMAT_R8G8_UINT,      offsetof(cpospack6_c16tex1_bone2_type_h, BONES) };
		a[1] = { 0, 1, VK_FORMAT_R8G8_UINT,      offsetof(cpospack6_c16tex1_bone2_type_h, WEIGHTS) };
		a[2] = { 0, 2, VK_FORMAT_R16G16_SINT,    offsetof(cpospack6_c16tex1_bone2_type_h, TEXTURE) };
		a[3] = { 0, 3, VK_FORMAT_R16G16B16_SINT, offsetof(cpospack6_c16tex1_bone2_type_h, POSITION) };

		return a;
	}

	bool operator==(const cpospack6_c16tex1_bone2_type_h& v) const
	{
		return POSITION == v.POSITION &&
			TEXTURE == v.TEXTURE &&
			WEIGHTS == v.WEIGHTS &&
			BONES == v.BONES;
	}

} CVertex_PosPack6_C16Tex1_Bone2;


typedef struct cpospack6_c16tex2_bone2_type_h
{
	glm::u8vec2 BONES;
	glm::u8vec2 WEIGHTS;
	glm::i16vec2 TEXTURE;
	glm::i16vec2 TEXTURE2;
	glm::i16vec3 POSITION;
	
	

	cpospack6_c16tex2_bone2_type_h() = default;
	cpospack6_c16tex2_bone2_type_h(const glm::i16vec3& _p, const glm::i16vec2& _t, const glm::i16vec2& _t2, const glm::u8vec2& _b, const glm::u8vec2& _w) :
		POSITION(_p), TEXTURE(_t), TEXTURE2(_t2), BONES(_b), WEIGHTS(_w)
	{
	}

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(cpospack6_c16tex2_bone2_type_h);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 4;
		attributeDescriptions[0].format = VK_FORMAT_R16G16B16_SINT;
		attributeDescriptions[0].offset = offsetof(cpospack6_c16tex2_bone2_type_h, POSITION);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 2;
		attributeDescriptions[1].format = VK_FORMAT_R16G16_SINT;
		attributeDescriptions[1].offset = offsetof(cpospack6_c16tex2_bone2_type_h, TEXTURE);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 3;
		attributeDescriptions[2].format = VK_FORMAT_R16G16_SINT;
		attributeDescriptions[2].offset = offsetof(cpospack6_c16tex2_bone2_type_h, TEXTURE2);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 0;
		attributeDescriptions[3].format = VK_FORMAT_R8G8_UINT;
		attributeDescriptions[3].offset = offsetof(cpospack6_c16tex2_bone2_type_h, BONES);

		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 1;
		attributeDescriptions[4].format = VK_FORMAT_R8G8_UINT;
		attributeDescriptions[4].offset = offsetof(cpospack6_c16tex2_bone2_type_h, WEIGHTS);

		return attributeDescriptions;
	}

	bool operator==(const cpospack6_c16tex2_bone2_type_h& v)
	{
		return (v.POSITION == this->POSITION) &&
			(v.TEXTURE == this->TEXTURE) &&
			(v.TEXTURE2 == this->TEXTURE2) &&
			(v.WEIGHTS == this->WEIGHTS) &&
			(v.BONES == this->BONES);
	}

} CVertex_PosPack6_C16Tex2_Bone2;

#pragma pack(pop)