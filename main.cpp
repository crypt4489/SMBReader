

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <regex>


#include "s3tc.h"

#pragma pack(2)
typedef struct BitmapFileHeader
{
	uint16_t  bfType;
	uint32_t  bfSize;
	uint16_t  bfReserved1;
	uint16_t  bfReserved2;
	uint32_t  bfOffBits;
} BitmapFileHeader;

typedef struct BitmapInfoHeader
{
	uint32_t biSize;
	int	biWidth;
	int	biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int	biXPelsPerMeter;
	int	biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
} BitmapInfoHeader;
#pragma pack(pop)

void WriteOutSMBBMP(std::string name, char *image, int width, int height)
{
	std::ofstream filehandle(name, std::ios::binary);

	if (!filehandle.is_open())
		throw std::runtime_error("BMP file is unable to be opened");

	BitmapFileHeader fileheader{ 0x4d42, 14+40+(width*height*4), 0, 0, 0x36};
	BitmapInfoHeader infoheader{ 40, width, height, 1, 32, 0, 0, 0, 0, 0, 0 };

	filehandle.write(reinterpret_cast<char*>(&fileheader.bfType), 14);
	filehandle.write(reinterpret_cast<char*>(&infoheader.biSize), 40);

	filehandle.write(image, (width * height * 4));
	
	filehandle.close();


}

void WriteOutChunk(std::string name, char* data, uint32_t size)
{
	std::ofstream filehandle(name, std::ios::binary);

	if (!filehandle.is_open())
		throw std::runtime_error("Chunk file is unable to be opened");
	
	filehandle.write(data, size);

	filehandle.close();
}

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

template <typename C> inline void CopyBytes(std::vector<uint8_t>::iterator& file, int size, C copy)
{
	if constexpr (std::is_pointer_v<C>)
	{
		std::copy(file, file + size, reinterpret_cast<char*>(copy));
	}
	else
	{
		std::copy(file, file + size, copy);
	}

	file += size;
}

enum chunktype
{
	GEO = 0,
	TEXTURE = 1,
	GR2 = 6,
	Joints = 20,
};

struct Texture
{
	uint32_t type;
	uint32_t width;
	uint32_t height;
	uint32_t miplevels;
};

struct GeoFile
{
};


// each string is null terminated except the first file

// 66 bytes in between each filename

// after joint names it is number * 116 bytes until next entry

// file offset is where the data actually starts (maybe?)

//numResources corresponds to the number of BEGINNINGCHUNKTAGS are present in a file

//each datachunk has an identifier that brackets that beginning and end of the chunk 
//before the size of the filestring doesn't seem to be size or offset of chunk

//number of total data from point in chunk is 16 bytes after BEGINNINGCHUNKTAG

// for geo, indices are 16-bit, could be tristrips, might appear in gf2 files as well.

// will need a matrix reader. matrices for joints come after names, could be a gr2 file. 

// 72 10 FD 2D is the tag for a  specific material in a material definition.

// material definitions follow a geo definition (at least for what has been sampled).

// 65 42 0B 00 tag for material definition.

// 62 56 72 49 is sequence for the indexed renderable class definition 62 56 62 50 is for pb indexed renderable class definition

// in geo file, uses XX XX FF 00 for demarcation between vertices structs?

// fc 17 04 00 is beginning and end of indices (of some kind)

// texture resources are 73 bytes in length (not including string size) from tag - 16 bytes in the chunk tag

// gr2 is 43 bytes excluding the string size minus the 16 bytes up to the string

// for joints, there are joints name and then the number of matrices for the joints. These matrices are packed within 112 bytes of data. followed by the number of matrices is 4byte indices

// for every pb indexed renderable there is a fc 17 04 00 pair that corresponds to the number of indices in the indexed renderable.
// 
// after geo file name, there 1, 1, 1, 0 (4bytes apiece) , then there are 28 seemingly random bytes aligned on the fourteenth byte in the sunsequent row. 


// every indexedrenderable has stored its sphere with center and radius (floats)

//it seems like they just store the data for the structs in total in there.

// 47 42 E6 41 FF FF FF FF means something...


// after joint names for indexed renderable, there a floats packed at 28 bytes a time with non consectutive id number for the first 4 bytes.

// size of colormap is 43776, additive spark map is 1408, and normal map is 87296 in compressed size bytes except normal maps

// the first two maps are DXT1 textures, normal map is stored as D3DFMT_X8l8V8u8

//colormap is followed by 96 bytes of zero. colormaps are mipmapped stored consectuvely

// normals are mipmapped from 128X128 to 8X8

// I think all textures for a chaarcter are contained within a smb file. it is just weird that they'll mention it multiple times for a geo definition.


