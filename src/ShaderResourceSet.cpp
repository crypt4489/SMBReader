#include "ShaderResourceSet.h"
#include <ctype.h>


#define INPUT_MANAGEMENT_MAX(a, b) ((a) < (b) ? (b) : (a))

#define MAX_ATTRIBUTE_LEN 50
#define MAX_VALUE_LEN 50
#define MAX_ATTRIBUTE_LINE_LEN 200

struct PipelineXMLTag
{
	unsigned long hashCode;
};

struct PipelineDescriptionXMLTag : PipelineXMLTag //followed by shaderNameLen Bytes
{
	int sampLo;
	int sampHi;
};

struct PrimitiveXMLTag : PipelineXMLTag
{
	PrimitiveType primType;
};

struct DepthXMLTag : PipelineXMLTag
{
	bool enabled;
	RasterizerTest depthOp;
};

struct CullModeXMLTag : PipelineXMLTag
{
	CullMode mode;
};

struct ShaderXMLTag
{
	unsigned long hashCode;
};

struct ShaderGLSLShaderXMLTag : ShaderXMLTag //followed by shaderNameLen Bytes
{
	ShaderStageType type;
};

struct ShaderComputeLayoutXMLTag : ShaderXMLTag
{
	ShaderComputeLayout comps;
};

struct ShaderResourceItemXMLTag : ShaderXMLTag
{
	ShaderStageType shaderstage;
	ShaderResourceType resourceType;
	ShaderResourceAction resourceAction;
	int arrayCount;
	int size;
	int offset;
	int pushRangeStage;
};

struct ShaderResourceSetXMLTag : ShaderXMLTag
{
	int resourceCount;
};

static constexpr unsigned long hash(const char* str);
static constexpr int ASCIIToInt(char* str);

static int ProcessTag(char* fileData, int size, int currentLocation, unsigned long* hash, bool* opening, Logger* scratchLogger);
static int SkipLine(char* fileData, int size, int currentLocation);
static int ReadValue(char* fileData, int size, int currentLocation, char* str, int* len, int maxStringLength, Logger* scratchLogger);
static int ReadAttributeName(char* fileData, int size, int currentLocation, unsigned long* hash, Logger* scratchLogger);
static int ReadAttributeValueHash(char* fileData, int size, int currentLocation, unsigned long* hash, Logger* scratchLogger);
static int ReadAttributeValueVal(char* fileData, int size, int currentLocation, unsigned long* val, Logger* scratchLogger);

static int ReadAttributes(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize, Logger* scratchLogger);
static int HandleGLSLShader(char* fileData, int size, int currentLocation, uintptr_t* offset, ShaderDetails* details, char* readerMemBuffer, int* readerMemBufferAllocate, Logger* scratchLogger);
static int HandleShaderResourceItem(char* fileData, int size, int currentLocation, uintptr_t* offset, char* readerMemBuffer, int* readerMemBufferAllocate, Logger* scratchLogger);
static int HandleComputeLayout(char* fileData, int size, int currentLocation, uintptr_t* offset, char* readerMemBuffer, int* readerMemBufferAllocate, Logger* scratchLogger);

static int HandlePipelineDescription(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger);
static int HandleCullMode(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger);
static int HandleDepthTest(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger);
static int HandleStencilTest(char* fileData, int size, int currentLocation, FaceStencilData* face, Logger* scratchLogger);
static int HandlePrimitiveType(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger);
static int HandleVertexComponentInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation, int perVertexSlotLocation, Logger* scratchLogger);
static int HandleVertexInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation, Logger* scratchLogger);
static int HandleBlendDetails(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger);
static int HandleBlendAttachment(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int blendAttachmentLocation, Logger* scratchLogger);



static int ReadAttributesAttachments(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize, Logger* scratchLogger);
static int HandleAttachment(char* fileData, int size, int currentLocation, AttachmentDescriptionType descType, AttachmentDescription* description, Logger* scratchLogger);
static int HandleAttachmentDesc(char* fileData, int size, int currentLocation, AttachmentRenderPass* holder, Logger* scratchLogger);
static int HandleAttachmentResource(char* fileData, int size, int currentLocation, AttachmentResource* resource, Logger* scratchLogger);

BarrierStage ConvertShaderStageToBarrierStage(ShaderStageType type)
{
	BarrierStage flags = 0;
	flags |= (VERTEX_SHADER_BARRIER) * ((type & VERTEXSHADERSTAGE) != 0);
	flags |= (FRAGMENT_BARRIER) * ((type & FRAGMENTSHADERSTAGE) != 0);
	flags |= (COMPUTE_BARRIER) * ((type & COMPUTESHADERSTAGE) != 0);
	return flags;
}

