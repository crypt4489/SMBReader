#pragma once


#include "AppTexture.h"
#include "FileManager.h"
#include "RenderInstance.h"
#include "VertexTypes.h"
#include "VKPipelineObject.h"

struct Font
{
	Font() = delete;
	Font(const std::string& imageName, const std::string& dataName)
	{
		TextureIOType type = TextureIOType::BMP;

		if (imageName.ends_with(".bmp"))
		{
			type = TextureIOType::BMP;
		}

		std::vector<char> imageData;
		FileManager::ReadFileInFull(imageName, imageData);
		std::vector<char> fontData;
		FileManager::ReadFileInFull(dataName, fontData);

		texture = new AppTexture(imageData, type);
		CreateFontWidths(fontData);
	}

	~Font() {
		if (texture) delete texture;
		if (fontWidths) delete [] fontWidths;
	}

	void CreateFontWidths(std::vector<char>& fontData)
	{
		auto iter = fontData.begin();
		std::copy(iter, iter + 17, reinterpret_cast<char*>(this));
		iter += 17;
		widthSize = (pictureWidth / cellWidth) * (pictureHeight / cellHeight);
		fontWidths = new char[widthSize];
		std::copy(iter, iter + widthSize, fontWidths);
	}

	uint32_t pictureHeight, pictureWidth;
	uint32_t cellWidth, cellHeight;
	uint8_t startingChar;
	AppTexture *texture;
	char *fontWidths;
	uint32_t widthSize;
};

class Text
{
public:
	Text() = delete;
	Text(std::string& text, Font& _f, float x, float y, glm::vec4 color) : font(_f)
	{
		CreateVertices(text, x, y, color);
	}

	void CreateVertices(std::string& text, float x, float y, glm::vec4& color)
	{
		textVertices.clear();
		//textVertices.resize(text.length() * 4); // 4 verts per letter;

		float actualX, actualY;
		NormalizeXANDY(x, y, actualX, actualY);

		float lastx = actualX;
		//float lasty = actualY;

		uint32_t LETTERSPERROW = font.pictureWidth / font.cellWidth;
		for (auto& c : text)
		{
			char cLetter = (std::toupper(c) - font.startingChar);
			if (cLetter == '\n')
			{
				continue;
			}

			int letterWidth = static_cast<int>(font.fontWidths[cLetter]);
			letterWidth = std::clamp(letterWidth, 12, 12);
			
			int tx = (cLetter % LETTERSPERROW);
			int ty = (cLetter / LETTERSPERROW);

			int cx = tx * font.cellWidth;
			int cy = ty * font.cellHeight;

			int cx2 = cx + letterWidth;
			int cy2 = cy + font.cellHeight;

			float s1 = static_cast<float>(cx) / static_cast<float>(font.pictureWidth);
			float t1 = static_cast<float>(cy) / static_cast<float>(font.pictureHeight);

			float s2 = static_cast<float>(cx2) / static_cast<float>(font.pictureWidth);
			float t2 = static_cast<float>(cy2) / static_cast<float>(font.pictureHeight);
			

			float left = lastx;
			float right = lastx + (static_cast<float>(letterWidth) / static_cast<float>(font.pictureWidth));
			float top = actualY;
			float bottom = actualY + (static_cast<float>(font.cellHeight) / static_cast<float>(font.pictureHeight));;
			
			textVertices.emplace_back(glm::vec2(left, top), glm::vec2(s1, t1), color);
			textVertices.emplace_back(glm::vec2(right, top), glm::vec2(s2, t1), color);
			textVertices.emplace_back(glm::vec2(left, bottom), glm::vec2(s1, t2), color);
			textVertices.emplace_back(glm::vec2(right, bottom), glm::vec2(s2, t2), color);

			lastx = right;
		}
	}

	void NormalizeXANDY(float x, float y, float& xA, float& xY)
	{
		//for vulkan now, soon dxd12
		xA = (2 * x) - 1.0f;
		xY = (2 * y) - 1.0f;
	}

	std::vector<TextVertex> textVertices;
	Font& font;
};

class TextManager
{
public:
	static void CreateFontTextManager(const std::string& imageName, const std::string& dataName)
	{
		fonts = new Font(imageName, dataName);
		CreateTextBuffer();
		CreatePipelineObject();
	}

	static void CreatePipelineObject()
	{
		obj = new VKPipelineObject();
		obj->AddShader("text.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		obj->AddShader("text.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		obj->AddPixelShaderImageDescription(fonts->texture->vkImpl->imageView, fonts->texture->vkImpl->sampler, 0);
		obj->CreatePipelineObject(TextVertex::getBindingDescription(), TextVertex::getAttributeDescriptions());
	}

	static void CreateTextBuffer()
	{
		VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();
		VkPhysicalDevice gpu = VKRenderer::gRenderInstance->GetVulkanPhysicalDevice();
		std::tie(textBuffer, textBufferMemory) = ::VK::Utils::createBuffer(device,
			gpu, BUFFERSIZE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	}

	static void UpdateVertexBuffer(const Text& text)
	{
		VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();
		char* data;

		size_t vertsDataSize = sizeof(TextVertex) * text.textVertices.size();

		count += text.textVertices.size();

		vkMapMemory(device, textBufferMemory, 0, BUFFERSIZE, 0, reinterpret_cast<void**>(&data));

		std::memcpy(data + bufferOffset, text.textVertices.data(), vertsDataSize);

		vkUnmapMemory(device, textBufferMemory);

		bufferOffset += vertsDataSize;
	}

	static void DrawTextTM(VkCommandBuffer cb, uint32_t frame)
	{
		obj->Draw(cb, textBuffer, count, frame);
	}

	static void DestroyTextManager()
	{
		if (fonts) delete fonts;
		if (obj) delete obj;
		auto device = VKRenderer::gRenderInstance->GetVulkanDevice();
		
		if (textBufferMemory)
			vkFreeMemory(device, textBufferMemory, nullptr);

		if (textBuffer)
			vkDestroyBuffer(device, textBuffer, nullptr);

	}

	static std::vector<std::pair<uint64_t, uint32_t>> offsetsAndCount;
	static constexpr uint32_t BUFFERSIZE = 2'000'000;
	static VkBuffer textBuffer;
	static VkDeviceMemory textBufferMemory;
	static uint64_t bufferOffset;
	static uint32_t count;
	static Font* fonts;
	static VKPipelineObject* obj;
};

