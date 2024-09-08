

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <regex>


#include "s3tc.h"
#include "SMB.h"
#include "FileManager.h"
#include "Exporter.h"




// each string is null terminated except the first file

// 66 bytes in between each filename

// after joint names it is number * 116 bytes until next entry

// file offset is where the data actually starts (maybe?)

//numResources corresponds to the number of BEGINNINGSMBChunkS are present in a file

//each datachunk has an identifier that brackets that beginning and end of the chunk 
//before the size of the filestring doesn't seem to be size or offset of chunk

//number of total data from point in chunk is 16 bytes after BEGINNINGSMBChunk

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

// so it is possible that all things with continguous offset (not sr2 files) are stored after the smbfile in order and that the sr2s are after the fileOffset + numContiguousByte 

// confirmed at least that all things that impact a continguous offset are placed at the contiguous offset where they are!!!

//21 bytes after texture string until the definition (at for what I need!!!)

//both files oddly use the same legacy directx values for the texture types. will specify in enumeration. WEIRD!!!!
#define BEGINNINGSMBChunk 0xa77e4dfa
//#define DATASMBChunk 0xcbe402b6
#define ENDTAG 0xbeef1234
#define ENDGEOTAG 0xdc7c9d74



int main()
{

	std::cout << sizeof(SMBFile) << std::endl;
	std::cout << sizeof(SMBChunk) << std::endl;
	std::string MainSMB = "strangernew.smb";
	
	FileManager::SetCurrentDirectory(MainSMB.substr(0, MainSMB.find_last_of('.')));
	
	SMBFile smbfile{};
	smbfile.LoadFile(MainSMB);
	std::cout << smbfile.numContiguousBytes << std::endl;
	std::cout << smbfile.numSystemBytes << std::endl;
	std::cout << smbfile.numSystemBytes + smbfile.fileOffset << std::endl;

	int j = 0;
	
	for (auto& i : smbfile.chunks)
	{
		std::cout << (j++) << "\n" << i << "\n";
	}
	
	
	Exporter::ExportChunksFromFile(smbfile);
	
	return 0;
}

//compression algorithm details for original xbox:
/*
the first two elements are paired for most part and repeat


*/


/*
	compression for hd version
	address of global_stranger.smb string is 0076b320
*/