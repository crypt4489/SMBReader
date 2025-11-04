#include "ShaderResourceSet.h"
char ShaderGraphReader::readerMemBuffer[16 * MB];
int ShaderGraphReader::readerMemBufferAllocate = 0;
char ShaderGraphReader::strings[4 * KB];
int ShaderGraphReader::stringAllocate = 0;


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


ShaderGraph* ShaderGraphReader::CreateShaderGraph(const std::string& filename, void* graphmemory)
{

	uintptr_t TreeNodes[50]{};

	int setCount = 0;

	std::vector<char> fileData;

	auto ret = FileManager::ReadFileInFull(filename, fileData, 0);

	if (ret)
	{
		throw std::runtime_error("Shader Init file is unable to be opened");
	}

	int tagCount = 0;
	int curr = 0;

	int stride = SkipLine(fileData, curr);
	curr += stride;
	while (curr + stride < fileData.size())
	{

		int memCounter = stringAllocate;

		stride = ProcessTag(fileData, curr);
		curr += stride;

		stride = SkipLine(fileData, curr);

		char c1 = strings[memCounter + 1];

		char* tag = &strings[memCounter + 1];

		bool opening = true;

		if (c1 == '/')
		{
			opening = false;
			tag = &strings[memCounter + 2];
			//std::cout << "Closing Node" << std::endl;
		}
		else {
			//std::cout << "Opening Node" << std::endl;
		}

		unsigned long hashl = hash(tag);

		switch (hashl)
		{
		case hash("ShaderGraph"):
			//std::cout << "ShaderGraph" << std::endl;
			break;
		case hash("GLSLShader"):
			//std::cout << "GLSLShader" << std::endl;
			if (opening) {
				stride = HandleGLSLShader(fileData, curr, &TreeNodes[tagCount]);
			}
			break;
		case hash("ShaderResourceSet"):
			//std::cout << "ShaderResourceSet" << std::endl;
			setCount++;
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

		tagCount++;
	}


	return nullptr;
}

int ShaderGraphReader::ProcessTag(std::vector<char>& fileData, int currentLocation)
{
	int count = 0;
	char* data = fileData.data() + currentLocation;
	size_t size = fileData.size();
	int memCounter = stringAllocate;
	while (currentLocation + count < size)
	{
		int test = count++;

		if (std::isspace(data[test]) && memCounter == stringAllocate)
			continue;

		if (data[test] != '<' && memCounter == stringAllocate)
			throw std::runtime_error("malformed xml tag");

		if (data[test] == '\n' || data[test] == ' ' || data[test] == '>') {
			strings[memCounter++] = 0;
			break;
		}

		strings[memCounter++] = data[test];
	}

	stringAllocate = memCounter;

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

int ShaderGraphReader::ReadValue(std::vector<char>& fileData, int currentLocation, int* len)
{
	int memCounter = readerMemBufferAllocate;
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

		readerMemBuffer[memCounter++] = c;

	}

	readerMemBuffer[memCounter++] = 0;

	*len = (memCounter - readerMemBufferAllocate);

	readerMemBufferAllocate = memCounter;

	return count;
}
#define MAX_ATTRIBUTE_LEN 50
int ShaderGraphReader::ReadAttributeName(std::vector<char>& fileData, int currentLocation)
{
	int memCounter = readerMemBufferAllocate;
	int count = 0;
	char* data = fileData.data() + currentLocation;
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
			continue;

		if (c != '=' && memCounter == readerMemBufferAllocate + MAX_ATTRIBUTE_LEN)
			throw std::runtime_error("malformed xml attribute");

		if (c == '=')
		{
			readerMemBuffer[memCounter++] = 0;
			break;
		}

		readerMemBuffer[memCounter++] = c;

	}

	readerMemBufferAllocate = memCounter;

	return count;
}

int ShaderGraphReader::ReadAttributeValue(std::vector<char>& fileData, int currentLocation)
{
	int memCounter = readerMemBufferAllocate;
	int count = 0;
	char* data = fileData.data() + currentLocation;
	while (true)
	{
		int test = count++;

		int c = data[test];

		if (c == '>')
		{
			count = count - 1;
			break;
		}

		if (std::isspace(c) && memCounter == readerMemBufferAllocate)
			continue;

		if (std::isspace(c) && memCounter != readerMemBufferAllocate)
		{

			break;
		}

		if (!std::isspace(c) && memCounter == readerMemBufferAllocate + MAX_ATTRIBUTE_LEN) {
			throw std::runtime_error("malformed xml attribute");
		}

		readerMemBuffer[memCounter++] = c;

	}

	readerMemBuffer[memCounter++] = 0;

	readerMemBufferAllocate = memCounter;

	return count;
}



