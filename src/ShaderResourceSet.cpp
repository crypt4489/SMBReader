#include "ShaderResourceSet.h"
char ShaderGraphReader::readerMemBuffer[16 * MB];
int ShaderGraphReader::readerMemBufferAllocate = 0;



constexpr unsigned long
ShaderGraphReader::hash(char* str)
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
ShaderGraphReader::hash(const std::string& string)
{
	unsigned long hash = 5381;
	int c;

	const char* str = string.c_str();

	while (c = *str++) {
		if (((c - 'A') >= 0 && (c - 'Z') <= 0) || ((c - 'a') >= 0 && (c - 'z') <= 0))
			hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}


ShaderGraph* ShaderGraphReader::CreateShaderGraph(
	const std::string& filename, uintptr_t graphmemory, size_t *outSize, 
	void* shaderDataOut, int *shaderDataSize, int *shaderDetailCount
)
{

	uintptr_t TreeNodes[50]{};
	int SetNodes[15]{};
	int ShaderRefs[5]{};
	ShaderDetails* details[5]{};
	
	uintptr_t shaderDeatsIter = (uintptr_t)shaderDataOut;
	uintptr_t shaderDeatsHead = shaderDeatsIter;

	std::vector<char> fileData;

	auto ret = FileManager::ReadFileInFull(filename, fileData, 0);

	if (ret)
	{
		throw std::runtime_error("Shader Init file is unable to be opened");
	}

	int shaderCount = 0;
	int shaderResourceCount = 0;
	int setCount = 0;

	int lastShader = 0;
	int shaderDetailDataStride = 0;

	int tagCount = 0;
	int curr = 0;

	int stride = SkipLine(fileData, curr);
	curr += stride;
	while (curr + stride < fileData.size())
	{

		unsigned long hashl = 0;
		bool opening = true; 
		stride = ProcessTag(fileData, curr, &hashl, &opening);
		curr += stride;

		stride = SkipLine(fileData, curr);

		if (opening)
		{
			tagCount++;
		}

		

		switch (hashl)
		{
		case hash("ShaderGraph"):
			//std::cout << "ShaderGraph" << std::endl;
			if (!opening)
				curr = (int)fileData.size();
			break;
		case hash("GLSLShader"):
			//std::cout << "GLSLShader" << std::endl;
			if (opening) {
				stride = HandleGLSLShader(fileData, curr, &TreeNodes[tagCount], (void*)shaderDeatsIter, &shaderDetailDataStride);
				
				ShaderRefs[shaderCount] = tagCount;
				details[shaderCount] = (ShaderDetails*)shaderDeatsIter;
				
				shaderDeatsIter += shaderDetailDataStride;
				
				lastShader = shaderCount;
				
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
				stride = HandleShaderResourceItem(fileData, curr, &TreeNodes[tagCount]);
			}
			break;
		case hash("ComputeLayout"):
			//std::cout << "ComputeLayout" << std::endl;
			if (opening) {
				stride = HandleComputeLayout(fileData, curr, &TreeNodes[tagCount]);
			}

			break;
		}

		curr += stride;

		
	}

	uintptr_t head = (graphmemory + *outSize);

	ShaderGraph* graph = (ShaderGraph*)(head);

	memset(graph, 0, sizeof(ShaderGraph) + (setCount * sizeof(ShaderSetLayout)));

	graph->shaderMapCount = shaderCount;
	graph->resourceSetCount = setCount;
	uintptr_t mapData = head + sizeof(ShaderGraph) + (setCount * sizeof(ShaderSetLayout));

	int shaderIndex = 0;

	for (int j = 0; j < shaderCount; j++)
	{
		ShaderMap* map = (ShaderMap*)(mapData);

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
		
		int resourceIter = 0;

		for (int i = 0; i<setCount; i++)
		{

			ShaderSetLayout* setLay = (ShaderSetLayout*)graph->GetSet(i);
			int resIter = SetNodes[i]+1;
			ShaderResourceItemXMLTag* tag = (ShaderResourceItemXMLTag*)TreeNodes[resIter];
			int bindingCount = 0;
			while (tag && tag->hashCode == hash("ShaderResourceItem"))
			{
				
				if (tag->shaderstage == type)
				{
					ShaderResource* resource = (ShaderResource*)map->GetResource(resourceIter++);
					if (tag->resourceType & CONSTANT_BUFFER)
					{
						resource->binding = ~0;
					}
					else 
					{
						resource->binding = bindingCount;
					}
					
					resource->action = tag->resourceAction;
					resource->type = tag->resourceType;
					resource->arrayCount = tag->arrayCount;
					resource->set = i;
					setLay->bindingCount++;
				}
				
				if (!(tag->resourceType & CONSTANT_BUFFER))
				{
					bindingCount++;
				}

				tag = (ShaderResourceItemXMLTag*)TreeNodes[++resIter];
			}
		}


		map->resourceCount = resourceIter;

		mapData += (sizeof(ShaderMap) + (map->resourceCount * sizeof(ShaderResource)));
	}

	*outSize += (mapData - head);

	*shaderDataSize = (int)(shaderDeatsIter - shaderDeatsHead);

	*shaderDetailCount = shaderCount;

	return graph;
}

int ShaderGraphReader::ProcessTag(std::vector<char>& fileData, int currentLocation, unsigned long *hash, bool *opening)
{
	int count = 0;
	char* data = fileData.data() + currentLocation;
	size_t size = fileData.size();
	int memCounter = 0;

	unsigned long hashl = 5381;

	

	while (currentLocation + count < size)
	{
		int test = count++;

		if (std::isspace(data[test]) && memCounter == 0)
			continue;

		if (data[test] != '<' && memCounter == 0)
			throw std::runtime_error("malformed xml tag");

		if (data[test] == '\n' || data[test] == ' ' || data[test] == '>') {
			break;
		}

		if (data[test] == '<')
		{
			memCounter++;
			if (data[test + 1] == '/')
			{
				*opening = false;
				count++;
			}
			continue;
		}

		hashl = ((hashl << 5) + hashl) + data[test];
	}

	*hash = hashl;

	return count;
}

int ShaderGraphReader::SkipLine(std::vector<char>& fileData, int currentLocation)
{
	int count = 0;
	char* data = fileData.data() + currentLocation;
	size_t size = fileData.size();
	while (currentLocation + count < size && data[count++] != '\n');
	data = fileData.data() + currentLocation + count;
	return count;
}

int ShaderGraphReader::ReadValue(std::vector<char>& fileData, int currentLocation, char* str, int* len)
{
	int memCounter = 0;
	int count = 0;
	char* data = fileData.data() + currentLocation;
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
int ShaderGraphReader::ReadAttributeName(std::vector<char>& fileData, int currentLocation, unsigned long* hash)
{
	int memCounter = 0;
	
	int count = 0;
	char* data = fileData.data() + currentLocation;

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

		if (memCounter == MAX_ATTRIBUTE_LEN)
			throw std::runtime_error("malformed xml attribute");

	
		memCounter++;

		hashl = ((hashl << 5) + hashl) + c;

	}
	
	*hash = hashl;

	return count;
}

int ShaderGraphReader::ReadAttributeValueHash(std::vector<char>& fileData, int currentLocation, unsigned long* hash)
{
	int memCounter = 0;
	int count = 0;
	char* data = fileData.data() + currentLocation;

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
			if (memCounter == 0)
				continue;
			else
				break;
		}

		if (c == '\"' || c == '\'')
			continue;

		if (memCounter == MAX_ATTRIBUTE_LEN) {
			throw std::runtime_error("malformed xml attribute");
		}

		memCounter++;

		hashl = ((hashl << 5) + hashl) + c;

	}

	*hash = hashl;

	return count;
}