constexpr unsigned long hash(const char* str)
{
	unsigned long hash = 5381;
	int c;

	while (c = *str++) {
		if (((c - 'A') >= 0 && (c - 'Z') <= 0) || ((c - 'a') >= 0 && (c - 'z') <= 0) || ((c - '0') >= 0 && (c - '9') <= 0))
			hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

constexpr int ASCIIToInt(char* str)
{
	int c;
	int out = 0;

	while (c = *str++) {
		if ((c - '0') >= 0 && (c - '9') <= 0)
			out = (out * 10) + (c - '0');
	}

	return out;
}

int CreateShaderGraph(StringView filename, Allocator* readerMemory, ShaderGraph* graph, ShaderDetails* details, int* shaderDetailCount, Logger* outputLogger)
{
	uintptr_t* TreeNodes = (uintptr_t*)readerMemory->Allocate(sizeof(uintptr_t) * 50);
	
	int* SetNodes = (int*)readerMemory->Allocate(sizeof(int) * 20);
	
	int* ShaderRefs = SetNodes + 15;

	OSFileHandle fileHandle;

	OSOpenFile(filename.stringData, filename.charCount, READ, &fileHandle);

	int dataSize = fileHandle.fileLength;
	
	void* fileData = readerMemory->Allocate(dataSize);

	uint64_t readCount = 0;

	OSReadFile(&fileHandle, dataSize, (char*)fileData, &readCount);

	OSCloseFile(&fileHandle);

	char* dataStart = (char*)fileData;

	int shaderCount = 0;
	int shaderResourceCount = 0;
	int setCount = 0;

	int tagCount = 0;
	int curr = 0;

	int stride = 0;
	int readerSizeMultiply = (dataSize / 512) + 1;

	int readerMemBufferAllocate = 0;
	char* readerMemBuffer = (char*)readerMemory->Allocate(readerSizeMultiply * (512));

	int retCode = 0;

	while (curr < dataSize)
	{
		unsigned long hashl = 0;
		bool opening = true; 
		stride = ProcessTag(dataStart, dataSize, curr, &hashl, &opening, outputLogger);
		curr += stride;

		stride = SkipLine(dataStart, dataSize, curr);

		if (stride < 0)
		{
			retCode = -1;
			break;
		}

		if (opening)
			tagCount++;

		switch (hashl)
		{
		case hash("ShaderGraph"):
			if (!opening)
				curr = dataSize;
			break;
		case hash("GLSLShader"):
			if (opening)
			{
				ShaderDetails* ldetails = &details[shaderCount];

				stride = HandleGLSLShader(dataStart, dataSize, curr, &TreeNodes[tagCount], ldetails, readerMemBuffer, &readerMemBufferAllocate, outputLogger);
				
				ShaderRefs[shaderCount] = tagCount;
				
				shaderCount++;
			}
			break;
		case hash("ShaderResourceSet"):
			if (opening) 
			{
				SetNodes[setCount] = tagCount;
				setCount++;
			}
			break;
		case hash("ShaderResourceItem"):
			if (opening) 
			{
				stride = HandleShaderResourceItem(dataStart, dataSize, curr, &TreeNodes[tagCount], readerMemBuffer, &readerMemBufferAllocate, outputLogger);
				shaderResourceCount++;
			}
			break;
		case hash("ComputeLayout"):
			if (opening)
				stride = HandleComputeLayout(dataStart, dataSize, curr, &TreeNodes[tagCount], readerMemBuffer, &readerMemBufferAllocate, outputLogger);
			break;
		default:
			outputLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid shader graph tag"));
			stride = -1;
			break;
		}

		if (stride < 0)
		{
			retCode = -1;
			break;
		}

		curr += stride;
	}

	if (retCode)
		return retCode;

	graph->shaderMapCount = shaderCount;
	graph->resourceSetCount = setCount;
	graph->resourceCount = shaderResourceCount;

	int shaderIndex = 0;

	for (int j = 0; j < shaderCount; j++)
	{
		ShaderMap* map = &graph->shaderMaps[j];

		ShaderGLSLShaderXMLTag* tag = (ShaderGLSLShaderXMLTag*)TreeNodes[ShaderRefs[j]];
		
		ShaderStageType type = tag->type;

		if (type == ShaderStageTypeBits::COMPUTESHADERSTAGE)
		{
			ShaderComputeLayoutXMLTag* ctag = (ShaderComputeLayoutXMLTag*)TreeNodes[ShaderRefs[j] + 1];
			ShaderDetails* deats = &details[j];
			deats->computeLayout.x = ctag->comps.x;
			deats->computeLayout.y = ctag->comps.y;
			deats->computeLayout.z = ctag->comps.z;
		}

		map->type = type;
	}

	int resourceIter = 0;

	for (int i = 0; i < setCount; i++)
	{
		ShaderResourceSetTemplate* setLay = &graph->shaderResourceSetTemplates[i];
		int resIter = SetNodes[i] + 1;
		ShaderResourceItemXMLTag* tag = (ShaderResourceItemXMLTag*)TreeNodes[resIter];
		int bindingCount = 0;

		setLay->resourceStart = resourceIter;
		setLay->constantsCount = 0;
		setLay->totalSamplersCount= 0;
		setLay->constantStageCount = 0;
		setLay->totalViewsCount = 0;
		setLay->bindingCount = 0;
	
		while (resourceIter < shaderResourceCount && tag && tag->hashCode == hash("ShaderResourceItem"))
		{
			ShaderResourceTemplate* resource = &graph->shaderResources[resourceIter++];

			if (tag->resourceType == ShaderResourceType::CONSTANT_BUFFER)
			{
				resource->binding = ~0;
				setLay->constantsCount++;
				setLay->constantStageCount = INPUT_MANAGEMENT_MAX(setLay->constantStageCount, tag->pushRangeStage + 1);
			}
			else
			{
				resource->binding = bindingCount;

				int checkForUnbounded = (tag->arrayCount < 1 ? 1 : tag->arrayCount);

				if (tag->resourceType == ShaderResourceType::SAMPLERSTATE)
				{
					setLay->totalSamplersCount += checkForUnbounded;
				}
				else
				{
					setLay->totalViewsCount += checkForUnbounded;
				}

				bindingCount++;
			}

			resource->rangeIndex = tag->pushRangeStage;
			resource->arrayCount = tag->arrayCount;
			resource->stages = tag->shaderstage;
			resource->action = tag->resourceAction;
			resource->type = tag->resourceType;
			resource->arrayCount = tag->arrayCount;
			resource->set = i;
			resource->size = tag->size;
			resource->offset = tag->offset;
			
			setLay->totalResourceCount++;

			tag = (ShaderResourceItemXMLTag*)TreeNodes[++resIter];
		}

		setLay->bindingCount = setLay->totalResourceCount - setLay->constantsCount;
	}

	*shaderDetailCount = shaderCount;

	return 0;
}

int ProcessTag(char* fileData, int size, int currentLocation, unsigned long *hash, bool *opening, Logger* scratchLogger)
{
	int charCount = 0;

	char* data = fileData + currentLocation;

	unsigned long hashl = 5381;

	while (true)
	{
		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Tag exceed file size"));
			return -1;
		}

		char readChar = data[charCount];

		if (isspace(readChar))
		{
			if (hashl == 5381)
			{	
				charCount++;
				continue;
			}
			break;
		}

		if (readChar != '<' && hashl == 5381)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid tag construction"));
			return -1;
		}

		if (readChar == '<')
		{
			charCount++;
			if (data[charCount] == '/')
			{
				*opening = false;
				charCount++;
			}
			readChar = data[charCount];
		}

		if (readChar == '>')
			break;

		charCount++;

		hashl = ((hashl << 5) + hashl) + readChar;
	}

	*hash = hashl;

	return charCount;
}

int SkipLine(char* fileData, int size, int currentLocation)
{
	int charCount = 0;
	
	char* data = fileData + currentLocation;

	while ((currentLocation + charCount) < size && data[charCount++] != '\n');

	return charCount;
}

int ReadValue(char* fileData, int size, int currentLocation, char* str, int* len, int maxStringLength, Logger* scratchLogger)
{
	int memCounter = 0;
	int charCount = 0;

	char* data = fileData + currentLocation;

	while (true)
	{
		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid String exceed file size"));
			return -1;
		}
		
		if (maxStringLength == memCounter+1)
		{
			scratchLogger->AddLogMessage(LOGWARNING, STRING_VIEW_FROM_LITERAL("Invalid string size"));
			break;
		}

		char readChar = data[charCount];

		if (readChar == '\n')
			break;

		charCount++;

		if (isspace(readChar))
			continue;

		str[memCounter++] = readChar;
	}

	str[memCounter++] = 0;

	*len = memCounter;

	return charCount;
}

int ReadAttributeName(char* fileData, int size, int currentLocation, unsigned long* hash, Logger* scratchLogger)
{
	int charCount = 0;
	
	char* data = fileData + currentLocation;

	unsigned long hashl = 5381;

	while (true)
	{
		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attribute name exceed file size"));
			return -1;
		}

		if (charCount == (MAX_ATTRIBUTE_LEN + currentLocation))
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attribute name exceed max length"));
			return -1;
		}

		char readChar = data[charCount];

		if (readChar == '>')
			break;

		charCount++;

		if (readChar == '=')
			break;

		if (isspace(readChar))
			continue;

		hashl = ((hashl << 5) + hashl) + readChar;
	}
	
	*hash = hashl;

	return charCount;
}

int ReadAttributeValueHash(char* fileData, int size, int currentLocation, unsigned long* hash, Logger* scratchLogger)
{
	int charCount = 0;

	char* data = fileData + currentLocation;

	unsigned long hashl = 5381;

	while (true)
	{
		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attribute hash exceed file size"));
			return -1;
		}

		if (charCount == (MAX_ATTRIBUTE_LEN + currentLocation)) 
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attribute hash exceed maximum length"));
			return -1;
		}

		char readChar = data[charCount];

		if (readChar == '>')
			break;

		charCount++;

		if (isspace(readChar))
		{
			if (hashl == 5381)
				continue;

			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid space char inside attribute hash"));
			return -1;
		}

		if (readChar == '\"' || readChar == '\'')
		{
			if (hashl == 5381)
				continue;
			break;
		}
		
		hashl = ((hashl << 5) + hashl) + readChar;
	}

	*hash = hashl;

	return charCount;
}

