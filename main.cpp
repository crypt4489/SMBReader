

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
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

void WriteOutSMBBMP(std::string name, std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end)
{
	std::ofstream filehandle(name);

	if (!filehandle.is_open())
		throw std::runtime_error("BMP file is unable to be opened");

	BitmapFileHeader fileheader{ 0x4d42, 14+40+(256*256*3), 0, 0, 0x36};
	BitmapInfoHeader infoheader{ 40, 256, 256, 1, 24, 0, 0, 0, 0, 0, 0 };

	filehandle.write(reinterpret_cast<char*>(&fileheader.bfType), 14);
	filehandle.write(reinterpret_cast<char*>(&infoheader.biSize), 40);
	while (begin != end)
	{
		
		filehandle.put(*begin);
		begin+=1;
	}
	
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

enum chunktype
{
	GEO = 0,
	GR2 = 6,
	Joints = 20,
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

// 72 10 FD 2D is the tag for material (texture) definition

// 62 56 72 49 is sequence for the indexed renderable class definition 62 56 62 50 is for pb indexed renderable class definition

// in geo file, uses XX XX FF 00 for demarcation between vertices structs?

// fc 17 04 00 is beginning and end of indices (of some kind)

// texture resources are 73 bytes in length (not including string size) from tag - 16 bytes in the chunk tag

// gr2 is 43 bytes excluding the string size minus the 16 bytes up to the string

// for joints, there are joints name and then the number of matrices for the joints. These matrices are packed within 112 bytes of data. followed by the number of matrices is 4byte indices

// for every pb indexed renderable there is a fc 17 04 00 pair that corresponds to the number of indices in the indexed renderable. 

#define BEGINNINGCHUNKTAG 0xa77e4dfa
//#define DATACHUNKTAG 0xcbe402b6
#define ENDTAG 0xbeef1234
#define ENDGEOTAG 0xdc7c9d74

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
	uint32_t pad;
	uint32_t fileOffset; //monotonically increasing for each chunk
	uint32_t numOfBytesAfterTag;
	uint32_t pad3;
	uint32_t tag;
	uint32_t pad4;
	uint32_t stringSize;
	std::string fileName;
} ChunkTag;

template <typename C> inline void CopyBytes(std::vector<uint8_t>::iterator& file, int size, C copy)
{
	if constexpr(std::is_pointer_v<C>)
	{
		std::copy(file, file + size, reinterpret_cast<char*>(copy));
	}
	else
	{
		std::copy(file, file + size, copy);
	}
	
	file += size;
}

void ReadHeader(std::vector<uint8_t>::iterator &file, SMBFile* header)
{
	CopyBytes(file, 4, &header->magic);
	CopyBytes(file, 4, &header->version);
	int stringSize;
	CopyBytes(file, 4, &stringSize);
	header->name.resize(stringSize);
	CopyBytes(file, stringSize, header->name.begin());
	CopyBytes(file, 32, &header->fileOffset);
}

void ReadChunk(std::vector<uint8_t>::iterator& file, ChunkTag* tag)
{
	int magic;
	CopyBytes(file, 4, &magic);
	CopyBytes(file, 8*4, &tag->chunkType);
	file += 4;
	CopyBytes(file, 4, &tag->stringSize);
	tag->fileName.resize(tag->stringSize);
	CopyBytes(file, tag->stringSize, tag->fileName.begin());
	file += (tag->numOfBytesAfterTag - (tag->stringSize + 16));
}

int main()
{
	std::cout << sizeof(BitmapFileHeader) << sizeof(BitmapInfoHeader) << std::endl;
	SMBFile fileHeader{};
	std::vector<uint8_t> filedata = LoadFile("come.smb");
	auto iter = filedata.begin();
	ReadHeader(iter, &fileHeader);
	int end;
	CopyBytes(iter, 4, &end);
	
	if (end != ENDTAG)
	{
		std::cerr << "Cannot read file header\n";
		return -1;
	}
	else
	{
		std::cerr << "Success\n";
	}
	std::vector<ChunkTag> chunks(fileHeader.numResources);
	std::vector<std::vector<uint8_t>::iterator> locations(fileHeader.numResources);
	int j = 0;
	int size = fileHeader.numResources;
	while (j < size)
	{
		locations[j] = iter;
		ReadChunk(iter, &chunks[j]);
		j++;
	}
	j = 0;
	for (auto& i : chunks)
	{
		std::cout << (j+1) << " " << i.fileName << " " << i.fileOffset+fileHeader.fileOffset << " " << i.numOfBytesAfterTag <<
			 " " << i.fileName.size() <<std::endl;
		i.fileOffset += fileHeader.fileOffset;
		j++;
	}
	auto geoIndex = filedata.begin() + fileHeader.fileOffset;
	size = chunks[5].fileOffset - chunks[4].fileOffset;
	std::cout << "size : " << size << std::endl;
	std::cout << chunks[5].fileOffset << " " << chunks[5].fileName << std::endl;
	std::cout << chunks[4].fileOffset << " " << chunks[4].fileName << std::endl;
	/*for (int i = 0; i < size; i += 4 * 16)
	{
		std::cout << i << "---";
		for (int k = 0; k < 16; k++)
		{
			float f;
			CopyBytes(geoIndex, 4, &f);
			std::cout << f << " ";
		}
		std::cout << "\n";
	} */
	
	int count = 0;
	auto loop = filedata.begin() + chunks[73].fileOffset;
	int sizet = chunks[74].fileOffset - chunks[73].fileOffset;
	/*while (loop < filedata.begin() + chunks[73].fileOffset + sizet)
	{
		//std::cout << count << " ";
		for (int i = 0; i < 4; i++)
		{
			if (count > 0)
			{
				float f;
				CopyBytes(loop, 4, &f);
				//std::cout << " " << f;
				count += 4;
			}
			else {
				int f;
				CopyBytes(loop, 4, &f);
				//std::cout << " " << f;
				count += 4;
			}
		}
		std::cout << "\n";
	} */
	/*count = 0;
	loop = filedata.begin() + chunks[74].fileOffset;
	while (loop < filedata.begin() + chunks[74].fileOffset + 4000)
	{
		std::cout << count << " ";
		for (int i = 0; i < 4; i++)
		{
			if (count < 15000)
			{
				float f;
				CopyBytes(loop, 4, &f);
				std::cout << " " << f;
				count += 4;
			}
			else {
				int f;
				CopyBytes(loop, 4, &f);
				std::cout << " " << f;
				count += 4;
			}
		}
		std::cout << "\n";
	}

	*/
	std::printf("%s %x %d\n", chunks[73].fileName.c_str(), chunks[73].fileOffset, chunks[73].fileOffset);
	std::printf("%x %d\n", chunks[74].fileOffset, chunks[74].fileOffset - chunks[73].fileOffset);
	//WriteOutSMBBMP("test.bmp", filedata.begin() + chunks[42].fileOffset, filedata.begin() + chunks[42].fileOffset + (256 * 256 * 3));

	return 0;
}