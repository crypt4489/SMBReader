#include "Exporter.h"

#include <cstdint>
#include <format>
#include <fstream>

#include "AppTextureImpl.h"
#include "FileManager.h"
#include "DXTCompression.h"
#include "SMBTexture.h"


//IndexedRenderable or Renderable, not sure

//0x24 is primitive type (internal)

//0x28 is indicies count



//VPTR TABLE
//3-4 4) 3 float 5) packed
//5-8 5-6) 3float 7-8) packed Setter Getter for Position -pospack6 6 bytes, pospack4 4 bytes, regular position is 4 bytes per componenet, 3 in total
//9-10-11 Normals - 4 bytes compressed-same format as position for compressed 4
// uncompressed normals are 4 bytes per component(3, 12 in total)
//12-13-14 Diffuse -- diffuse is 4 bytes, 3 bytes for RGB only
//15-17 Textures - Tex1 uncompressed 4 bytes each coord, 2 bytes compressed (8 total, 4 total)
// Tex2 8 uncompressed 4 bytes compressed (16 total, 8 total)
// Tex3 16 uncompressed 8 bytes compressed per cord (32 total, 16 total)
// Compressed textures are stored in 4:12

//18-19 Bone and weight indicator
//20-24 Bone and Weights -Bone2 is 4 bytes
//25-27 Binormals
//28-32 sprites

std::vector<std::vector<glm::vec3>> positions;
std::vector<std::vector<glm::vec3>> normals;
std::vector<std::vector<glm::vec2>> texCoords;
std::vector<std::vector<glm::vec2>> texCoordsP2;
std::vector<std::vector<glm::ivec2>> bonesID;
std::vector<std::vector<glm::vec2>> weights;

std::vector<std::vector<uint16_t>> indices;;


const int MaterialDefSize = 136; //bytes
const int GeometryBaseDefSize = 72; //bytes

const uint64_t RenderableByVertex = 5292387491162064043;
const uint64_t RenderableByIndex = 5792287050554945273;

const float dx = 3.051851e-05;

glm::vec2 converttexcoords16(int16_t* huh)
{
	float x = huh[0] * dx * 16.0f;
	float y = huh[1] * dx * 16.0f;


	return glm::vec2(x, y);
}


float top2[6];


const float ax = 4.661165e-10;
const float bx = 4.665726e-10;

glm::vec3 pack6decomp(int16_t* hello, float *top)
{
	float x = ((hello[0] * dx) + 1.0) * 0.5;
	float y = (((hello[1] * dx)) + 1.0) * 0.5;
	float z = (((hello[2] * dx)) + 1.0) * 0.5;

	x = ((top[3] - top[0]) * x) + top[0];
	y = ((top[4] - top[1]) * y) + top[1];
	z = ((top[5] - top[2]) * z) + top[2];

	return glm::vec3(x, y, z);
}

enum RenderableFlags
{
	VBRENDERABLE = 0,
	IVBRENDERABLE = 1,
};

int sizes[3] = {
	18,
	18,
	14,
};

enum SMBVertexSize
{
	PosPack6_CNorm_C16Tex1_Bone2_Size = 0,
	PosPack6_C16Tex2_Bone2_Size = 1,
	PosPack6_C16Tex1_Bone2_Size = 2,
};

enum SMBVertexTypes
{
	PosPack6_CNorm_C16Tex1_Bone2 = 122,
	PosPack6_C16Tex2_Bone2 = 119,
	PosPack6_C16Tex1_Bone2 = 114,
};

std::vector<int> renderableTypes;
std::vector<SMBVertexTypes> vertexTypes;
std::vector<int> indicesCount;
std::vector<int> verticesCount;
std::vector<int> vertexOffset1;
std::vector<int> indicesOffset;
std::vector<PrimitiveType> primitiveType;