int ReadAttributeValueVal(char* fileData, int size, int currentLocation, unsigned long* val, Logger* scratchLogger)
{
	int charCount = 0;

	char* data = fileData + currentLocation;

	unsigned long outVal = 0;

	bool readingVal = false;

	while (true)
	{
		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attribute value exceed file size"));
			return -1;
		}

		if (charCount == (MAX_ATTRIBUTE_LEN + currentLocation)) 
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attribute value exceed maximum length"));
			return -1;
		}

		char readChar = data[charCount];

		if (readChar == '>')
			break;

		charCount++;

		if (isspace(readChar))
		{
			if (readingVal)
			{
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid space char inside attribute value"));
				return -1;
			}

			continue;
		}

		if (readChar == '\"' || readChar == '\'')
		{
			if (readingVal)
				break;
			continue;
		}

		if ((readChar - '0') < 0 || (readChar - '9') > 0)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid non-digit char inside attribute value"));
			return -1;
		}

	
		outVal = (outVal * 10) + (readChar - '0');
		
		readingVal = true;
	}

	*val = outVal;

	return charCount;
}

int ReadAttributes(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize, Logger* scratchLogger)
{
	int hashableCount = 0;
	char* data = fileData + currentLocation;

	int charCount = 0;
	char readChar = data[charCount];

	int attrValRetCode = 0, attrNameRetCode = 0;

	while (true)
	{
		if (readChar == '>')
			break;
	
		if (charCount >= MAX_ATTRIBUTE_LINE_LEN)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("attribute line length too long"));
			return -1;
		}

		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("attribute line exceed file size"));
			return -1;
		}

		attrNameRetCode = ReadAttributeName(fileData, size, currentLocation + charCount, &hashes[hashableCount], scratchLogger);
		
		if (attrNameRetCode < 0)
			return -1;
	
		
		charCount += attrNameRetCode;
	
		switch (hashes[hashableCount])
		{
		case hash("unbounded"):
		case hash("type"):
		case hash("used"):
		case hash("rw"):
		{
			attrValRetCode = ReadAttributeValueHash(fileData, size,  currentLocation + charCount, &hashes[hashableCount + 1], scratchLogger);

			if (attrValRetCode < 0)
				return -1;

			charCount += attrValRetCode;

			break;
		}
		case hash("pushstage"):
		case hash("offset"):
		case hash("size"):
		case hash("count"):
		case hash("x"):
		case hash("y"):
		case hash("z"):
		{
			attrValRetCode = ReadAttributeValueVal(fileData, size, currentLocation + charCount, &hashes[hashableCount + 1], scratchLogger);

			if (attrValRetCode < 0)
				return -1;

			charCount += attrValRetCode;
			break;
		}
		default:
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Unhandled shader graph attribute name"));
			return -1;
		}

		readChar = data[charCount];

		hashableCount += 2;
	}

	*stackSize = hashableCount;

	while (readChar != '\n' && charCount < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + charCount) < size)
	{
		readChar = data[charCount++];
	}

	return charCount;
}



