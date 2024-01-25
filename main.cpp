

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>

std::vector<uint8_t> LoadFile(std::string name)
{
	std::ifstream filehandle(name, std::ios::binary | std::ios::ate);

	if (!filehandle.is_open())
		throw std::runtime_error("SMB file is unable to be opened");

	filehandle.seekg(0, std::ios_base::end);

	std::streampos filesize = filehandle.tellg();

	std::vector<uint8_t> filedata(filesize);

	filehandle.seekg(0, std::ios_base::beg);

	filehandle.read(reinterpret_cast<char*>(filedata.data()), filesize);

	filehandle.close();

	return filedata;
}

enum chunktype
{
	GEO = 0,
	GR2 = 6,
	Joints = 20, //?
};

// each string is null terminated;

// 66 bytes in between each filename


// file offset is where the data actually starts (maybe?)

//numResources corresponds to the number of BEGINNINGCHUNKTAGS are present in a file

#define BEGINNINGCHUNKTAG 0xa77e4dfa
#define ENDTAG 0xbeef1234
typedef struct smb_file_t
{
	uint32_t magic;
	uint32_t version;
	std::string name;
	uint32_t fileOffset;
	uint32_t headerStreamEnd;
	uint32_t numContiguousBytes;
	uint32_t numSystemBytes;
	uint32_t contiguousSizeUnpadded;
	uint32_t systemSizeUnpadded;
	uint32_t numResources;
	uint32_t ioMode;
} SMBFile;

int main()
{
	std::vector<uint8_t> fileType = LoadFile("test.smb");
}