int ShaderGraphReader::ReadAttributeValueVal(std::vector<char>& fileData, int currentLocation, unsigned long* val)
{
	int memCounter = 0;
	int count = 0;
	char* data = fileData.data() + currentLocation;

	unsigned long out = 0;

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
			if (memCounter == 0)
				continue;
			else
				break;
		}

		if (c == '\"' || c == '\'')
			continue;

		if (memCounter == MAX_ATTRIBUTE_LEN || ((c - '0') < 0 || (c - '9') > 0)) {
			throw std::runtime_error("malformed xml attribute");
		}

		memCounter++;

		out = (out * 10) + (c - '0');

	}

	*val = out;

	return count;
}

#define MAX_ATTRIBUTE_LINE_LEN 200

int ShaderGraphReader::ReadAttributes(std::vector<char>& fileData, int currentLocation, unsigned long* hashes, int* stackSize, int valType)
{
	int count = 0;
	char* data = fileData.data() + currentLocation;
	size_t size = fileData.size();

	int ret = 0;
	char c = data[ret];

	while (c != '>' && ret < MAX_ATTRIBUTE_LINE_LEN && (currentLocation+ret) < size)
	{
		ret += ReadAttributeName(fileData, currentLocation + ret, &hashes[count]);
	
		switch (hashes[count])
		{
		case hash("type"):
		case hash("used"):
		case hash("rw"):
		{
			ret += ReadAttributeValueHash(fileData, currentLocation + ret, &hashes[count + 1]);
			break;
		}
		case hash("count"):
		case hash("x"):
		case hash("y"):
		case hash("z"):
		{
			ret += ReadAttributeValueVal(fileData, currentLocation + ret, &hashes[count + 1]);
			break;
		}
		}
		c = data[ret];
		count += 2;
	}

	*stackSize = count;

	return ret + 2;
}