int HandleGLSLShader(char* fileData, int size, int currentLocation, uintptr_t* offset, ShaderDetails* details, char* readerMemBuffer, int* readerMemBufferAllocate, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int glslSize = 0;

	int charStride = ReadAttributes(fileData, size, currentLocation, hashes, &glslSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	details->glslShaderNameSize = 0;

	ShaderGLSLShaderXMLTag* tag = (ShaderGLSLShaderXMLTag*)&readerMemBuffer[*readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("GLSLShader");

	int stackIter = 0;

	int retCode = 0;

	while (glslSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];
	
		switch (code)
		{
		case hash("type"):
		{
			switch (codeV)
			{
			case hash("compute"):
				tag->type = ShaderStageTypeBits::COMPUTESHADERSTAGE;
				break;
			case hash("vert"):
				tag->type = ShaderStageTypeBits::VERTEXSHADERSTAGE;
				break;
			case hash("fragment"):
				tag->type = ShaderStageTypeBits::FRAGMENTSHADERSTAGE;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid GLSL Shader Type"));
				retCode = -1;
				break;
			}
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	*readerMemBufferAllocate += sizeof(ShaderGLSLShaderXMLTag);

	int shaderNameCode = ReadValue(fileData, size, currentLocation + charStride, details->glslShaderName, &details->glslShaderNameSize, sizeof(details->glslShaderName), scratchLogger);

	if (shaderNameCode > 0)
	{
		charStride += shaderNameCode;
	}
	else
	{
		return -1;
	}

	return charStride;
}

int HandleShaderResourceItem(char* fileData, int size, int currentLocation, uintptr_t* offset, char* readerMemBuffer, int* readerMemBufferAllocate,  Logger* scratchLogger)
{
	unsigned long hashes[14];

	int dataSize = 0;

	int charStride = ReadAttributes(fileData, size, currentLocation, hashes, &dataSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	ShaderResourceItemXMLTag* tag = (ShaderResourceItemXMLTag*)&readerMemBuffer[*readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("ShaderResourceItem");

	tag->arrayCount = 1;

	int stackIter = 0;

	int retCode = 0;

	while (dataSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];
		switch (code)
		{
		case hash("type"):
		{
			switch (codeV)
			{
			case hash("samplerCube"):
				tag->resourceType = ShaderResourceType::SAMPLERCUBE;
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
				break;
			case hash("sampler2d"):
				tag->resourceType = ShaderResourceType::SAMPLER2D;
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
				break;
			case hash("storageImage2D"):
				tag->resourceType = ShaderResourceType::IMAGESTORE2D;
				break;
			case hash("image2D"):
				tag->resourceType = ShaderResourceType::IMAGE2D;
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
				break;
			case hash("sampler"):
				tag->resourceType = ShaderResourceType::SAMPLERSTATE;
				break;
			case hash("storage"):
				tag->resourceType = ShaderResourceType::STORAGE_BUFFER;
				break;
			case hash("uniform"):
				tag->resourceType = ShaderResourceType::UNIFORM_BUFFER;
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
				break;
			case hash("constantbuffer"):
				tag->resourceType = ShaderResourceType::CONSTANT_BUFFER;
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
				break;
			case hash("sampler3d"):
				tag->resourceType = ShaderResourceType::SAMPLER3D;
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
				break;
			case hash("bufferView"):
				tag->resourceType = ShaderResourceType::BUFFER_VIEW;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Resource Type"));
				retCode = -1;
			}

			break;
		}
		case hash("used"):
		{
			switch (codeV)
			{
			case hash("c"):
				tag->shaderstage = ShaderStageTypeBits::COMPUTESHADERSTAGE;
				break;
			case hash("v"):
				tag->shaderstage = ShaderStageTypeBits::VERTEXSHADERSTAGE;
				break;
			case hash("f"):
				tag->shaderstage = ShaderStageTypeBits::FRAGMENTSHADERSTAGE;
				break;
			case hash("vf"):
				tag->shaderstage = ShaderStageTypeBits::VERTEXSHADERSTAGE | ShaderStageTypeBits::FRAGMENTSHADERSTAGE;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Used Type"));
				retCode = -1;
			}
			break;
		}
		case hash("rw"):
		{
			switch (codeV)
			{
			case hash("read"):
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
				break;
			case hash("write"):
				tag->resourceAction = ShaderResourceAction::SHADERWRITE;
				break;
			case hash("readwrite"):
				tag->resourceAction = ShaderResourceAction::SHADERREADWRITE;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Action Type"));
				retCode = -1;
			}
			break;
		}
		case hash("unbounded"):
		{
			if (codeV == hash("true"))
				tag->arrayCount |= (UNBOUNDED_DESCRIPTOR_ARRAY);
			break;
		}
		case hash("count"):
		{
			tag->arrayCount |= (codeV & DESCRIPTOR_COUNT_MASK);
			break;
		}
		case hash("size"):
		{
			tag->size = codeV;
			break;
		}
		case hash("offset"):
		{
			tag->offset = codeV;
			break;
		}
		case hash("pushstage"):
		{
			tag->pushRangeStage = codeV;
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	*readerMemBufferAllocate += sizeof(ShaderResourceItemXMLTag);

	return charStride;
}

int HandleComputeLayout(char* fileData, int size, int currentLocation, uintptr_t* offset, char* readerMemBuffer, int* readerMemBufferAllocate, Logger* scratchLogger)
{
	unsigned long hashesAndVals[6];

	int dataSize = 0;

	int charStride = ReadAttributes(fileData, size, currentLocation, hashesAndVals, &dataSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	ShaderComputeLayoutXMLTag* tag = (ShaderComputeLayoutXMLTag*)&readerMemBuffer[*readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("ComputeLayout");

	int stackIter = 0;

	while (dataSize > stackIter)
	{
		unsigned long code = hashesAndVals[stackIter];
		unsigned long comp = hashesAndVals[stackIter + 1];

		switch (code)
		{
		case hash("x"):
		{
			tag->comps.x = comp;
			break;
		}
		case hash("y"):
		{
			tag->comps.y = comp;
			break;
		}
		case hash("z"):
		{
			tag->comps.z = comp;
			break;
		}
		}

		stackIter += 2;
	}

	*readerMemBufferAllocate += sizeof(ShaderComputeLayoutXMLTag);

	return charStride;
}

int CreatePipelineDescription(StringView filename, GenericPipelineStateInfo* stateInfo, Allocator* tempAllocator, Logger* outputLogger)
{
	int retCode = 0;

	memset(stateInfo, 0, sizeof(GenericPipelineStateInfo));

	OSFileHandle fileHandle;

	OSOpenFile(filename.stringData, filename.charCount, READ, &fileHandle);

	int dataSize = fileHandle.fileLength;

	void* fileData = tempAllocator->Allocate(dataSize);

	uint64_t readCount = 0;

	OSReadFile(&fileHandle, dataSize, (char*)fileData, &readCount);

	OSCloseFile(&fileHandle);

	char* dataStart = (char*)fileData;

	int tagCount = 0;
	int curr = 0;

	int stride = 0;

	int currentVertexInput = 0;
	int currentVertexInputDescription = 0;

	int blendAttachmentCount = 0;

	stateInfo->depthEnable = false;
	stateInfo->depthWrite = false;
	stateInfo->StencilEnable = false;

	while (curr < dataSize)
	{
		unsigned long hashl = 0;
		
		bool opening = true;

		stride = ProcessTag(dataStart, dataSize, curr, &hashl, &opening, outputLogger);

		if (stride < 0)
		{
			retCode = -1;
			break;
		}

		curr += stride;

		stride = SkipLine(dataStart, dataSize, curr);

		if (opening)
			tagCount++;

		switch (hashl)
		{
		case hash("PipelineDescription"):
			if (opening)
			{
				stride = HandlePipelineDescription(dataStart, dataSize, curr, stateInfo, outputLogger);
			}
			break;
		case hash("DepthTest"):
			if (opening)
			{
				stride = HandleDepthTest(dataStart, dataSize, curr, stateInfo, outputLogger);
			}
			break;
		case hash("PrimitiveType"):
			if (opening)
			{
				stride = HandlePrimitiveType(dataStart, dataSize, curr, stateInfo, outputLogger);
			}
			break;
	
		case hash("CullMode"):
			if (opening)
			{
				stride = HandleCullMode(dataStart, dataSize, curr, stateInfo, outputLogger);
			}
			break;
		case hash("VertexInput"):
		{
			if (opening)
			{
				currentVertexInputDescription = 0;
				stateInfo->vertexBufferDescCount++;
				stride = HandleVertexInput(dataStart, dataSize, curr, stateInfo, currentVertexInput, outputLogger);
			}
			else
			{
				currentVertexInput++;
			}
			break;
		}
		case hash("VertexComponent"):
		{
			if (opening)
			{
				stateInfo->vertexBufferDesc[currentVertexInput].descCount++;
				stride = HandleVertexComponentInput(dataStart, dataSize, curr, stateInfo, currentVertexInput, currentVertexInputDescription, outputLogger);
			}
			else
			{
				currentVertexInputDescription++;
			}
			break;
		}
		case hash("FrontStencilTest"):
		{
			if (opening)
			{
				stateInfo->StencilEnable = true;
				stride = HandleStencilTest(dataStart, dataSize, curr, &stateInfo->frontFace, outputLogger);
			}
			break;
		}
		case hash("BackStencilTest"):
		{
			if (opening)
			{
				stateInfo->StencilEnable = true;
				stride = HandleStencilTest(dataStart, dataSize, curr, &stateInfo->backFace, outputLogger);
			}
			break;
		}
		case hash("BlendDetails"):
		{
			if (opening)
			{
				stride = HandleBlendDetails(dataStart, dataSize, curr, stateInfo, outputLogger);
			}
			break;
		}
		case hash("BlendAttachment"):
		{
			if (opening)
			{
				stride = HandleBlendAttachment(dataStart, dataSize, curr, stateInfo, blendAttachmentCount, outputLogger);
			}
			else
			{
				blendAttachmentCount++;
			}
			break;
		}
		default:
			outputLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid pipeline state tag"));
			stride = -1;
		}

		if (stride < 0)
		{
			retCode = -1;
			break;
		}

		curr += stride;
	}

	stateInfo->blendAttachmentCount = blendAttachmentCount;

	return retCode;
}

int ReadAttributesPipeline(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize, Logger* scratchLogger)
{
	int hashableCount = 0;
	char* data = fileData + currentLocation;

	int charCount = 0;
	char readChar = data[charCount];

	int attrValRetCode = 0, attrNameRetCode = 0;

	while (true)
	{
		if (readChar == '>')
			break;

		if (charCount >= MAX_ATTRIBUTE_LINE_LEN)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("attribute line length too long"));
			return -1;
		}

		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("attribute line exceed file size"));
			return -1;
		}

		attrNameRetCode = ReadAttributeName(fileData, size, currentLocation + charCount, &hashes[hashableCount], scratchLogger);

		if (attrNameRetCode < 0)
			return attrNameRetCode;


		charCount += attrNameRetCode;

		switch (hashes[hashableCount])
		{
		case hash("op"):
		case hash("type"):
		case hash("mode"):
		case hash("format"):
		case hash("usage"):
		case hash("rate"):
		case hash("winding"):
		case hash("write"):
		case hash("failop"):
		case hash("passop"):
		case hash("depthfailop"):
		case hash("compareop"):
		case hash("enable"):
		case hash("srcColor"):
		case hash("dstColor"):
		case hash("colorOp"):
		case hash("alphaOp"):
		case hash("srcAlpha"):
		case hash("dstAlpha"):
		case hash("logicOp"):
		{
			attrValRetCode = ReadAttributeValueHash(fileData, size, currentLocation + charCount, &hashes[hashableCount + 1], scratchLogger);

			if (attrValRetCode < 0)
				return attrValRetCode;

			charCount += attrValRetCode;

			break;
		}
		case hash("writemask"):
		case hash("comparemask"):
		case hash("ref"):
		case hash("offset"):
		case hash("sampLo"):
		case hash("sampHi"):
		case hash("size"):
		{
			attrValRetCode = ReadAttributeValueVal(fileData, size, currentLocation + charCount, &hashes[hashableCount + 1], scratchLogger);

			if (attrValRetCode < 0)
				return attrValRetCode;

			charCount += attrValRetCode;

			break;
		}
		default:
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Unhandled pipeline attribute name"));
			return -1;
		}

		readChar = data[charCount];
		
		hashableCount += 2;
	}

	*stackSize = hashableCount;

	while (readChar != '\n' && charCount < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + charCount) < size)
	{
		readChar = data[charCount++];
	}

	return charCount;
}

int HandlePipelineDescription(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("sampLo"):
		{
			stateInfo->sampleCountLow = codeV;
			break;
		}
		case hash("sampHi"):
		{
			stateInfo->sampleCountHigh = codeV;
			break;
		}
		}

		stackIter += 2;
	}

	return charStride;
}

int HandleCullMode(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("mode"):
		{
			switch (codeV)
			{
			case hash("none"):
				stateInfo->cullMode = CullMode::CULL_NONE;
				break;
			case hash("back"):
				stateInfo->cullMode = CullMode::CULL_BACK;
				break;
			case hash("front"):
				stateInfo->cullMode = CullMode::CULL_FRONT;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Cull Mode"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("winding"):
		{
			switch (codeV)
			{
			case hash("cw"):
				stateInfo->windingOrder = TriangleWinding::CW;
				break;

			case hash("ccw"):
				stateInfo->windingOrder = TriangleWinding::CCW;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Winding"));
				retCode = -1;
				break;
			}
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

int HandleDepthTest(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	stateInfo->depthEnable = true;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("write"):
		{
			switch (codeV)
			{
			case hash("true"):
			{
				stateInfo->depthWrite = true;
				break;
			}
			case hash("false"):
			{
				stateInfo->depthWrite = false;
				break;
			}
			default:
			{
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid depth write boolean"));
				retCode = -1;
				break;
			}
			}
			break;
		}

		case hash("op"):
		{
			switch (codeV)
			{
			case hash("never"):
				stateInfo->depthTest = NEVER;
				break;

			case hash("less"):
				stateInfo->depthTest = LESS;
				break;

			case hash("equal"):
				stateInfo->depthTest = EQUAL;
				break;

			case hash("lessequal"):
				stateInfo->depthTest = LESSEQUAL;
				break;

			case hash("greater"):
				stateInfo->depthTest = GREATER;
				break;

			case hash("notequal"):
				stateInfo->depthTest = NOTEQUAL;
				break;

			case hash("greaterequal"):
				stateInfo->depthTest = GREATEREQUAL;
				break;

			case hash("always"):
				stateInfo->depthTest = ALLPASS;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Depth Compare Op"));
				retCode = -1;
				break;
			}
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

static StencilOp ParseStencilOp(unsigned long codeV)
{
	switch (codeV)
	{
	case hash("replace"): return StencilOp::REPLACE;
	case hash("keep"):    return StencilOp::KEEP;
	case hash("zero"):    return StencilOp::ZERO;
	default:              return StencilOp::KEEP;
	}
}

int HandleStencilTest(char* fileData, int size, int currentLocation, FaceStencilData* face, Logger* scratchLogger)
{
	unsigned long hashes[14];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("failop"):
			face->failOp = ParseStencilOp(codeV);
			break;
		case hash("passop"):
			face->passOp = ParseStencilOp(codeV);
			break;
		case hash("depthfailop"):
			face->depthFailOp = ParseStencilOp(codeV);
			break;
		case hash("compareop"):
		{
			switch (codeV)
			{
			case hash("never"):
				face->stencilCompare = NEVER;
				break;
			case hash("less"):
				face->stencilCompare = LESS;
				break;
			case hash("equal"):
				face->stencilCompare = EQUAL;
				break;
			case hash("lessequal"):
				face->stencilCompare = LESSEQUAL;
				break;
			case hash("greater"):
				face->stencilCompare = GREATER;
				break;
			case hash("notequal"):
				face->stencilCompare = NOTEQUAL;
				break;
			case hash("greaterequal"):
				face->stencilCompare = GREATEREQUAL;
				break;
			case hash("always"):
				face->stencilCompare = ALLPASS;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Stencil Compare Op"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("writemask"):
		{
			face->writeMask = codeV;
			break;
		}
		case hash("comparemask"):
		{
			face->compareMask = codeV;
			break;
		}
		case hash("ref"):
		{
			face->reference = codeV;
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

int HandlePrimitiveType(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("type"):
		{
			switch (codeV)
			{
			case hash("trilists"):
				stateInfo->primType = TRIANGLES;
				break;
			case hash("tristrips"):
				stateInfo->primType = TRISTRIPS;
				break;
			case hash("trifans"):
				stateInfo->primType = TRIFAN;
				break;
			case hash("points"):
				stateInfo->primType = POINTSLIST;
				break;
			case hash("linelists"):
				stateInfo->primType = LINELIST;
				break;
			case hash("linestrips"):
				stateInfo->primType = LINESTRIPS;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Primitive Type"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("size"):
		{
			stateInfo->lineWidth = (float)codeV;
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return ret;
}

int HandleVertexComponentInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation, int perVertexSlotLocation, Logger* scratchLogger)
{
	unsigned long hashes[8];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("usage"):
		{
			switch (codeV)
			{
			case hash("POS0"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::POSITION;
				break;
			case hash("TEX0"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::TEX0;
				break;
			case hash("TEX1"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::TEX1;
				break;
			case hash("TEX2"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::TEX2;
				break;
			case hash("TEX3"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::TEX3;
				break;
			case hash("NORMAL"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::NORMAL;
				break;
			case hash("BONES"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::BONES;
				break;
			case hash("WEIGHTS"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::WEIGHTS;
				break;
			case hash("COLOR0"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].vertexusage = VertexUsage::COLOR0;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid vertex usage"));
				retCode = -1;
				break;
			}
			break;
		}

		case hash("format"):
		{
			switch (codeV)
			{
			case hash("uint"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R32_UINT;
				break;
			case hash("int"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R32_SINT;
				break;
			case hash("vec4"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R32G32B32A32_SFLOAT;
				break;
			case hash("vec3"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R32G32B32_SFLOAT;
				break;
			case hash("vec2"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R32G32_SFLOAT;
				break;
			case hash("float"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R32_SFLOAT;
				break;
			case hash("ivec2"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R32G32_SINT;
				break;
			case hash("u8vec2"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R8G8_UINT;
				break;
			case hash("i16vec2"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R16G16_SINT;
				break;
			case hash("i16vec3"):
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].format = ComponentFormatType::R16G16B16_SINT;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid vertex format"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("offset"):
		{
			stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].byteoffset = codeV;
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

int HandleVertexInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);
	
	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("rate"):
		{
			switch (codeV)
			{
			case hash("vertex"):
			{
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].rate = VertexBufferRate::PERVERTEX;
				break;
			}
			case hash("instance"):
			{
				stateInfo->vertexBufferDesc[vertexBufferInputLocation].rate = VertexBufferRate::PERINSTANCE;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid vertex rate"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("size"):
		{
			stateInfo->vertexBufferDesc[vertexBufferInputLocation].perInputSize = codeV;
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

static int HandleBlendDetails(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("logicOp"):
		{
			switch (codeV)
			{
			case hash("copy"):
			{
				stateInfo->blendOp = BlendLogicOp::LOGIC_COPY;
				break;
			}
			case hash("and"):
			{
				stateInfo->blendOp = BlendLogicOp::LOGIC_AND;
				break;
			}
			case hash("clear"):
			{
				stateInfo->blendOp = BlendLogicOp::LOGIC_CLEAR;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid vertex rate"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("enable"):
		{
			switch (codeV)
			{
			case hash("true"):
			{
				stateInfo->blendEnable = true;
				break;
			}
			default:
				stateInfo->blendEnable = false;
				break;
			}
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

static int HandleBlendAttachment(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int blendAttachmentLocation, Logger* scratchLogger)
{
	unsigned long hashes[14];

	int attrSize = 0;

	int charStride = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("srcColor"):
		{
			switch (codeV)
			{
			case hash("zero"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_ZERO;
				break;
			}
			case hash("one"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_ONE;
				break;
			}
			case hash("srcColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_SRC_COLOR;
				break;
			}
			case hash("dstColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_DST_COLOR;
				break;
			}
			case hash("srcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_SRC_ALPHA;
				break;
			}
			case hash("dstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_DST_ALPHA;
				break;
			}
			case hash("minusSrcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			}
			case hash("minusDstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcColorFactor = BlendFactor::FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Source Color Factor"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("dstColor"):
		{
			switch (codeV)
			{
			case hash("zero"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_ZERO;
				break;
			}
			case hash("one"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_ONE;
				break;
			}
			case hash("srcColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_SRC_COLOR;
				break;
			}
			case hash("dstColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_DST_COLOR;
				break;
			}
			case hash("srcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_SRC_ALPHA;
				break;
			}
			case hash("dstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_DST_ALPHA;
				break;
			}
			case hash("minusSrcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			}
			case hash("minusDstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstColorFactor = BlendFactor::FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Dst Color Factor"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("colorOp"):
		{
			switch (codeV)
			{
			case hash("add"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].colorOp = BlendOp::BLEND_ADD;
				break;
			}
			case hash("sub"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].colorOp = BlendOp::BLEND_SUB;
				break;
			}
			case hash("reverseSub"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].colorOp = BlendOp::BLEND_REVERSE_SUB;
				break;
			}
			case hash("min"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].colorOp = BlendOp::BLEND_MIN;
				break;
			}
			case hash("max"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].colorOp = BlendOp::BLEND_MAX;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Color Op"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("srcAlpha"):
		{
			switch (codeV)
			{
			case hash("zero"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_ZERO;
				break;
			}
			case hash("one"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_ONE;
				break;
			}
			case hash("srcColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_SRC_COLOR;
				break;
			}
			case hash("dstColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_DST_COLOR;
				break;
			}
			case hash("srcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_SRC_ALPHA;
				break;
			}
			case hash("dstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_DST_ALPHA;
				break;
			}
			case hash("minusSrcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			}
			case hash("minusDstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].srcAlphaFactor = BlendFactor::FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Source Alpha Factor"));
				retCode = -1;
				break;
			}
			break;
		
		}
		case hash("dstAlpha"):
		{
			switch (codeV)
			{
			case hash("zero"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_ZERO;
				break;
			}
			case hash("one"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_ONE;
				break;
			}
			case hash("srcColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_SRC_COLOR;
				break;
			}
			case hash("dstColor"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_DST_COLOR;
				break;
			}
			case hash("srcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_SRC_ALPHA;
				break;
			}
			case hash("dstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_DST_ALPHA;
				break;
			}
			case hash("minusSrcAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_ONE_MINUS_SRC_ALPHA;
				break;
			}
			case hash("minusDstAlpha"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].dstAlphaFactor = BlendFactor::FACTOR_ONE_MINUS_DST_ALPHA;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Dest Alpha Factor"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("alphaOp"):
		{
			switch (codeV)
			{
			case hash("add"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].alphaOp = BlendOp::BLEND_ADD;
				break;
			}
			case hash("sub"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].alphaOp = BlendOp::BLEND_SUB;
				break;
			}
			case hash("reverseSub"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].alphaOp = BlendOp::BLEND_REVERSE_SUB;
				break;
			}
			case hash("min"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].alphaOp = BlendOp::BLEND_MIN;
				break;
			}
			case hash("max"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].alphaOp = BlendOp::BLEND_MAX;
				break;
			}
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid Alpha Op"));
				retCode = -1;
				break;
			}
			break;
			break;
		}

		case hash("enable"):
		{
			switch (codeV)
			{
			case hash("true"):
			{
				stateInfo->blendAttachments[blendAttachmentLocation].blendingEnabled = true;
				break;
			}
			default:
				stateInfo->blendAttachments[blendAttachmentLocation].blendingEnabled = false;
				break;
			}
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

int CreateAttachmentGraphFromFile(StringView filename, AttachmentGraph* graph, Allocator* inputScratchAllocator, Logger* outputLogger)
{
	int retCode = 0;

	OSFileHandle fileHandle;

	OSOpenFile(filename.stringData, filename.charCount, READ, &fileHandle);

	int dataSize = fileHandle.fileLength;

	void* data = inputScratchAllocator->Allocate(dataSize);

	uint64_t readCount = 0;

	OSReadFile(&fileHandle, dataSize, (char*)data, &readCount);

	OSCloseFile(&fileHandle);

	char* dataStart = (char*)data;

	int attachmentCounter = 0;
	int resourceCounter = 0;
	int holderCounter = 0;

	int depthStencilCount = 0;
	int colorCount = 0;
	int resolveCount = 0;

	int tagCount = 0;
	int curr = 0;

	int stride = 0;

	AttachmentRenderPass* currentHolder = &graph->holders[0];

	while (curr < dataSize)
	{
		unsigned long hashl = 0;
		bool opening = true;
		stride = ProcessTag(dataStart, dataSize, curr, &hashl, &opening, outputLogger);

		if (stride < 0) 
		{
			retCode = -1;
			break;
		}

		curr += stride;

		stride = SkipLine(dataStart, dataSize, curr);

		if (opening)
			tagCount++;
		
		if (opening)
		{
			switch (hashl)
			{
			case hash("Attachments"):
				currentHolder = &graph->holders[holderCounter++];
				break;
			case hash("ColorAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::COLORATTACH, &currentHolder->descs[attachmentCounter], outputLogger);
				attachmentCounter++;
				colorCount++;
				break;
			case hash("DepthAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::DEPTHATTACH, &currentHolder->descs[attachmentCounter], outputLogger);
				attachmentCounter++;
				depthStencilCount++;
				break;
			case hash("ResolveAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::RESOLVEATTACH, &currentHolder->descs[attachmentCounter], outputLogger);
				attachmentCounter++;
				resolveCount++;
				break;
			case hash("StencilAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::STENCILATTACH, &currentHolder->descs[attachmentCounter], outputLogger);
				attachmentCounter++;
				depthStencilCount++;
				break;
			case hash("DepthStencilAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::DEPTHSTENCILATTACH, &currentHolder->descs[attachmentCounter], outputLogger);
				attachmentCounter++;
				depthStencilCount++;
				break;
			case hash("AttachmentResource"):
				stride = HandleAttachmentResource(dataStart, dataSize, curr, &graph->resources[resourceCounter], outputLogger);
				resourceCounter++;
				break;
			default:
				outputLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attachment tag type"));
				stride = -1;
				break;
			}
		}
		else
		{
			switch (hashl)
			{
			case hash("Attachments"):
				if (currentHolder)
				{
					currentHolder->attachmentCount = attachmentCounter;
					currentHolder->colorCount = colorCount;
					currentHolder->depthStencilCount = depthStencilCount;
					currentHolder->resolveCount = resolveCount;
				}

				attachmentCounter = 0;
				depthStencilCount = 0;
				colorCount = 0;
				resolveCount = 0;
				currentHolder = nullptr;
				break;
			}
		}

		if (stride < 0)
		{
			retCode = -1;
			break;
		}

		curr += stride;
	}

	graph->resourceCount = resourceCounter;

	graph->passesCount = holderCounter;

	return retCode;
}

int HandleAttachment(char* fileData, int size, int currentLocation, AttachmentDescriptionType descType, AttachmentDescription* description, Logger* scratchLogger)
{
	unsigned long hashes[16];

	int attrSize = 0;

	int charStride = ReadAttributesAttachments(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	description->attachType = descType;
	
	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];
		 //loadOp = "clear" storeOp = "discard" layout = "dsv"
		switch (code)
		{
		case hash("loadOp"):
		{
			switch (codeV)
			{
			case hash("clear"):
				description->loadOp = AttachmentLoadUsage::ATTACHCLEAR;
				break;
			case hash("nocare"):
				description->loadOp = AttachmentLoadUsage::ATTACHNOCARE;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attachment load op"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("storeOp"):
		{
			switch (codeV)
			{
			case hash("discard"):
				description->storeOp = AttachmentStoreUsage::ATTACHDISCARD;
				break;
			case hash("store"):
				description->storeOp = AttachmentStoreUsage::ATTACHSTORE;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attachment store op"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("srcLayout"):
		{
			switch (codeV)
			{
			case hash("present"):
				description->srcLayout = ImageLayout::PRESENT;
				break;
			case hash("dv"):
				description->srcLayout = ImageLayout::DEPTHATTACHMENT;
				break;
			case hash("sv"):
				description->srcLayout = ImageLayout::STENCILATTACHMENT;
				break;
			case hash("dsv"):
				description->srcLayout = ImageLayout::DEPTHSTENCILATTACHMENT;
				break;
			case hash("rtv"):
				description->srcLayout = ImageLayout::COLORATTACHMENT;
				break;
			case hash("shader"):
				description->srcLayout = ImageLayout::SHADERREADABLE;
				break;
			case hash("undefined"):
				description->srcLayout = ImageLayout::UNDEFINED;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attachment source layout"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("dstLayout"):
		{
			switch (codeV)
			{
			case hash("present"):
				description->dstLayout = ImageLayout::PRESENT;
				break;
			case hash("dv"):
				description->dstLayout = ImageLayout::DEPTHATTACHMENT;
				break;
			case hash("sv"):
				description->dstLayout = ImageLayout::STENCILATTACHMENT;
				break;
			case hash("dsv"):
				description->dstLayout = ImageLayout::DEPTHSTENCILATTACHMENT;
				break;
			case hash("rtv"):
				description->dstLayout = ImageLayout::COLORATTACHMENT;
				break;
			case hash("shader"):
				description->dstLayout = ImageLayout::SHADERREADABLE;
				break;
			case hash("undefined"):
				description->dstLayout = ImageLayout::UNDEFINED;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attachment dest layout"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("resource"):
		{
			description->resourceIndex = codeV;
			break;
		}
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

int HandleAttachmentDesc(char* fileData, int size, int currentLocation, AttachmentRenderPass* holder, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int charStride = ReadAttributesAttachments(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];
		/*
		switch (code)
		{

		case hash("imageCount"):
		{
			switch (codeV)
			{
			case hash("swc"):
				holder->rpType = RenderPassType::SWAPCHAIN_IMAGE_COUNT;
				break;
			case hash("sys"):
				holder->rpType = RenderPassType::PER_FRAME_IMAGE_COUNT;
				break;
			}
			break;
		}
		}
		*/
		stackIter += 2;
	}

	return charStride;
}

int HandleAttachmentResource(char* fileData, int size, int currentLocation, AttachmentResource* resource, Logger* scratchLogger)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int charStride = ReadAttributesAttachments(fileData, size, currentLocation, hashes, &attrSize, scratchLogger);

	if (charStride < 0)
		return charStride;

	int stackIter = 0;

	int retCode = 0;

	resource->msaa = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];

		switch (code)
		{
		case hash("imageType"):
		{
			switch (codeV)
			{
			case hash("static"):
				resource->viewType = AttachmentViewType::STATIC;
				break;
			case hash("swc"):
				resource->viewType = AttachmentViewType::SWAPCHAIN;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attachment image type"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("format"):
		{
			switch (codeV)
			{
			case hash("b8g8r8a8"):
				resource->format = ImageFormat::B8G8R8A8;
				break;
			case hash("d24s8"):
				resource->format = ImageFormat::D24UNORMS8STENCIL;
				break;
			case hash("d32f"):
				resource->format = ImageFormat::D32FLOAT;
				break;
			default:
				scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Invalid attachment image format"));
				retCode = -1;
				break;
			}
			break;
		}
		case hash("msaa"):
		{
			resource->msaa = 1;
			break;
		}
		/*
		case hash("height"):
		{
			resource->height = codeV;
			break;
		}
		case hash("width"):
		{
			resource->width = codeV;
			break;
		}
		*/
		}

		if (retCode)
			return retCode;

		stackIter += 2;
	}

	return charStride;
}

int ReadAttributesAttachments(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize, Logger* scratchLogger)
{
	int hashableCount = 0;
	char* data = fileData + currentLocation;

	int charCount = 0;
	char readChar = data[charCount];

	int attrValRetCode = 0, attrNameRetCode = 0;

	while (true)
	{
		if (readChar == '>')
			break;

		if (charCount >= MAX_ATTRIBUTE_LINE_LEN)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("attribute line length too long"));
			return -1;
		}

		if ((currentLocation + charCount) >= size)
		{
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("attribute line exceed file size"));
			return -1;
		}

		attrNameRetCode = ReadAttributeName(fileData, size, currentLocation + charCount, &hashes[hashableCount], scratchLogger);

		if (attrNameRetCode < 0)
			return attrNameRetCode;


		charCount += attrNameRetCode;

		switch (hashes[hashableCount])
		{
		case hash("imageType"):
		case hash("loadOp"):
		case hash("storeOp"):
		case hash("srcLayout"):
		case hash("dstLayout"):
		case hash("format"):
		case hash("msaa"):
		{
			attrValRetCode = ReadAttributeValueHash(fileData, size, currentLocation + charCount, &hashes[hashableCount + 1], scratchLogger);

			if (attrValRetCode < 0)
				return attrValRetCode;

			charCount += attrValRetCode;

			break;
		}
		case hash("resource"):
		//case hash("height"):
		//case hash("width"):
		{
			attrValRetCode = ReadAttributeValueVal(fileData, size, currentLocation + charCount, &hashes[hashableCount + 1], scratchLogger);

			if (attrValRetCode < 0)
				return attrValRetCode;

			charCount += attrValRetCode;

			break;
		}
		default:
			scratchLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Unhandled Attachment attribute name"));
			return -1;
		}

		readChar = data[charCount];

		hashableCount += 2;
	}

	*stackSize = hashableCount;

	while (readChar != '\n' && charCount < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + charCount) < size)
	{
		readChar = data[charCount++];
	}

	return charCount;
}


ShaderResourceSetBuilder::ShaderResourceSetBuilder(int _descriptorManagerIndex, int _descriptorSetIndex, ShaderResourceSet* _setPtr)
	:
	set(_setPtr), handle(_descriptorManagerIndex, _descriptorSetIndex)
{

}

ShaderResourceSetHandle ShaderResourceSetBuilder::operator()()
{
	return handle;
}

void ShaderResourceSetBuilder::SetVariableArrayCount(ShaderResourceSetContext* context, int bindingIndex, int varArrayCount)
{
	ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

	if (!(header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY))
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Binding is not a variable array count!\n"));
		return;
	}

	header->arrayCount = (header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY) | (varArrayCount & DESCRIPTOR_COUNT_MASK);
}

void ShaderResourceSetBuilder::BindBufferToShaderResource(ShaderResourceSetContext* context, int* allocationIndex, int firstBuffer, int bufferCount, int bindingIndex)
{
	ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

	if (header->type != ShaderResourceType::UNIFORM_BUFFER && header->type != ShaderResourceType::STORAGE_BUFFER)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Binding is not a uniform or storage buffer!\n"));
		return;
	}

	ShaderResourceBuffer* buffer = &header->resourceArray.buffers;

	if ((firstBuffer + bufferCount) > header->arrayCount)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Buffer count is greater than allocated array count!\n"));
		return;
	}

	for (int i = 0; i < bufferCount; i++)
		buffer->allocationIndex[firstBuffer + i] = allocationIndex[i];

	if ((firstBuffer + bufferCount) > buffer->bufferCount)
		buffer->bufferCount = (firstBuffer + bufferCount);

}

void ShaderResourceSetBuilder::BindImageResourceToShaderResource(ShaderResourceSetContext* context, int* index, int* views, int textureCount, int firstTexture, int bindingIndex)
{
	ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

	if (header->type != ShaderResourceType::IMAGESTORE2D && header->type != ShaderResourceType::IMAGE2D)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Binding is not a image resource array!\n"));
		return;
	}

	if ((firstTexture + textureCount) > header->arrayCount)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Texture count is greater than allocated array count!\n"));
		return;
	}

	ShaderResourceImage* images = &header->resourceArray.images;

	for (int i = 0; i < textureCount; i++)
	{
		images->textureDetails[firstTexture + i].textureHandle = index[i];
		images->textureDetails[firstTexture + i].viewIndex = views[i];
	}

	if ((firstTexture + textureCount) > images->textureCount)
		images->textureCount = (firstTexture + textureCount);
}

void ShaderResourceSetBuilder::BindSamplerResourceToShaderResource(ShaderResourceSetContext* context, int* indices, int samplerCount, int firstSampler, int bindingIndex)
{
	ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

	if (header->type != ShaderResourceType::SAMPLERSTATE)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Binding is not a sampler array!\n"));
		return;
	}

	if ((firstSampler + samplerCount) > header->arrayCount)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Sampler count is greater than allocated array count!\n"));
		return;
	}

	ShaderResourceSampler* samplers = &header->resourceArray.samplers;

	for (int i = 0; i < samplerCount; i++)
		samplers->samplerHandles[firstSampler + i] = indices[i];

	if ((firstSampler + samplerCount) > samplers->samplerCount)
		samplers->samplerCount = (firstSampler + samplerCount);
}

void ShaderResourceSetBuilder::BindSampledImageToShaderResource(ShaderResourceSetContext* context, int* index, int* views, int* samplers, int textureCount, int firstTexture, int bindingIndex)
{
	ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

	if (header->type != ShaderResourceType::SAMPLER2D && header->type != ShaderResourceType::SAMPLERCUBE && header->type != ShaderResourceType::SAMPLER3D)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Binding is not a combined sampler/texture array!\n"));
		return;
	}

	if ((firstTexture + textureCount) > header->arrayCount)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Texture count is greater than allocated array count!\n"));
		return;
	}

	ShaderResourceCombinedImage* images = &header->resourceArray.combinedImages;

	for (int i = 0; i < textureCount; i++)
	{
		images->textureDetails[firstTexture + i].textureHandle = index[i];
		images->textureDetails[firstTexture + i].viewIndex = views[i];
		images->textureDetails[firstTexture + i].samplerHandle = samplers[i];
	}

	if ((firstTexture + textureCount) > images->textureCount)
		images->textureCount = (firstTexture + textureCount);
}

void ShaderResourceSetBuilder::BindBufferView(ShaderResourceSetContext* context, int* allocationIndex, int firstView, int viewCount, int bindingIndex)
{
	ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

	if (header->type != ShaderResourceType::BUFFER_VIEW)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Binding is not a structured buffer array!\n"));
		return;
	}

	if ((firstView + viewCount) > header->arrayCount)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("View count is greater than allocated array count!\n"));
		return;
	}

	ShaderResourceBuffer* bufferView = &header->resourceArray.views;

	for (int i = 0; i < viewCount; i++)
	{
		bufferView->allocationIndex[firstView + i] = allocationIndex[i];
	}

	if ((firstView + viewCount) > bufferView->bufferCount)
		bufferView->bufferCount = (firstView + viewCount);

}

ShaderResourceConstantBuffer* ShaderResourceSetBuilder::GetConstantBuffer(int constantBuffer)
{
	ShaderResourceConstantBuffer* ret = &set->constantBuffers[constantBuffer];

	if (ret->type != ShaderResourceType::CONSTANT_BUFFER)
		return nullptr;

	return ret;
}

int ShaderResourceSetBuilder::GetConstantBufferCount()
{
	return set->templateMetaData->constantsCount;
}

void ShaderResourceSetBuilder::UploadConstant(ShaderResourceSetContext* context, void* data, int bufferLocation)
{
	ShaderResourceConstantBuffer* header = (ShaderResourceConstantBuffer*)GetConstantBuffer(bufferLocation);

	if (!header)
	{
		context->contextFailed = true;
		context->contextLogger->AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Not a push constant index!\n"));
		return;
	}

	header->data = data;
}

void ShaderResourceManager::Create(Allocator* shaderResourceMemoryAllocator, uint32_t maxDescriptorSets)
{
	descriptorSetHandles.Create(shaderResourceMemoryAllocator, maxDescriptorSets);
	memset(descriptorSetHandles.pool, 0xFF, sizeof(EntryHandle) * maxDescriptorSets);
	descriptorSets = (ShaderResourceSet**)shaderResourceMemoryAllocator->Allocate(sizeof(ShaderResourceSet*) * maxDescriptorSets, alignof(ShaderResourceSet*));
}

int ShaderResourceManager::AddShaderToSets(ShaderResourceSet* location)
{
	int indexRet = descriptorSetHandles.Allocate();

	descriptorSets[indexRet] = location;

	return indexRet;
}

int ShaderResourceManager::GetConstantBufferCount(int descriptorSet)
{
	ShaderResourceSet* set = descriptorSets[descriptorSet];

	return set->templateMetaData->constantsCount;
}

ShaderResourceHeader* ShaderResourceManager::GetConstantBuffer(int descriptorSet, int constantBuffer)
{
	ShaderResourceSet* set = descriptorSets[descriptorSet];

	ShaderResourceConstantBuffer* ret = &set->constantBuffers[constantBuffer];

	if (ret->type != ShaderResourceType::CONSTANT_BUFFER)
		return nullptr;

	return ret;

}