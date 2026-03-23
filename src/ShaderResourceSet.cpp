#include "ShaderResourceSet.h"

static char* readerMemBuffer;
static int readerMemBufferAllocate;

BarrierStage ConvertShaderStageToBarrierStage(ShaderStageType type)
{
	BarrierStage flags = 0;
	flags |= (VERTEX_SHADER_BARRIER) * ((type & VERTEXSHADERSTAGE) != 0);
	//flags |= (VK_SHADER_STAGE_FRAGMENT_BIT) * ((type & FRAGMENTSHADERSTAGE) != 0);
	flags |= (COMPUTE_BARRIER) * ((type & COMPUTESHADERSTAGE) != 0);
	return flags;
}

ShaderDetails* ShaderDetails::GetNext()
{
	return (ShaderDetails*)((uintptr_t)this + sizeof(ShaderDetails) + shaderDataSize + shaderNameSize);
}

char* ShaderDetails::GetString()
{
	return (char*)((uintptr_t)this + sizeof(ShaderDetails));
}

void* ShaderDetails::GetShaderData()
{
	return (void*)((uintptr_t)this + sizeof(ShaderDetails) + shaderNameSize);
}



constexpr unsigned long
hash(char* str)
{
	unsigned long hash = 5381;
	int c;

	while (c = *str++) {
		if (((c - 'A') >= 0 && (c - 'Z') <= 0) || ((c - 'a') >= 0 && (c - 'z') <= 0))
			hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

constexpr unsigned long
hash(const std::string& string)
{
	unsigned long hash = 5381;
	int c;

	const char* str = string.c_str();

	while (c = *str++) {
		if (((c - 'A') >= 0 && (c - 'Z') <= 0) || ((c - 'a') >= 0 && (c - 'z') <= 0) || ((c - '0') >= 0 && (c - '9') <= 0))
			hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}


ShaderGraph* CreateShaderGraph(const std::string& filename, RingAllocator* readerMemory, SlabAllocator* graphAllocator, SlabAllocator* shaderAllocator, int* shaderDetailCount)
{
	uintptr_t* TreeNodes = (uintptr_t*)readerMemory->Allocate(sizeof(uintptr_t) * 50);
	
	int* SetNodes = (int*)readerMemory->Allocate(sizeof(int) * 20);
	
	int* ShaderRefs = SetNodes + 15;
	
	ShaderDetails** details = (ShaderDetails**)readerMemory->Allocate(sizeof(ShaderDetails*) * 5);

	FileID fileID = FileManager::OpenFile(filename, READ);

	OSFileHandle* fileHandle = FileManager::GetFile(fileID);

	int dataSize = fileHandle->fileLength;
	
	void* fileData = readerMemory->Allocate(dataSize);

	OSReadFile(fileHandle, dataSize, (char*)fileData);

	OSCloseFile(fileHandle);

	char* dataStart = (char*)fileData;

	int shaderCount = 0;
	int shaderResourceCount = 0;
	int setCount = 0;

	int tagCount = 0;
	int curr = 0;

	int stride = SkipLine(dataStart, dataSize, curr);
	
	curr += stride;

	int readerSizeMultiply = (dataSize / KiB) + 1;

	readerMemBufferAllocate = 0;
	readerMemBuffer = (char*)readerMemory->Allocate(readerSizeMultiply * (KiB >> 1));

	while (curr + stride < dataSize)
	{

		unsigned long hashl = 0;
		bool opening = true; 
		stride = ProcessTag(dataStart, dataSize, curr, &hashl, &opening);
		curr += stride;

		stride = SkipLine(dataStart, dataSize, curr);

		if (opening)
		{
			tagCount++;
		}

		switch (hashl)
		{
		case hash("ShaderGraph"):
			//std::cout << "ShaderGraph" << std::endl;
			if (!opening)
				curr = dataSize;
			break;
		case hash("GLSLShader"):
			//std::cout << "GLSLShader" << std::endl;
			if (opening) {

				ShaderDetails* ldetails = (ShaderDetails*)shaderAllocator->Head();

				stride = HandleGLSLShader(dataStart, dataSize, curr, &TreeNodes[tagCount], shaderAllocator);
				
				ShaderRefs[shaderCount] = tagCount;

				details[shaderCount] = ldetails;
				
				shaderCount++;
			}
			break;
		case hash("ShaderResourceSet"):
			//std::cout << "ShaderResourceSet" << std::endl;
			if (opening) {
				SetNodes[setCount] = tagCount;
				setCount++;
			}
			break;
		case hash("ShaderResourceItem"):
			//std::cout << "ShaderResourceItem" << std::endl;
			if (opening) {
				stride = HandleShaderResourceItem(dataStart, dataSize, curr, &TreeNodes[tagCount]);
				shaderResourceCount++;
			}
			break;
		case hash("ComputeLayout"):
			//std::cout << "ComputeLayout" << std::endl;
			if (opening) {
				stride = HandleComputeLayout(dataStart, dataSize, curr, &TreeNodes[tagCount]);
			}

			break;
		}

		curr += stride;

		
	}

	int graphSizesize = sizeof(ShaderGraph) + (setCount * sizeof(ShaderResourceSetTemplate)) + (shaderCount * sizeof(ShaderMap)) + (shaderResourceCount * sizeof(ShaderResource));

	ShaderGraph* graph = (ShaderGraph*)graphAllocator->Allocate(graphSizesize);

	memset(graph, 0, sizeof(ShaderGraph) + (setCount * sizeof(ShaderResourceSetTemplate)));

	graph->shaderMapCount = shaderCount;
	graph->resourceSetCount = setCount;
	graph->resourceCount = shaderResourceCount;

	int shaderIndex = 0;

	for (int j = 0; j < shaderCount; j++)
	{
		ShaderMap* map = (ShaderMap*)graph->GetMap(j);

		ShaderGLSLShaderXMLTag* tag = (ShaderGLSLShaderXMLTag*)TreeNodes[ShaderRefs[j]];
		
		ShaderStageType type = tag->type;

		if (type == ShaderStageTypeBits::COMPUTESHADERSTAGE)
		{
			ShaderComputeLayoutXMLTag* ctag = (ShaderComputeLayoutXMLTag*)TreeNodes[ShaderRefs[j] + 1];
			ShaderDetails* deats = details[j];
			ShaderComputeLayout* layout = (ShaderComputeLayout*)deats->GetShaderData();
			layout->x = ctag->comps.x;
			layout->y = ctag->comps.y;
			layout->z = ctag->comps.z;
		}

		map->type = type;
		
		
	}

	int resourceIter = 0;

	for (int i = 0; i < setCount; i++)
	{

		ShaderResourceSetTemplate* setLay = (ShaderResourceSetTemplate*)graph->GetSet(i);
		int resIter = SetNodes[i] + 1;
		ShaderResourceItemXMLTag* tag = (ShaderResourceItemXMLTag*)TreeNodes[resIter];
		int bindingCount = 0;

		setLay->resourceStart = resourceIter;
		setLay->constantsCount = 0;
		setLay->samplerCount = 0;
		setLay->constantStageCount = 0;
		setLay->viewCount = 0;
	
		while (tag && tag->hashCode == hash("ShaderResourceItem"))
		{

			ShaderResource* resource = (ShaderResource*)graph->GetResource(resourceIter++);
			if (tag->resourceType == ShaderResourceType::CONSTANT_BUFFER)
			{
				resource->binding = ~0;
				setLay->constantsCount++;
				setLay->constantStageCount = std::max(setLay->constantStageCount, tag->pushRangeStage + 1);
			}
			else
			{
				resource->binding = bindingCount;

				int checkForUnbounded = (tag->arrayCount < 1 ? 1 : tag->arrayCount);

				if (tag->resourceType == ShaderResourceType::SAMPLERSTATE)
				{
					setLay->samplerCount += checkForUnbounded;
				}
				else
				{
					setLay->viewCount += checkForUnbounded;
				}
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
			setLay->bindingCount++;


			if (!(tag->resourceType == ShaderResourceType::CONSTANT_BUFFER))
			{
				bindingCount++;
			}

			tag = (ShaderResourceItemXMLTag*)TreeNodes[++resIter];
		}
	}

	*shaderDetailCount = shaderCount;

	return graph;
}

int ProcessTag(char* fileData, int size, int currentLocation, unsigned long *hash, bool *opening)
{
	int count = 0;
	char* data = fileData + currentLocation;

	unsigned long hashl = 5381;

	while (currentLocation + count < size)
	{
		if (std::isspace(data[count]) && hashl == 5381)
		{
			count++;
			continue;
		}

		if (data[count] != '<' && hashl == 5381)
			throw std::runtime_error("malformed xml tag");

		if (data[count] == '\n' || data[count] == ' ' || data[count] == '>') 
		{
			count++;
			break;
		}

		if (data[count] == '<')
		{
			count++;
			if (data[count] == '/')
			{
				*opening = false;
				count++;
			}
		}

		hashl = ((hashl << 5) + hashl) + data[count];

		count++;
	}

	*hash = hashl;

	return count;
}

int SkipLine(char* fileData, int size, int currentLocation)
{
	int count = 0;
	char* data = fileData + currentLocation;

	while (currentLocation + count < size && data[count++] != '\n');
	data = fileData + currentLocation + count;
	return count;
}

int ReadValue(char* fileData, int size, int currentLocation, char* str, int* len)
{
	int memCounter = 0;
	int count = 0;
	char* data = fileData + currentLocation;
	while (true)
	{
		int test = count++;

		int c = data[test];

		if (c == '\n')
		{
			count = count - 1;
			break;
		}

		if (std::isspace(c))
			continue;

		str[memCounter++] = c;

	}

	str[memCounter++] = 0;

	*len = (memCounter);

	return count;
}
#define MAX_ATTRIBUTE_LEN 50
int ReadAttributeName(char* fileData, int size, int currentLocation, unsigned long* hash)
{
	int count = 0;
	char* data = fileData + currentLocation;

	unsigned long hashl = 5381;


	while (true)
	{
		int test = count++;

		int c = data[test];

		if (c == '>')
		{
			count = count - 1;
			break;
		}

		if (c == '=')
			break;

		if (std::isspace(c))
			continue;

		if (count  == MAX_ATTRIBUTE_LEN + currentLocation)
			throw std::runtime_error("malformed xml attribute");

		hashl = ((hashl << 5) + hashl) + c;

	}
	
	*hash = hashl;

	return count;
}

int ReadAttributeValueHash(char* fileData, int size, int currentLocation, unsigned long* hash)
{
	int count = 0;
	char* data = fileData + currentLocation;

	unsigned long hashl = 5381;

	while (true)
	{
		int test = count++;

		int c = data[test];

		if (c == '>')
		{
			count = count - 1;
			break;
		}

		if (std::isspace(c))
		{
			if (hashl == 5381)
				continue;
			else
				break;
		}

		if (c == '\"' || c == '\'')
			continue;

		if (count == MAX_ATTRIBUTE_LEN+currentLocation) {
			throw std::runtime_error("malformed xml attribute");
		}

		hashl = ((hashl << 5) + hashl) + c;

	}

	*hash = hashl;

	return count;
}

int ReadAttributeValueVal(char* fileData, int size, int currentLocation, unsigned long* val)
{
	int count = 0;
	char* data = fileData + currentLocation;

	unsigned long out = 0;
	bool readingVal = false;

	while (true)
	{
		int test = count++;

		int c = data[test];

		if (c == '>')
		{
			count = count - 1;
			break;
		}

		if (std::isspace(c))
		{
			if (readingVal)
				break;
			continue;
		}

		if (c == '\"' || c == '\'')
		{
			if (readingVal)
				break;
			continue;
		}

		if (count == (MAX_ATTRIBUTE_LEN+currentLocation) || ((c - '0') < 0 || (c - '9') > 0)) {
			throw std::runtime_error("malformed xml attribute");
		}

		out = (out * 10) + (c - '0');
		readingVal = true;

	}

	*val = out;

	return count;
}

#define MAX_ATTRIBUTE_LINE_LEN 200

int ReadAttributes(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize)
{
	int count = 0;
	char* data = fileData + currentLocation;

	int ret = 0;
	char c = data[ret];

	while (c != '>' && ret < MAX_ATTRIBUTE_LINE_LEN && (currentLocation+ret) < size)
	{
		ret += ReadAttributeName(fileData, size, currentLocation + ret, &hashes[count]);
	
		switch (hashes[count])
		{
		case hash("unbounded"):
		case hash("type"):
		case hash("used"):
		case hash("rw"):
		{
			ret += ReadAttributeValueHash(fileData, size,  currentLocation + ret, &hashes[count + 1]);
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
			ret += ReadAttributeValueVal(fileData, size, currentLocation + ret, &hashes[count + 1]);
			break;
		}
		}
		c = data[ret];
		count += 2;
	}

	*stackSize = count;

	while (c != '\n' && ret < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + ret) < size)
	{
		c = data[ret++];
	}

	return ret;
}

#define MAX_SHADER_NAME 50

int HandleGLSLShader(char* fileData, int size, int currentLocation, uintptr_t* offset, SlabAllocator* shaderAllocator)
{
	unsigned long hashes[6];
	char nameScratch[MAX_SHADER_NAME];

	int glslSize = 0;

	int ret = ReadAttributes(fileData, size, currentLocation, hashes, &glslSize);

	ShaderDetails* details = (ShaderDetails*)shaderAllocator->Allocate(sizeof(ShaderDetails));

	details->shaderDataSize = 0;
	details->shaderNameSize = 0;

	ShaderGLSLShaderXMLTag* tag = (ShaderGLSLShaderXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("GLSLShader");

	int stackIter = 0;

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
				details->shaderDataSize = 12;
				tag->type = ShaderStageTypeBits::COMPUTESHADERSTAGE;
				break;
			case hash("vert"):
				tag->type = ShaderStageTypeBits::VERTEXSHADERSTAGE;
				break;
			case hash("fragment"):
				tag->type = ShaderStageTypeBits::FRAGMENTSHADERSTAGE;
				break;
			}
			break;
		}

		default:
			throw std::runtime_error("Failed GLSL Type of Shader");
			break;
		}

		stackIter += 2;
	}

	readerMemBufferAllocate += sizeof(ShaderGLSLShaderXMLTag);

	ret += ReadValue(fileData, size, currentLocation + ret, nameScratch, &details->shaderNameSize);

	char* nameCopy = (char*)shaderAllocator->Allocate(details->shaderNameSize + details->shaderDataSize);

	memcpy(nameCopy, nameScratch, details->shaderNameSize);

	return ret;
}

