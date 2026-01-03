#include "TextManager.h"
#include "FileManager.h"

#include "RenderInstance.h"
#include "VertexTypes.h"
#include "VKPipelineObject.h"
#include "VKDescriptorSetBuilder.h"
size_t TextManager::bufferOffset = ~0ui64;
Font* TextManager::fonts;
size_t TextManager::vertexCount = 0;
size_t TextManager::commandCount = 0;
VKGraphicsPipelineObject* TextManager::obj;
std::vector<std::tuple<Text *, size_t, size_t>> TextManager::textsCommand;
int TextManager::vertexBufferIndex = ~0i32, TextManager::indirectCommandsIndex = ~0i32;
EntryHandle TextManager::descHandle;

void TextManager::CreateFontTextManager(const std::string& imageName, const std::string& dataName)
{
	fonts = new Font(imageName, dataName);
	CreateTextBuffer();
	CreatePipelineObject();
}

void TextManager::CreatePipelineObject()
{
	std::string text = "text";

	auto rendInst = VKRenderer::gRenderInstance;


	uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;
	//DescriptorSetBuilder *dsb = rendInst->CreateDescriptorSet(rendInst->descriptorLayouts[2], frames);
	//dsb->AddPixelShaderImageDescription(rendInst->GetImageView(fonts->texture->vkImpl), rendInst->GetSampler(fonts->texture->vkImpl), 0, frames);
	//descHandle = dsb->AddDescriptorsToCache();

	VKGraphicsPipelineObjectCreateInfo create = {
		//.drawType = 1,
		.vertexBufferIndex = rendInst->allocations[vertexBufferIndex].memIndex,
		.vertexBufferOffset = static_cast<uint32_t>(rendInst->allocations[vertexBufferIndex].offset),
		.vertexCount = ~0U,
		//.pipelinename = TEXT,
		.descriptorsetid = &descHandle,
		.maxDynCap = 0,
	};
	
	//obj = new VKGraphicsPipelineObject(&create);
}

void TextManager::CreateTextBuffer()
{
	vertexBufferIndex = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(BUFFERSIZE, 0, STATIC);
	indirectCommandsIndex = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(MAXTEXTRENDERABLES * sizeof(VkDrawIndirectCommand), 0U, STATIC);
}

void TextManager::UploadToVertexBuffer(Text* text)
{

	size_t textVertexCount = text->textVertices.size();

	size_t vertsDataSize = sizeof(TextVertex) * textVertexCount;

	size_t allocatedVerts = 4 * text->bufferReserved;

	size_t allocatedDataSize = sizeof(TextVertex) * allocatedVerts;

	VKRenderer::gRenderInstance->UpdateAllocation(
		text->textVertices.data(), vertexBufferIndex, vertsDataSize,
		bufferOffset, 0, 1);

	VkDrawIndirectCommand command;

	command.firstVertex = static_cast<uint32_t>(vertexCount);
	command.firstInstance = 0;
	command.instanceCount = 1;
	command.vertexCount = static_cast<uint32_t>(textVertexCount);

	size_t commandOffset = commandCount * sizeof(VkDrawIndirectCommand);

	VKRenderer::gRenderInstance->UpdateAllocation(
		&command, indirectCommandsIndex, sizeof(VkDrawIndirectCommand),
		commandOffset, 0, 1);

	textsCommand.push_back({ text, commandCount++, bufferOffset });

	bufferOffset = bufferOffset + allocatedDataSize;

	vertexCount += allocatedVerts;

}

void TextManager::UpdateVertexBuffer(Text* text, size_t indexInString)
{
	size_t i = 0;

	for (auto& ref : textsCommand)
	{
		if (std::get<Text*>(ref) == text) break;
		i++;
	}

	VkDeviceSize cCount, bOffset;

	std::tie(std::ignore, cCount, bOffset) = textsCommand[i];

	size_t textVertexCount = text->textVertices.size();

	size_t startingOffset = i * 4;

	size_t newCount = textVertexCount - startingOffset;

	VKRenderer::gRenderInstance->UpdateAllocation(
		text->textVertices.data(), vertexBufferIndex, newCount * sizeof(TextVertex),
		 bOffset + startingOffset, 0, 1);

	VKRenderer::gRenderInstance->UpdateAllocation(
		&textVertexCount, indirectCommandsIndex, sizeof(uint32_t),
		 offsetof(VkDrawIndirectCommand, vertexCount) +
		 (cCount * sizeof(VkDrawIndirectCommand)), 0, 1);
}

void TextManager::DrawTextTM(RecordingBufferObject &cb, uint32_t frame)
{
	//obj->DrawIndirectOneBuffer(&cb, static_cast<uint32_t>(textsCommand.size()), frame, 0);
}

void TextManager::DestroyTextManager()
{
	if (fonts) delete fonts;
	if (obj) delete obj;
}


//Text

Text::Text(std::string& text, Font& _f, float x, float y, Vector4f color, size_t _buffersize)
	: font(_f), mainText(text), bufferReserved(_buffersize), textColor(color)
	, startX(x), startY(y)
{
	CreateVertices(text, x, y, 0U);
}

void Text::CreateVertices(std::string& text, float x, float y, size_t startingString)
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

	Vector4f color = { textColor.x, textColor.y, textColor.z, textColor.w };

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

		textVertices.emplace_back(Vector2f(left, top), Vector2f(s1, t1), color);
		textVertices.emplace_back(Vector2f(left, bottom), Vector2f(s1, t2),color);
		textVertices.emplace_back(Vector2f(right, top), Vector2f(s2, t1), color);
		textVertices.emplace_back(Vector2f(right, bottom), Vector2f(s2, t2), color);

		lastx = right;
	}
}

void Text::UpdateText(std::string& newText)
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

void Text::NormalizeXANDY(float x, float y, float& xA, float& xY)
{
	//for vulkan now, soon dxd12
	xA = (2 * x) - 1.0f;
	xY = (2 * y) - 1.0f;
}


Font::Font(const std::string& imageName, const std::string& dataName)
{
	TextureIOType type = TextureIOType::BMP;

	if (imageName.ends_with(".bmp"))
	{
		type = TextureIOType::BMP;
	}

	std::vector<char> imageData;
	int ret = FileManager::ReadFileInFull(imageName, imageData);
	if (ret)
	{
		throw std::runtime_error("Cannot open Font image data");
	}

	std::vector<char> fontData;
	ret = FileManager::ReadFileInFull(dataName, fontData);

	if (ret)
	{
		throw std::runtime_error("Cannot open Font Widths data");
	}

	
	CreateFontWidths(fontData);
}

Font::~Font() {

	if (fontWidths) delete[] fontWidths;
}

void Font::CreateFontWidths(std::vector<char>& fontData)
{
	auto iter = fontData.begin();
	std::copy(iter, iter + 17, reinterpret_cast<char*>(this));
	iter += 17;
	widthSize = (pictureWidth / cellWidth) * (pictureHeight / cellHeight);
	fontWidths = new char[widthSize];
	std::copy(iter, iter + widthSize, fontWidths);
}