static void ProcessGeometry(char* data)
{
	char* iter = data;
	char* axialBox = iter + 36;
	memcpy(top2, axialBox, sizeof(float) * 6);


	char* material = iter + GeometryBaseDefSize;

	while (true)
	{
		int copy =  *((int*)material);
		if (copy != 737893) break;

		uint64_t *burrr = ((uint64_t*)(material + 4));
		uint64_t copy2 = *burrr;
		
		if (copy2 == RenderableByIndex)
		{
			char* renderable = material + 4 + 8 + 8;
			int indexCount = *((int*)(renderable + (18 + 48)));
			int vertexCount = *((int*)(renderable + (18 + 16)));
			int vertexGroupSize = *((int*)(renderable + (18 + 16 + 16)));
			int vertexGroupSize2 = *((int*)(renderable + (18 + 16 + 16 + 8)));
			SMBVertexTypes vertexType = *((SMBVertexTypes*)(renderable + (18 + 16 + 8)));
			indicesCount.push_back(indexCount);
			verticesCount.push_back(vertexCount);
			renderableTypes.push_back(IVBRENDERABLE);
			vertexTypes.push_back(vertexType);
			vertexOffset1.push_back(vertexGroupSize);
			indicesOffset.push_back(vertexGroupSize2);
			material += (64 + 13 + 13);
		}
		else if (copy2 == RenderableByVertex)
		{
			char* renderable = material + 4 + 8 + 8;
			int vertexCount = *((int*)(renderable + (18 + 16)));
			int vertexGroupSize = *((int*)(renderable + (18 + 16 + 16)));
	
			SMBVertexTypes vertexType = *((SMBVertexTypes*)(renderable + (18 + 16 + 8)));
			verticesCount.push_back(vertexCount);
			renderableTypes.push_back(VBRENDERABLE);
			vertexTypes.push_back(vertexType);
			vertexOffset1.push_back(vertexGroupSize);
	
			material += (64 + 18);
		}
		else {

			material += MaterialDefSize;

			copy = *((int*)material);


			while (copy != 0)
			{
				material += (copy + 4);
				copy = *((int*)material);
			}

			material += 8;

			copy = *((int*)material);

			while (copy != 0)
			{
				if (copy == 737893) break;
				material += (copy + 4);
				copy = *((int*)material);
				if (copy != 0) {
					break;
				}
				material += 8;
				copy = *((int*)material);

			}

			copy = *((int*)material);

			if (copy == 0)
			{
				material = (char*)((uintptr_t)material + (((uintptr_t)material | 0xf) - (uintptr_t)material));
			}
		}
	}

}