int HandleShaderResourceItem(char* fileData, int size, int currentLocation, uintptr_t* offset)
{
	unsigned long hashes[14];

	int dataSize = 0;

	int ret = ReadAttributes(fileData, size, currentLocation, hashes, &dataSize);

	ShaderResourceItemXMLTag* tag = (ShaderResourceItemXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("ShaderResourceItem");

	tag->arrayCount = 1;

	int stackIter = 0;

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
				throw std::runtime_error("Failed Resource Type");
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
				throw std::runtime_error("Failed Used Type");
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
				throw std::runtime_error("Failed Action Type");
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
			tag->arrayCount = (tag->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY) | (codeV & DESCRIPTOR_COUNT_MASK);
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

		stackIter += 2;
	}

	readerMemBufferAllocate += sizeof(ShaderResourceItemXMLTag);

	return ret;
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

int HandleComputeLayout(char* fileData, int size, int currentLocation, uintptr_t* offset)
{
	unsigned long hashesAndVals[6];

	int dataSize = 0;

	int ret = ReadAttributes(fileData, size, currentLocation, hashesAndVals, &dataSize);

	ShaderComputeLayoutXMLTag* tag = (ShaderComputeLayoutXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

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

	readerMemBufferAllocate += sizeof(ShaderComputeLayoutXMLTag);

	return ret;
}

void CreatePipelineDescription(const std::string& filename, GenericPipelineStateInfo* stateInfo)
{
	memset(stateInfo, 0, sizeof(GenericPipelineStateInfo));

	std::vector<char> fileData;

	auto ret = FileManager::ReadFileInFull(filename, fileData);

	if (ret)
	{
		throw std::runtime_error("Shader Init file is unable to be opened");
	}

	char* dataStart = fileData.data();
	int dataSize = fileData.size();

	int tagCount = 0;
	int curr = 0;

	int stride = SkipLine(dataStart, dataSize, curr);
	curr += stride;

	int currentVertexInput = 0;
	int currentVertexInputDescription = 0;
	while (curr + stride < dataSize)
	{

		unsigned long hashl = 0;
		bool opening = true;
		stride = ProcessTag(dataStart, dataSize, curr, &hashl, &opening);
		curr += stride;

		stride = SkipLine(dataStart, dataSize, curr);

		if (opening)
		{
			tagCount++;
		}



		switch (hashl)
		{
		case hash("PipelineDescription"):
			if (opening)
			{
				stride = HandlePipelineDescription(dataStart, dataSize, curr, stateInfo);
			}
			break;
		case hash("DepthTest"):
			if (opening)
			{
				stride = HandleDepthTest(dataStart, dataSize, curr, stateInfo);
			}
			break;
		case hash("PrimitiveType"):
			if (opening)
			{
				stride = HandlePrimitiveType(dataStart, dataSize, curr, stateInfo);
			}
			break;
	
		case hash("CullMode"):
			if (opening)
			{
				stride = HandleCullMode(dataStart, dataSize, curr, stateInfo);
			}
			break;
		case hash("VertexInput"):
		{
			if (opening)
			{
				stateInfo->vertexBufferDescCount++;
				stride = HandleVertexInput(dataStart, dataSize, curr, stateInfo, currentVertexInput);
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
				stride = HandleVertexComponentInput(dataStart, dataSize, curr, stateInfo, currentVertexInput, currentVertexInputDescription);
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
				stride = HandleStencilTest(dataStart, dataSize, curr, &stateInfo->frontFace);
			}
			break;
		}
		case hash("BackStencilTest"):
		{
			if (opening)
			{
				stateInfo->StencilEnable = true;
				stride = HandleStencilTest(dataStart, dataSize, curr, &stateInfo->backFace);
			}
			break;
		}
		}

		curr += stride;


	}
}

int ReadAttributesPipeline(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize)
{
	int count = 0;
	char* data = fileData + currentLocation;

	int ret = 0;
	char c = data[ret];

	while (c != '>' && ret < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + ret) < size)
	{
		ret += ReadAttributeName(fileData, size, currentLocation + ret, &hashes[count]);

		switch (hashes[count])
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
		{
			ret += ReadAttributeValueHash(fileData, size, currentLocation + ret, &hashes[count + 1]);
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
			ret += ReadAttributeValueVal(fileData, size, currentLocation + ret, &hashes[count + 1]);
			break;
		}
		}
		c = data[ret];
		count += 2;
	}

	*stackSize = count;

	while (c != '\n' && ret < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + ret) < size)
	{
		c = data[ret++];
	}

	return ret;
}