// end of geo tag has the number after the joint names * 4 * 7 many bytes of floating point data in chunks of 7 floats. then followed by 24 bytes.

// then fololowed by a 4 byte number then that number of bytes until end of chunk

//VertexFormat is what determines the encoding of the following vertex for geo file (which is fucking compressed and pisses me off) I think?

// numSystembyte + numContinguous + file offset = total size of file

// so it is possible that all things with continguous offset (not sr2 files) are stored after the fileheader in order and that the sr2s are after the fileOffset + numContiguousByte 

// confirmed at least that all things that impact a continguous offset are placed at the contiguous offset where they are!!!

//21 bytes after texture string until the definition (at for what I need!!!)

//both files oddly use the same legacy directx values for the texture types. will specify in enumeration. WEIRD!!!!
#define BEGINNINGCHUNKTAG 0xa77e4dfa
//#define DATACHUNKTAG 0xcbe402b6
#define ENDTAG 0xbeef1234
#define ENDGEOTAG 0xdc7c9d74

static std::filesystem::path currDir = std::filesystem::current_path();

typedef struct smb_file_t
{
	uint32_t magic;
	uint32_t version;
	std::string name;
	uint32_t fileOffset; // number of bytes from beginning of file
	uint32_t headerStreamEnd; //number of bytes from beginning of file
	uint32_t numContiguousBytes;
	uint32_t numSystemBytes;
	uint32_t contiguousSizeUnpadded;
	uint32_t systemSizeUnpadded;
	uint32_t numResources;
	uint32_t ioMode;
} SMBFile;

typedef struct chunk_data_t
{
	uint32_t chunkType;
	uint32_t chunkId;
	uint32_t contigOffset;
	uint32_t fileOffset; //monotonically increasing for each chunk
	uint32_t numOfBytesAfterTag;
	uint32_t pad3;
	uint32_t tag;
	uint32_t pad4;
	uint32_t stringSize;
	std::string fileName;
	uint32_t definitionBegin;
} ChunkTag;

inline void BGRATexture(char* image, int height, int width)
{
	for (int i = 0; i < width * height * 4; i += 4)
	{
		int temp = image[i];
		int temp2 = image[i + 1];
		int temp3 = image[i + 2];
		int temp4 = image[i + 3];
		image[i + 3] = temp;
		image[i + 2] = temp4;
		image[i + 1] = temp3;
		image[i] = temp2;
	}
}

inline void BGRATexture2(char* image, int height, int width)
{
	for (int i = 0; i < width * height * 4; i += 4)
	{
		int temp = image[i];
		//int temp2 = image[i + 1];
		int temp3 = image[i + 2];
		image[i] = temp3;
		//image[i + 2] = temp2;
		image[i + 2] = temp;
	}
}

std::filesystem::path SetupDirectory(std::string nameOfDir)
{
	std::filesystem::path pathToDir = currDir / nameOfDir;
	if (!std::filesystem::exists(pathToDir))
	{
		std::filesystem::create_directory(pathToDir);
	}
	return pathToDir;
}

void ExportTexture(ChunkTag chunk, std::vector<uint8_t>::iterator fileBegin, uint32_t offset)
{
	std::regex filenamePattern{ "\([A-Za-z_]+)\\." };

	std::smatch match;

	std::string name;
	if (std::regex_search(chunk.fileName, match, filenamePattern))
	{
		name = match[1];
		std::cout << match[1] << std::endl;
	}
	else
	{
		std::cerr << "Couldn't find the filename in " << chunk.fileName << std::endl;
		return;
	}
	
	auto pathToTextures = SetupDirectory(name);
	auto definitionLoc = fileBegin + chunk.definitionBegin + 21;
	Texture tex{};
	CopyBytes(definitionLoc, sizeof(Texture), &tex);
	std::cout
		<< tex.height << "\n"
		<< tex.width << "\n"
		<< tex.type << "\n"
		<< tex.miplevels << "\n"
		<< "-------------------\n";
	int writeWidth = tex.width, writeHeight = tex.height;
	auto dataLoc = fileBegin + offset + chunk.contigOffset;
	int offsetwithin = 0;
	std::vector<char> image(writeWidth * writeHeight * 4);
	for (int i = 0; i < tex.miplevels; i++)
	{
		
		std::string writeFileName = name + std::to_string(i+1) + ".bmp";
		auto writePath = pathToTextures / writeFileName;
		char* dataPtr = reinterpret_cast<char*>(&(*dataLoc));
		switch (tex.type)
		{
		case 7:
			std::cerr << "X8L8U8V8 format is not exportable\n";
			return;
		case 12:
			offsetwithin = BlockDecompressImageDXT1(writeWidth, writeHeight, (unsigned char*)dataPtr, (unsigned long*)image.data());
			BGRATexture(image.data(), writeHeight, writeWidth);
			WriteOutSMBBMP(writePath.string(), image.data(), writeWidth, writeHeight);
			dataLoc += offsetwithin;
			break;
		case 14:
			offsetwithin = BlockDecompressImageDXT3(writeWidth, writeHeight, (unsigned char*)dataPtr, (unsigned char*)image.data());
			BGRATexture(image.data(), writeHeight, writeWidth);
			WriteOutSMBBMP(writePath.string(), image.data(), writeWidth, writeHeight);
			dataLoc += offsetwithin;
			break;
		case 18:
			CopyBytes(dataLoc, writeHeight * writeWidth * 4, image.data());
			BGRATexture2(image.data(), writeHeight, writeWidth);
			WriteOutSMBBMP(writePath.string(), image.data(), writeWidth, writeHeight);
			break;
		default:
			std::cerr << "Unsupported/Unknown texture type " << tex.type << "\n";
			return;
		}
		image.clear();
		image.resize(writeWidth * writeHeight * 4);
		writeHeight >>= 1;
		writeWidth >>= 1;
		
	}
	
}

