#pragma once

#include <tuple>
#include <vector>

#include "RenderInstance.h"
#include "VertexTypes.h"


struct Font
{
	Font() = delete;
	Font(const std::string& imageName, const std::string& dataName);

	~Font();

	void CreateFontWidths(std::vector<char>& fontData);

	uint32_t pictureHeight, pictureWidth;
	uint32_t cellWidth, cellHeight;
	char startingChar;
	int textureHandle;
	char *fontWidths;
	uint32_t widthSize;
};

struct Text
{
public:
	Text() = delete;
	Text(std::string& text, Font& _f, float x, float y, Vector4f color, size_t _buffersize);

	void CreateVertices(std::string& text, float x, float y, size_t startingString);

	void UpdateText(std::string& newText);

	void NormalizeXANDY(float x, float y, float& xA, float& xY);

	std::vector<TextVertex> textVertices;
	Font& font;
	std::string mainText;
	size_t bufferReserved;
	Vector4f textColor;
	float startX, startY;
};

struct TextManager
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
	static size_t bufferOffset;
	static size_t vertexCount, commandCount;
	static int vertexBufferIndex, indirectCommandsIndex;
	static Font* fonts;
	static VKGraphicsPipelineObject* obj;
	static std::vector<std::tuple<Text*, size_t, size_t>> textsCommand;
	static EntryHandle descHandle;
};

