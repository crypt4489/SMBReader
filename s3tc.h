#ifndef S3TC_H
#define S3TC_H

unsigned long PackRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void DecompressBlockDXT1(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image);
int BlockDecompressImageDXT1(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image);
void DecompressBlockDXT5(unsigned long x, unsigned long y, unsigned long width, const unsigned char *blockStorage, unsigned long *image);
int BlockDecompressImageDXT5(unsigned long width, unsigned long height, const unsigned char *blockStorage, unsigned long *image);
int BlockDecompressImageDXT3(unsigned long width, unsigned long height, const unsigned char* blockStorage, unsigned char* image);
int DXT1CompressedSize(unsigned long width, unsigned long height);
int DXT3CompressedSize(unsigned long width, unsigned long height);
int DXT5CompressedSize(unsigned long width, unsigned long height);


#endif // S3TC_H
