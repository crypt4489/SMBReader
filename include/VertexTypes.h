#pragma once
#include <array>
#include "glm/glm.hpp"
#include "VKUtilities.h"
typedef struct text_vertex_t
{
	text_vertex_t() = delete;
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

	vertex_type_h() = delete;
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

	basic_vertex_type_h() = delete;
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