void ExportChunks(std::vector<ChunkTag> chunks, std::vector<uint8_t>::iterator fileBegin, int fileOffset)
{
	for (const auto& chunk : chunks)
	{
		switch (chunk.chunkType)
		{
		case GEO:
			break;
		case TEXTURE:
			ExportTexture(chunk, fileBegin, fileOffset);
			break;
		case GR2:
			break;
		case Joints:
			break;
		default:
			std::cerr << "Unprocessed chunkType\n";
			break;
		}
	}
}

void ReadHeader(std::vector<uint8_t>::iterator &file, SMBFile& header)
{
	CopyBytes(file, 4, &header.magic);
	CopyBytes(file, 4, &header.version);
	int stringSize;
	CopyBytes(file, 4, &stringSize);
	header.name.resize(stringSize);
	CopyBytes(file, stringSize, header.name.begin());
	CopyBytes(file, 32, &header.fileOffset);
}

void ReadChunk(std::vector<uint8_t>::iterator& file, ChunkTag& tag, std::vector<uint8_t>::iterator fileBegin)
{
	int magic;
	CopyBytes(file, 4, &magic);
	CopyBytes(file, 8*4, &tag.chunkType);
	file += 4;
	CopyBytes(file, 4, &tag.stringSize);
	tag.fileName.resize(tag.stringSize);
	CopyBytes(file, tag.stringSize, tag.fileName.begin());
	tag.definitionBegin = std::distance(fileBegin, file);
	file += (tag.numOfBytesAfterTag - (tag.stringSize + 16));
}

int main()
{
	std::string MainSMB = "test.smb";
	
	currDir = SetupDirectory(MainSMB.substr(0, MainSMB.find_last_of('.')));
	
	SMBFile fileHeader{};
	std::vector<uint8_t> filedata = LoadFile(MainSMB);
	auto iter = filedata.begin();
	ReadHeader(iter, fileHeader);
	int end;
	CopyBytes(iter, 4, &end);
	
	if (end != ENDTAG)
	{
		std::cerr << "Cannot read file header\n";
		return -1;
	}

	std::cout << fileHeader.numContiguousBytes << std::endl;
	std::cout << fileHeader.numSystemBytes << std::endl;
	std::cout << fileHeader.numSystemBytes + fileHeader.fileOffset << std::endl;
	std::cout << filedata.size() << std::endl;
	std::vector<ChunkTag> chunks(fileHeader.numResources);
	std::vector<std::vector<uint8_t>::iterator> locations(fileHeader.numResources);
	int j = 0;
	int size = fileHeader.numResources;
	while (j < size)
	{
		locations[j] = iter;
		ReadChunk(iter, chunks[j], filedata.begin());
		j++;
	}
	j = 0;
	for (auto& i : chunks)
	{
		std::cout << (j+1) << " " << i.fileName << " " << i.fileOffset+fileHeader.fileOffset << " " << i.numOfBytesAfterTag <<
			 " " << i.fileName.size() <<std::endl;
		std::cout << i.chunkId << " " << i.chunkType << " " << i.contigOffset + fileHeader.fileOffset << " " << i.pad3 << " " << i.pad4 << std::endl;
		std::cout << "-------------" << std::endl;
		j++;
		
	}

	ExportChunks(chunks, filedata.begin(), fileHeader.fileOffset);
	
	return 0;
}