static int HandlePipelineDescription(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize);

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

		default:
			throw std::runtime_error("Failed Pipeline Description");
			break;
		}

		stackIter += 2;
	}

	return ret;
}

int HandleCullMode(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

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
				throw std::runtime_error("Invalid Cull Mode");
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
				throw std::runtime_error("Invalid Winding");
			}
			break;
		}

		default:
			throw std::runtime_error("Failed Cull");
		}

		stackIter += 2;
	}

	return ret;
}

int HandleDepthTest(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

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
				throw std::runtime_error("Invalid Depth Compare Op");
			}
			break;
		}

		default:
			throw std::runtime_error("Failed Depth Mode");
		}

		stackIter += 2;
	}

	return ret;
}

StencilOp ParseStencilOp(uint32_t codeV)
{
	switch (codeV)
	{
	case hash("replace"): return StencilOp::REPLACE;
	case hash("keep"):    return StencilOp::KEEP;
	case hash("zero"):    return StencilOp::ZERO;
	default:              return StencilOp::KEEP;
	}
}

static int HandleStencilTest(char* fileData, int size, int currentLocation, FaceStencilData* face)
{
	unsigned long hashes[14];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

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
				throw std::runtime_error("Invalid Stencil Compare Op");
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

		default:
			throw std::runtime_error("Failed Depth Mode");
		}

		stackIter += 2;
	}

	return ret;
}

