#include "VertexTypes.h"
#include <array>
#include "MathTypes.h"
#include "VKUtilities.h"

text_vertex_t::text_vertex_t(Vector2f _p, Vector2f _t, Vector4f _c) :
	POSITION(_p), TEXTURE(_t), COLOR(_c)

{

}

VkVertexInputBindingDescription text_vertex_t::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(text_vertex_t);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> text_vertex_t::getAttributeDescriptions() {
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

VkVertexInputBindingDescription vertex_type_h::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(vertex_type_h);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> vertex_type_h::getAttributeDescriptions() {
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

VkVertexInputBindingDescription basic_vertex_type_h::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(basic_vertex_type_h);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> basic_vertex_type_h::getAttributeDescriptions() {
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

bool basic_vertex_type_h::operator==(const basic_vertex_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE);
}

pospack6_cnorm_c16tex1_bone2_type_h::pospack6_cnorm_c16tex1_bone2_type_h(const Vector4f& _p, const Vector2f& _t, const Vector3f& _n, const Vector2i& _b, const Vector2f& _w) :
	POSITION(_p), TEXTURE(_t), NORMAL(_n), BONES(_b), WEIGHTS(_w)
{
}

VkVertexInputBindingDescription pospack6_cnorm_c16tex1_bone2_type_h::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(pospack6_cnorm_c16tex1_bone2_type_h);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> pospack6_cnorm_c16tex1_bone2_type_h::getAttributeDescriptions() {
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

VkVertexInputBindingDescription pospack6_c16tex1_bone2_type_h::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(pospack6_c16tex1_bone2_type_h);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> pospack6_c16tex1_bone2_type_h::getAttributeDescriptions() {
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

VkVertexInputBindingDescription pospack6_c16tex2_bone2_type_h::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(pospack6_c16tex2_bone2_type_h);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> pospack6_c16tex2_bone2_type_h::getAttributeDescriptions() {
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

VkVertexInputBindingDescription cpospack6_cnorm_c16tex1_bone2_type_h::getBindingDescription()
{
	VkVertexInputBindingDescription desc{};
	desc.binding = 0;
	desc.stride = sizeof(cpospack6_cnorm_c16tex1_bone2_type_h);
	desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return desc;
}

std::vector<VkVertexInputAttributeDescription> cpospack6_cnorm_c16tex1_bone2_type_h::getAttributeDescriptions()
{
	std::vector<VkVertexInputAttributeDescription> a(5);

	a[0] = { 0, 0, VK_FORMAT_R8G8_UINT,        offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, BONES) };
	a[1] = { 0, 1, VK_FORMAT_R8G8_UINT,        offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, WEIGHTS) };
	a[2] = { 0, 2, VK_FORMAT_R16G16_SINT,      offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, TEXTURE) };
	a[3] = { 0, 3, VK_FORMAT_R32_SINT,		   offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, NORMAL) };
	a[4] = { 0, 4, VK_FORMAT_R16G16B16_SINT,   offsetof(cpospack6_cnorm_c16tex1_bone2_type_h, POSITION) };

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

VkVertexInputBindingDescription cpospack6_c16tex1_bone2_type_h::getBindingDescription()
{
	VkVertexInputBindingDescription desc{};
	desc.binding = 0;
	desc.stride = sizeof(cpospack6_c16tex1_bone2_type_h);
	desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return desc;
}

std::vector<VkVertexInputAttributeDescription> cpospack6_c16tex1_bone2_type_h::getAttributeDescriptions()
{
	std::vector<VkVertexInputAttributeDescription> a(4);

	a[0] = { 0, 0, VK_FORMAT_R8G8_UINT,      offsetof(cpospack6_c16tex1_bone2_type_h, BONES) };
	a[1] = { 0, 1, VK_FORMAT_R8G8_UINT,      offsetof(cpospack6_c16tex1_bone2_type_h, WEIGHTS) };
	a[2] = { 0, 2, VK_FORMAT_R16G16_SINT,    offsetof(cpospack6_c16tex1_bone2_type_h, TEXTURE) };
	a[3] = { 0, 3, VK_FORMAT_R16G16B16_SINT, offsetof(cpospack6_c16tex1_bone2_type_h, POSITION) };

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

VkVertexInputBindingDescription cpospack6_c16tex2_bone2_type_h::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(cpospack6_c16tex2_bone2_type_h);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> cpospack6_c16tex2_bone2_type_h::getAttributeDescriptions() {
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

bool cpospack6_c16tex2_bone2_type_h::operator==(const cpospack6_c16tex2_bone2_type_h& v)
{
	return (v.POSITION == this->POSITION) &&
		(v.TEXTURE == this->TEXTURE) &&
		(v.TEXTURE2 == this->TEXTURE2) &&
		(v.WEIGHTS == this->WEIGHTS) &&
		(v.BONES == this->BONES);
}

