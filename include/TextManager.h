#pragma once

#include <tuple>
#include <vector>

#include "AppTexture.h"
#include "RenderInstance.h"
#include "VertexTypes.h"

#include <glm/glm.hpp>

struct Font
{
	Font() = delete;
	Font(const std::string& imageName, const std::string& dataName);

	~Font();

	void CreateFontWidths(std::vector<char>& fontData);

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
	Text(std::string& text, Font& _f, float x, float y, glm::vec4 color, size_t _buffersize);

	void CreateVertices(std::string& text, float x, float y, size_t startingString);

	void UpdateText(std::string& newText);

	void NormalizeXANDY(float x, float y, float& xA, float& xY);

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
	static void CreateFontTextManager(const std::string& imageName, const std::string& dataName);

	static void CreatePipelineObject();

	static void CreateTextBuffer();

	static void UploadToVertexBuffer(Text* text);

	static void UpdateVertexBuffer(Text* text, size_t indexInString);

	static void DrawTextTM(RecordingBufferObject& cb, uint32_t frame);

	static void DestroyTextManager();

	static constexpr uint32_t MAXTEXTRENDERABLES = 64;
	static constexpr uint32_t BUFFERSIZE = 1048576;
	static OffsetIndex bufferOffset;
	static size_t vertexCount, commandCount;
	static OffsetIndex vertexBufferOffset, indirectCommandsOffset;
	static Font* fonts;
	static VKPipelineObject* obj;
	static std::vector<std::tuple<Text*, size_t, size_t>> textsCommand;
};