int ShaderGraphReader::HandleGLSLShader(std::vector<char>& fileData, int currentLocation, uintptr_t* offset, void *shaderData, int *shaderDataSize)
{
	unsigned long hashes[6];

	int size = 0;

	int ret = ReadAttributes(fileData, currentLocation, hashes, &size, 0);

	uintptr_t detailHead = (uintptr_t)shaderData;

	ShaderDetails* details = (ShaderDetails*)detailHead;

	details->shaderDataSize = 0;
	details->shaderNameSize = 0;

	detailHead += sizeof(ShaderDetails);

	ShaderGLSLShaderXMLTag* tag = (ShaderGLSLShaderXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("GLSLShader");

	int stackIter = 0;

	while (size > stackIter)
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

	ret += ReadValue(fileData, currentLocation + ret, details->GetString(), &details->shaderNameSize);

	*shaderDataSize = (sizeof(ShaderDetails) + details->shaderNameSize + details->shaderDataSize);

	return ret;
}

int ShaderGraphReader::HandleShaderResourceItem(std::vector<char>& fileData, int currentLocation, uintptr_t* offset)
{
	unsigned long hashes[12];

	int size = 0;

	int ret = ReadAttributes(fileData, currentLocation, hashes, &size, 0);

	ShaderResourceItemXMLTag* tag = (ShaderResourceItemXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("ShaderResourceItem");

	tag->arrayCount = 1;

	int stackIter = 0;

	while (size > stackIter)
	{
		unsigned long code = hashes[stackIter];
		unsigned long codeV = hashes[stackIter + 1];
		switch (code)
		{
		case hash("type"):
		{
			switch (codeV)
			{
			case hash("sampler"):
				tag->resourceType = ShaderResourceType::SAMPLER;
				break;
			case hash("storageimage"):
				tag->resourceType = ShaderResourceType::IMAGESTORE2D;
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
			case hash("samplerBindless"):
				tag->resourceType = ShaderResourceType::SAMPLERBINDLESS;
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
		case hash("count"):
		{
			tag->arrayCount = codeV;
			break;
		}
		}

		stackIter += 2;
	}

	readerMemBufferAllocate += sizeof(ShaderResourceItemXMLTag);

	return ret;
}


constexpr int ShaderGraphReader::ASCIIToInt(char* str)
{
	int c;
	int out = 0;

	while (c = *str++) {
		if ((c - '0') >= 0 && (c - '9') <= 0)
			out = (out * 10) + (c - '0');
	}

	return out;
}

int ShaderGraphReader::HandleComputeLayout(std::vector<char>& fileData, int currentLocation, uintptr_t* offset)
{
	unsigned long hashesAndVals[6];

	int size = 0;

	int ret = ReadAttributes(fileData, currentLocation, hashesAndVals, &size, 1);

	ShaderComputeLayoutXMLTag* tag = (ShaderComputeLayoutXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("ComputeLayout");

	int stackIter = 0;

	while (size > stackIter)
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