void Exporter::ExportChunksFromFile(SMBFile& smb)
{

	auto& chunk = smb.chunks;
	for (size_t j = 0; j<smb.chunks.size(); j++)
	{
		
			
			
		switch (smb.chunks[j].chunkType)
		{
		case GEO:
		{

			std::string name = "GEOSTRANGER2.bin";
			FileHandle* handle, * handle2;
			auto outputfilehandle = FileManager::OpenFile(name, std::ios::binary | std::ios::out, handle);

			if (!outputfilehandle)
			{
				std::cerr << "Something bad happened\n";
				return;
			}

			auto& outputFile = handle->streamHandle;

			std::vector<char> output;

			handle2 = FileManager::GetFile(smb.id);

			auto& geoChunk = handle2->streamHandle;

			size_t seekpos2 = chunk[j].offsetInHeader;

			geoChunk.seekg(seekpos2);

			size_t howbout = chunk[j].numOfBytesAfterTag-(16+chunk[j].stringsize);

			std::vector<char> data2(howbout);

			geoChunk.read(data2.data(), howbout);

			ProcessGeometry(data2.data());

			size_t seekpos = chunk[j].contigOffset + smb.fileOffset;

			seekpos2 = chunk[j + 1].contigOffset + smb.fileOffset;

			geoChunk.seekg(seekpos);

			size_t size = seekpos2 - seekpos;

			std::vector<char> data(size);

			geoChunk.read(data.data(), size);

			outputFile.write(data.data(), size);



			FileManager::RemoveOpenFile(outputfilehandle.value());


			size_t renderables = renderableTypes.size();

			positions.resize(renderables);
			texCoords.resize(renderables);
			weights.resize(renderables);
			bonesID.resize(renderables);
			indices.resize(renderables);
			normals.resize(renderables);
			texCoordsP2.resize(renderables);

			size_t vbCount = 0;
			size_t ibCount = 0;


			for (size_t ii = 0; ii < renderables; ii++)
			{

				unsigned char* g = (unsigned char*)(data.data() + vertexOffset1[ii]);
				int vertexSize = 0;
				SMBVertexTypes type = vertexTypes[ii];


				switch (type)
				{
				case PosPack6_CNorm_C16Tex1_Bone2:
					vertexSize = sizes[PosPack6_CNorm_C16Tex1_Bone2_Size];
					break;
				case PosPack6_C16Tex2_Bone2:
					vertexSize = sizes[PosPack6_C16Tex2_Bone2_Size];
					break;
				case PosPack6_C16Tex1_Bone2:
					vertexSize = sizes[PosPack6_C16Tex1_Bone2_Size];
					break;
				}


				int vSize = verticesCount[ii];

				auto& bonesRef = bonesID[ii];
				bonesRef.resize(vSize);
				auto& weightRef = weights[ii];
				weightRef.resize(vSize);
				auto& positionsRef = positions[ii];
				positionsRef.resize(vSize);
				auto& tex1Ref = texCoords[ii];
				tex1Ref.resize(vSize);

				for (int i = 0; i < vSize; i++)
				{
					switch (type) {
					case PosPack6_CNorm_C16Tex1_Bone2:
					{

						auto& normalRef = normals[ii];
						normalRef.resize(vSize);

						uint32_t l = g[2], h = g[3];
						bonesRef[i].x = g[0];
						bonesRef[i].y = g[1];
						weightRef[i].x = ((float)l) * 0.00392156f;
						weightRef[i].y = ((float)h) * 0.00392156f;

						uint32_t norms = *(uint32_t*)&g[8];

						normalRef[i].x = ((float)(norms << 0x15)) * ax;
						normalRef[i].x = ((float)((norms & 0xfffff800) << 10)) * ax;
						normalRef[i].x = ((float)((norms & 0xffc00000) << 10)) * bx;

						int16_t t[2];
						t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
						t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];
				
						tex1Ref[i] = converttexcoords16(t);


						int16_t s[3];

						s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
						s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
						s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

						positionsRef[i] = pack6decomp(s, top2);

						break;
					}
					case PosPack6_C16Tex2_Bone2:
					{
						uint32_t l = g[2], h = g[3];
						bonesRef[i].x = g[0];
						bonesRef[i].y = g[1];
						weightRef[i].x = ((float)l) * 0.00392156f;
						weightRef[i].y = ((float)h) * 0.00392156f;


						int16_t t[2];
						t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
						t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];

						tex1Ref[i] = converttexcoords16(t);


						auto& tex2Ref = texCoordsP2[ii];
						tex2Ref.resize(vSize);

			
						t[0] = (((int16_t)g[9] & 0xff) << 8) | g[8];
						t[1] = (((int16_t)g[11] & 0xff) << 8) | g[10];

						tex2Ref[i] = converttexcoords16(t);

						int16_t s[3];

						s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
						s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
						s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

						positionsRef[i] = pack6decomp(s, top2);


						break;
					}
					case PosPack6_C16Tex1_Bone2:
					{
						uint32_t l = g[2], h = g[3];
						bonesRef[i].x = g[0];
						bonesRef[i].y = g[1];
						weightRef[i].x = ((float)l) * 0.00392156f;
						weightRef[i].y = ((float)h) * 0.00392156f;

						int16_t t[2];
						t[0] = (((int16_t)g[5] & 0xff) << 8) | g[4];
						t[1] = (((int16_t)g[7] & 0xff) << 8) | g[6];

						tex1Ref[i] = converttexcoords16(t);

						int16_t s[3];

						s[0] = (((int16_t)g[13] & 0xff) << 8) | g[12];
						s[1] = (((int16_t)g[15] & 0xff) << 8) | g[14];
						s[2] = (((int16_t)g[17] & 0xff) << 8) | g[16];

						positionsRef[i] = pack6decomp(s, top2);

						break;
					}
					}
					g += vertexSize;
				}


				if (renderableTypes[ii] == IVBRENDERABLE)
				{
					auto& currIB = indices[ibCount];
					currIB.resize(indicesCount[ibCount]);
					size_t buh = currIB.size();
					size_t accum = 0;
					uint32_t* god = (uint32_t*)(data.data() + indicesOffset[ibCount]);
					while (buh > 0) {
						
						uint32_t stride = (*god >> 0x12) & 0x7ff;
						uint32_t indexType = (*god & 0x3ffff);

						if (indexType == 0x1800)
						{
		
							memcpy(currIB.data()+accum, god + 1, stride * 4);
							accum += (stride * 2);
							buh -= (stride * 2);
						}

						else if (indexType == 0x1808)
						{
							uint16_t blah;
							memcpy(&blah, god+1, 2);
							currIB[indicesCount[ibCount]-1] = blah;
							accum += 1;
							buh -= 1;
						}
						else if (indexType == 0x17fc) {
							PrimitiveType ptype;
							memcpy(&ptype, god + 1, 4);
	
						}

						

						god += (stride + 1);
					
					}

					ibCount++;
					

				
				}

				

			}

			for (size_t i = 0; i < renderables; i++)
			{
				auto& weightRef = weights[i];
				int vSize = verticesCount[i];
				for (int j = 0; j < vSize; j++)

				{
					float x = weightRef[j].x;
					float y = weightRef[j].y;
					float z = x + y;
					if (z < 0.99f || z > 1.0f)
					{
						std::cout << z << std::endl;
					}
				}
			}

			for (size_t i = 0; i < renderables; i++)
			{
				auto& ref = normals[i];
				int vSize = verticesCount[i];
				if (!ref.size()) continue;
				for (int j = 0; j < vSize; j++)

				{
					float x = ref[j].x;
					float y = ref[j].y;
					float z = ref[j].z;
					if (z > 1.0f || z < -1.0f)
					{
						std::cout << "NORMALZ: " << i << " " << j << " " << z <<  std::endl;
					}

					if (y > 1.0f || y < -1.0f)
					{
						std::cout << "NORMALY: " << i << " " << j << " " << y << std::endl;
					}

					if (x > 1.0f || x < -1.0f)
					{
						std::cout << "NORMALX: " << i << " " << j << " " << x << std::endl;
					}
				}
			}

			for (size_t i = 0; i < renderables; i++)
			{
				auto& ref = texCoords[i];
				int vSize = verticesCount[i];
				if (!ref.size()) continue;
				for (int j = 0; j < vSize; j++)

				{
					float x = ref[j].x;
					float y = ref[j].y;
					

					if (y > 1.0f || y < -1.0f)
					{
						std::cout << "TEXTUREY: " << i << " " << j << " " << y << std::endl;
					}

					if (x > 1.0f || x < -1.0f)
					{
						std::cout << "TEXTUREX: " << i << " " << j << " " << x << std::endl;
					}
				}

				auto& ref2 = texCoordsP2[i];
				if (!ref2.size()) continue;

				for (int j = 0; j < vSize; j++)

				{
					float x = ref2[j].x;
					float y = ref2[j].y;


					if (y > 1.0f || y < -1.0f)
					{
						std::cout << "TEXTURE2Y: " << i << " " << j << " " << y << std::endl;
					}

					if (x > 1.0f || x < -1.0f)
					{
						std::cout << "TEXTURE2X: " << i << " " << j << " " << x << std::endl;
					}
				}
			}

			for (size_t i = 0; i < renderables; i++)
			{
				auto& positionRef = positions[i];
				int vSize = verticesCount[i];
				for (int j = 0; j < vSize; j++)

				{
					float x = positionRef[j].x;
					float y = positionRef[j].y;
					float z = positionRef[j].z;
					if ((z < top2[2] || z > top2[5]))
					{
						std::cout << "POSITIONZ: " << i << " " << j << " " << z << " " << top2[2] << " " << top2[5] << std::endl;
					}

					if ((y < top2[1] || y > top2[4]))
					{
						std::cout << "POSITIONY: " << i << " " << j << " " << y << std::endl;
					}

					if ((x < top2[0] || x > top2[3]))
					{
						std::cout << "POSITIONX: " << i << " " << j << " " << x << std::endl;
					}
				}
			}

			break;
		}
		
		case TEXTURE:
			ExportTextureFromFile(smb, chunk[j]);
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


void Exporter::ExportTextureFromFile(const SMBFile& smb, const SMBChunk& chunk)
{
	std::string name = FileManager::ExtractFileNameFromPath(chunk.fileName);

	auto pathToTextures = FileManager::SetupDirectory(name);

	SMBTexture tex(smb, chunk);

	auto ptr = tex.data;

	for (uint32_t i = 0; i < tex.miplevels; i++)
	{
		std::string writeFileName = name + std::to_string(i + 1) + ".bmp";

		auto writePath = pathToTextures / writeFileName;

		uint32_t writeWidth = tex.width >> i;
		uint32_t writeHeight = tex.height >> i;

		FileHandle* handle;

		auto outputfilehandle = FileManager::OpenFile(writePath, std::ios::binary | std::ios::out, handle);

		if (!outputfilehandle)
		{
			std::cerr << "Something bad happened\n";
			return;
		}

		auto& outputFile = handle->streamHandle;

		TexUtils::BMP::WriteOutBMPHeaders(outputFile, writeWidth, writeHeight);

		std::byte* bgra = ptr;

		std::vector<char> input;

		int compressedSize = 0;
		input.resize(writeWidth * writeHeight * 4);;
		switch (tex.type)
		{
		case X8L8U8V8:
			std::cerr << "X8L8U8V8 format is not exportable\n";
			return;
		case DXT1:
			//compressedSize = DXTCompression::DXT1CompressedSize(writeWidth, writeHeight);
			
			DXTCompression::BlockDecompressImageDXT1(writeWidth, writeHeight, (unsigned char*)ptr, (unsigned long*)input.data());
			break;
		case DXT3:
			//compressedSize = DXTCompression::DXT3CompressedSize(writeWidth, writeHeight);
			DXTCompression::BlockDecompressImageDXT3(writeWidth, writeHeight, (unsigned char*)ptr, (unsigned char*)input.data());
			break;
		case R8G8B8A8:
			std::memcpy(input.data(), ptr, writeWidth * writeHeight * 4);
			break;
		default:
			std::cerr << "Unsupported/Unknown texture type " << tex.type << "\n";
			return;
		}

		TexUtils::BGRATexture(input.data(), tex.height >> i, tex.width >> i, 4);


		uint32_t bpr = writeWidth * 4;

		uint32_t offset = (writeHeight - 1) * bpr;

		for (uint32_t i = 0; i < writeHeight; i++)
		{
			outputFile.write(input.data() + offset, bpr);
			offset -= bpr;
		}

		ptr += tex.imageSizes[i];

		FileManager::RemoveOpenFile(outputfilehandle.value());
	}
}

void Exporter::ExportToOBJFormat(std::vector<Vertex>& vertices, std::string& outputFile)
{
	using ExportHelper::operator<<;

	FileHandle* handle;

	auto fileRet = FileManager::OpenFile(outputFile, std::ios_base::out, handle);

	if (!fileRet)
	{
		throw std::runtime_error("Cannot open file for OBJ export " + outputFile);
	}


	auto& fileStream = handle->streamHandle;

	for (const auto& vert : vertices) {
		fileStream << "v " << vert.POSITION;
	}

	for (const auto& vert : vertices) {
		fileStream << "vt " << vert.TEXTURE;
	}

	for (const auto& vert : vertices) {
		fileStream << "vn " << vert.NORMAL;
	}

	FileManager::RemoveOpenFile(fileRet.value());
}