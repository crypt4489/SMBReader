#pragma once

#include <tuple>

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
	char startingChar;
	AppTexture *texture;
	char *fontWidths;
	uint32_t widthSize;
};

class Text
{
public:
	Text() = delete;
	Text(std::string& text, Font& _f, float x, float y, glm::vec4 color, size_t _buffersize) 
		: font(_f), mainText(text), bufferReserved(_buffersize), textColor(color)
		, startX(x), startY(y)
	{
		CreateVertices(text, x, y, 0U);
	}

	void CreateVertices(std::string& text, float x, float y, size_t startingString)
	{
		//textVertices.clear();
		//textVertices.resize(text.length() * 4); // 4 verts per letter;
		size_t startingVertex = startingString * 4;
		float actualX, actualY;

		if (!textVertices.size())
		{
			NormalizeXANDY(x, y, actualX, actualY);
		}
		else
		{
			actualX = textVertices[startingVertex - 1].POSITION.x;
			actualY = textVertices[startingVertex - 4].POSITION.y;
			textVertices.erase(textVertices.begin() + startingVertex, textVertices.end());
		}
		
		float lastx = actualX;
		float lasty = actualY;

		uint32_t LETTERSPERROW = font.pictureWidth / font.cellWidth;

		for (auto& c : text.substr(startingString, text.size()))
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
			
			textVertices.emplace_back(glm::vec2(left, top), glm::vec2(s1, t1), textColor);
			textVertices.emplace_back(glm::vec2(left, bottom), glm::vec2(s1, t2), textColor);
			textVertices.emplace_back(glm::vec2(right, top), glm::vec2(s2, t1), textColor);
			textVertices.emplace_back(glm::vec2(right, bottom), glm::vec2(s2, t2), textColor);

			lastx = right;
		}
	}

	void UpdateText(std::string& newText)
	{
		size_t newsize = newText.size();
		size_t oldsize = mainText.size();
		size_t i = 0U;

		for (; i < oldsize && i < newsize; i++)
		{
			if (newText[i] != mainText[i])
				break;
		}

		mainText = mainText.substr(0, i) + newText.substr(i, newText.size());

		CreateVertices(mainText, 0.f, 0.f, i);
	}

	void NormalizeXANDY(float x, float y, float& xA, float& xY)
	{
		//for vulkan now, soon dxd12
		xA = (2 * x) - 1.0f;
		xY = (2 * y) - 1.0f;
	}

	std::vector<TextVertex> textVertices;
	Font& font;
	std::string mainText;
	size_t bufferReserved;
	glm::vec4 textColor;
	float startX, startY;
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
		std::string text = "text";
		obj = new VKPipelineObject(
			text, 
			&vertexCount, 
			textBuffer
		);

		auto rendInst = VKRenderer::gRenderInstance;
		uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;
		auto dlc = rendInst->GetDescriptorLayoutCache();
		auto dsc = rendInst->GetDescriptorSetCache();
		auto dl = dlc->GetLayout("oneimage");
		DescriptorSetBuilder dsb{};
		dsb.AllocDescriptorSets(rendInst->GetVulkanDevice(), rendInst->GetDescriptorPool(), dl, frames);
		dsb.AddPixelShaderImageDescription(rendInst->GetVulkanDevice(), fonts->texture->vkImpl->imageView, fonts->texture->vkImpl->sampler, 0, frames);
		dsc->AddDesciptorSet(text, dsb.descriptorSets);
	}

	static void CreateTextBuffer()
	{
		VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();
		VkPhysicalDevice gpu = VKRenderer::gRenderInstance->GetVulkanPhysicalDevice();
		std::tie(textBuffer, textBufferMemory) = ::VK::Utils::createBuffer(device,
			gpu, BUFFERSIZE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		std::tie(indirectDrawBuffer, indirectDrawBufferMemory) = ::VK::Utils::createBuffer(device,
			gpu, MAXTEXTRENDERABLES * sizeof(VkDrawIndirectCommand), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	}

	static void UploadToVertexBuffer(Text* text)
	{
		VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();
		void* data;

		size_t textVertexCount = text->textVertices.size();

		size_t vertsDataSize = sizeof(TextVertex) * textVertexCount;

		size_t allocatedVerts = 4 * text->bufferReserved;

		size_t allocatedDataSize = sizeof(TextVertex) * allocatedVerts;

		vkMapMemory(device, textBufferMemory, bufferOffset, vertsDataSize, 0, &data);

		std::memcpy(data, text->textVertices.data(), vertsDataSize);

		vkUnmapMemory(device, textBufferMemory);

		VkDrawIndirectCommand command;

		command.firstVertex = static_cast<uint32_t>(vertexCount);
		command.firstInstance = 0;
		command.instanceCount = 1;
		command.vertexCount = static_cast<uint32_t>(textVertexCount);

		vkMapMemory(device, indirectDrawBufferMemory, commandCount * sizeof(VkDrawIndirectCommand), sizeof(VkDrawIndirectCommand), 0, &data);

		std::memcpy(data, &command, sizeof(VkDrawIndirectCommand));

		vkUnmapMemory(device, indirectDrawBufferMemory);

		textsCommand.push_back({ text, commandCount++, bufferOffset });

		bufferOffset += allocatedDataSize;

		vertexCount += allocatedVerts;


	}

	static void UpdateVertexBuffer(Text* text, size_t indexInString)
	{
		size_t i = 0;
		for (auto& ref : textsCommand)
		{
			if (std::get<Text *>(ref) == text) break;
			i++;
		}

		VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();
		void* data;

		VkDeviceSize cCount, bOffset;

		std::tie(std::ignore, cCount, bOffset) = textsCommand[i];

		size_t textVertexCount = text->textVertices.size();

		size_t startingOffset = i * 4;

		size_t newCount = textVertexCount - startingOffset;

		vkMapMemory(device, textBufferMemory, bOffset + startingOffset, newCount, 0, &data);

		std::memcpy(data, text->textVertices.data() + startingOffset, newCount * sizeof(TextVertex));

		vkUnmapMemory(device, textBufferMemory);

		vkMapMemory(device, indirectDrawBufferMemory, cCount * sizeof(VkDrawIndirectCommand), sizeof(VkDrawIndirectCommand), 0, &data);

		VkDrawIndirectCommand* command = reinterpret_cast<VkDrawIndirectCommand*>(data);

		command->vertexCount = static_cast<uint32_t>(textVertexCount);

		vkUnmapMemory(device, indirectDrawBufferMemory);

	}

	static void DrawTextTM(VkCommandBuffer cb, uint32_t frame)
	{
		obj->DrawIndirectOneBuffer(cb, indirectDrawBuffer, static_cast<uint32_t>(textsCommand.size()), frame, 0);
	}

	static void DestroyTextManager()
	{
		if (fonts) delete fonts;
		if (obj) delete obj;
		auto device = VKRenderer::gRenderInstance->GetVulkanDevice();

		if (indirectDrawBufferMemory)
			vkFreeMemory(device, indirectDrawBufferMemory, nullptr);

		if (indirectDrawBuffer)
			vkDestroyBuffer(device, indirectDrawBuffer, nullptr);

		
		if (textBufferMemory)
			vkFreeMemory(device, textBufferMemory, nullptr);

		if (textBuffer)
			vkDestroyBuffer(device, textBuffer, nullptr);

	}

	static constexpr uint32_t MAXTEXTRENDERABLES = 50;
	static constexpr uint32_t BUFFERSIZE = 2'000'000;
	static VkBuffer textBuffer, indirectDrawBuffer;
	static VkDeviceMemory textBufferMemory, indirectDrawBufferMemory;
	static VkDeviceSize bufferOffset;
	static VkDeviceSize vertexCount, commandCount;
	static Font* fonts;
	static VKPipelineObject* obj;
	static std::vector<std::tuple<Text*, VkDeviceSize, VkDeviceSize>> textsCommand;
};