int HandlePrimitiveType(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

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
				throw std::runtime_error("Invalid Primitive Type");
			}
			break;
		}
		case hash("size"):
		{
			stateInfo->lineWidth = (float)codeV;
			break;
		}

		default:
			throw std::runtime_error("Failed Prim Mode");
		}

		stackIter += 2;
	}

	return ret;
}

int HandleVertexComponentInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation, int perVertexSlotLocation)
{
	unsigned long hashes[8];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

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
				throw std::runtime_error("Invalid vertex usage");
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
				throw std::runtime_error("Invalid vertex format");
			}
			break;
		}

		case hash("offset"):
		{
			stateInfo->vertexBufferDesc[vertexBufferInputLocation].descriptions[perVertexSlotLocation].byteoffset = codeV;
			break;
		}

		default:
			throw std::runtime_error("Failed Vertex Component");
		}

		stackIter += 2;
	}

	return ret;
}

int HandleVertexInput(char* fileData, int size, int currentLocation, GenericPipelineStateInfo* stateInfo, int vertexBufferInputLocation)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesPipeline(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

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
			}
			break;
		}
		case hash("size"):
		{
			stateInfo->vertexBufferDesc[vertexBufferInputLocation].perInputSize = codeV;
			break;
		}
		
		default:
			throw std::runtime_error("Failed Vertex Input");
			break;
		}

		stackIter += 2;
	}

	return ret;
}
#include <vector>