int ShaderGraphReader::ReadAttributes(std::vector<char>& fileData, int currentLocation, uintptr_t* offsets, int* stackSize)
{
	int count = 0;
	char* data = fileData.data() + currentLocation;
	size_t size = fileData.size();

	int ret = 0;
	char c = data[ret];

	while (c != '>')
	{
		offsets[count++] = (uintptr_t)&readerMemBuffer[readerMemBufferAllocate];
		ret += ReadAttributeName(fileData, currentLocation + ret);
		offsets[count++] = (uintptr_t)&readerMemBuffer[readerMemBufferAllocate];
		ret += ReadAttributeValue(fileData, currentLocation + ret);
		c = data[ret];
	}

	*stackSize = count;

	return ret + 2;
}

int ShaderGraphReader::HandleGLSLShader(std::vector<char>& fileData, int currentLocation, uintptr_t* offset)
{
	uintptr_t offsets[6];

	int size = 0;

	int ret = ReadAttributes(fileData, currentLocation, offsets, &size);

	ShaderGLSLShaderXMLTag* tag = (ShaderGLSLShaderXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("GLSLShader");

	int stackIter = 0;

	while (size > stackIter)
	{
		char* codeStr = (char*)offsets[stackIter];
		char* valStr = (char*)offsets[stackIter + 1];
		unsigned long code = hash(codeStr);
		unsigned long codeV = hash(valStr);
		switch (code)
		{
		case hash("type"):
		{
			switch (codeV)
			{
			case hash("compute"):
				tag->type = ShaderStageTypeBits::COMPUTESHADERSTAGE;
			}
		}
		}

		stackIter += 2;
	}

	readerMemBufferAllocate += sizeof(ShaderGLSLShaderXMLTag);

	ret += ReadValue(fileData, currentLocation + ret, &tag->shaderNameLen);

	return ret;
}

int ShaderGraphReader::HandleShaderResourceItem(std::vector<char>& fileData, int currentLocation, uintptr_t* offset)
{
	uintptr_t offsets[12];

	int size = 0;

	int ret = ReadAttributes(fileData, currentLocation, offsets, &size);

	ShaderResourceItemXMLTag* tag = (ShaderResourceItemXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("ShaderResourceItem");

	int stackIter = 0;

	while (size > stackIter)
	{
		char* codeStr = (char*)offsets[stackIter];
		char* valStr = (char*)offsets[stackIter + 1];
		unsigned long code = hash(codeStr);
		unsigned long codeV = hash(valStr);
		switch (code)
		{
		case hash("type"):
		{
			switch (codeV)
			{
			case hash("storage"):
				tag->resourceType = ShaderResourceType::STORAGE_BUFFER;
				break;
			case hash("uniform"):
				tag->resourceType = ShaderResourceType::STORAGE_BUFFER;
				break;
			case hash("constantbuffer"):
				tag->resourceType = ShaderResourceType::CONSTANT_BUFFER;
				tag->resourceAction = ShaderResourceAction::SHADERREAD;
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
	uintptr_t offsets[6];

	int size = 0;

	int ret = ReadAttributes(fileData, currentLocation, offsets, &size);

	ShaderComputeLayoutXMLTag* tag = (ShaderComputeLayoutXMLTag*)&readerMemBuffer[readerMemBufferAllocate];

	*offset = (uintptr_t)tag;

	tag->hashCode = hash("ComputeLayout");

	int stackIter = 0;

	while (size > stackIter)
	{
		char* codeStr = (char*)offsets[stackIter];
		char* valStr = (char*)offsets[stackIter + 1];
		unsigned long code = hash(codeStr);

		int comp = ASCIIToInt(valStr);

		switch (code)
		{
		case hash("x"):
		{
			tag->x = comp;

			break;
		}
		case hash("y"):
		{
			tag->y = comp;
			break;
		}
		case hash("z"):
		{
			tag->z = comp;

			break;
		}
		}

		stackIter += 2;
	}

	return ret;
}