void CreateAttachmentGraph(const std::string& filename, AttachmentGraph* graph)
{

	FileID fileID = FileManager::OpenFile(filename, READ);

	OSFileHandle* fileHandle = FileManager::GetFile(fileID);

	int dataSize = fileHandle->fileLength;

	std::vector<char> fileDataC(dataSize);

	void* fileData = (void*)fileDataC.data();

	OSReadFile(fileHandle, dataSize, (char*)fileData);

	OSCloseFile(fileHandle);

	char* dataStart = (char*)fileData;

	int attachmentCounter = 0;
	int resourceCounter = 0;
	int holderCounter = 0;

	int depthStencilCount = 0;
	int colorCount = 0;
	int resolveCount = 0;

	int tagCount = 0;
	int curr = 0;

	int stride = 0;

	AttachmentHolder* currentHolder = nullptr;

	while (curr < dataSize)
	{

		unsigned long hashl = 0;
		bool opening = true;
		stride = ProcessTag(dataStart, dataSize, curr, &hashl, &opening);
		curr += stride;

		stride = SkipLine(dataStart, dataSize, curr);

		if (opening)
		{
			tagCount++;
		}


		if (opening)
		{

			switch (hashl)
			{
			case hash("Attachments"):
				currentHolder = &graph->holders[holderCounter++];
				//stride = HandleAttachmentDesc(dataStart, dataSize, curr, currentHolder);

				break;
			case hash("ColorAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::COLORATTACH, &currentHolder->descs[attachmentCounter]);
				attachmentCounter++;
				colorCount++;
				break;
			case hash("DepthAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::DEPTHATTACH, &currentHolder->descs[attachmentCounter]);
				attachmentCounter++;
				depthStencilCount++;
				break;
			case hash("ResolveAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::RESOLVEATTACH, &currentHolder->descs[attachmentCounter]);
				attachmentCounter++;
				resolveCount++;
				break;
			case hash("StencilAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::STENCILATTACH, &currentHolder->descs[attachmentCounter]);
				attachmentCounter++;
				depthStencilCount++;
				break;
			case hash("DepthStencilAttachment"):
				stride = HandleAttachment(dataStart, dataSize, curr, AttachmentDescriptionType::DEPTHSTENCILATTACH, &currentHolder->descs[attachmentCounter]);
				attachmentCounter++;
				depthStencilCount++;
				break;
			case hash("AttachmentResource"):
				stride = HandleAttachmentResource(dataStart, dataSize, curr, &graph->resources[resourceCounter]);
				resourceCounter++;
				break;
			}
			curr += stride;
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
	}

	graph->resourceCount = resourceCounter;

	graph->passesCount = holderCounter;
}

int HandleAttachment(char* fileData, int size, int currentLocation, AttachmentDescriptionType descType, AttachmentDescription* description)
{
	unsigned long hashes[16];

	int attrSize = 0;

	int ret = ReadAttributesAttachments(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

	description->attachType = descType;
	description->msaa = 0;

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
			case hash("undefined"):
				description->srcLayout = ImageLayout::UNDEFINED;
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
			case hash("undefined"):
				description->dstLayout = ImageLayout::UNDEFINED;
				break;
			}
			break;
		}
		
		case hash("msaa"):
		{
			description->msaa = 1;
			break;
		}

		case hash("resource"):
		{
			description->resourceIndex = codeV;
			break;
		}

		default:
			throw std::runtime_error("Failed attahcment tag");
			break;
		}

		stackIter += 2;
	}

	return ret;
}


int HandleAttachmentDesc(char* fileData, int size, int currentLocation, AttachmentHolder* holder)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesAttachments(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

	while (attrSize > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];


		

		stackIter += 2;
	}

	return ret;
}

static int HandleAttachmentResource(char* fileData, int size, int currentLocation, AttachmentResource* resource)
{
	unsigned long hashes[6];

	int attrSize = 0;

	int ret = ReadAttributesAttachments(fileData, size, currentLocation, hashes, &attrSize);

	int stackIter = 0;

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
			}
			break;
		}
		default:
			throw std::runtime_error("Failed Attachments Resource");
			break;
		}

		stackIter += 2;
	}

	return ret;
}

static int ReadAttributesAttachments(char* fileData, int size, int currentLocation, unsigned long* hashes, int* stackSize)
{
	int count = 0;
	char* data = fileData + currentLocation;

	int ret = 0;
	char c = data[ret];

	while (c != '>' && ret < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + ret) < size)
	{
		ret += ReadAttributeName(fileData, size, currentLocation + ret, &hashes[count]);

		switch (hashes[count])
		{
		case hash("imageType"):
		case hash("loadOp"):
		case hash("storeOp"):
		case hash("srcLayout"):
		case hash("dstLayout"):
		case hash("format"):
		case hash("msaa"):
		{
			ret += ReadAttributeValueHash(fileData, size, currentLocation + ret, &hashes[count + 1]);
			break;
		}
		case hash("resource"):
		{
			ret += ReadAttributeValueVal(fileData, size, currentLocation + ret, &hashes[count + 1]);
			break;
		}
		}
		c = data[ret];
		count += 2;
	}

	*stackSize = count;

	while (c != '\n' && ret < MAX_ATTRIBUTE_LINE_LEN && (currentLocation + ret) < size)
	{
		c = data[ret++];
	}

	return ret;
}