
#include <stdio.h>
#include <string.h>

#include "gif.h"

#define GIF87a 0
#define GIF89a 1

unsigned char *loadgif(FILE *infile, int *bufwidth, int *bufheight){
	int version;
	unsigned char *palette;
	int pallen;
	unsigned char *buf;
	unsigned int bpp;

	{
		/* Read header*/
		unsigned char packed;
		unsigned char header[13];
		
		if(fread(header, 1, 13, infile) != 13)
			return NULL;
		if(!memcmp("GIF89a", header, 6))
			version = GIF89a;
		else if(!memcmp("GIF87a", header, 6))
			version = GIF87a;
		else
			return NULL;
		
		*bufwidth = header[6] | header[7] << 8;
		*bufheight = header[8] | header[9] << 8;
		packed = header[10];
		bpp = (packed & 7) + 1;

		/* Read colormap */
		if(packed & 0x80){
			printf("Reading palette\n");
			pallen = 1 << bpp;
			printf("pallen: %i\n", pallen);
			palette = malloc(3 * pallen);
			fread(palette, 1, 3 * pallen, infile);
		}
	}

	{
		int blocktype;
		while((blocktype = fgetc(infile)) != EOF){
			if(blocktype == ';'){
				printf("end of file\n");
				break;
			}else if(blocktype == '!'){
				int bytecount, fncode;
				if((fncode = fgetc(infile)) == EOF)
					return NULL;
				while(1){
					bytecount = fgetc(infile);
					printf("Skipping extblock 0x%x\n", fncode);
					if(bytecount == EOF)
						return NULL;
					else if(bytecount > 0)
						fseek(infile, bytecount, SEEK_CUR);
					else
						break;
				}
			}else if(blocktype == ','){
				int imagex, imagey, imagew, imageh;
				unsigned char imagedesc[9];
				if(fread(imagedesc, 1, 9, infile) != 9)
					return NULL;
				if(imagedesc[8] & 0x80){
					printf("ERROR: local palettes not supported\n");
					return NULL;
				}
				if(imagedesc[8] & 0x40){
					printf("ERROR: interlaced images\n");
					return NULL;
				}

				imagex = imagedesc[0] | imagedesc[1] << 8;
				imagey = imagedesc[2] | imagedesc[3] << 8;
				imagew = imagedesc[4] | imagedesc[5] << 8;
				imageh = imagedesc[6] | imagedesc[7] << 8;

				if(imagex != 0 || imagey != 0 || imagew != *bufwidth || imageh != *bufheight){
					printf("%ix%i %ix%i\n", imagex, imagey, imagew, imageh);
					printf("TODO: allow this sort of thing\n");
					return NULL;
				}

				{
					int initcodesize, curcodesize;
					initcodesize = fgetc(infile);
					if(initcodesize < 1 || initcodesize > 9){
						printf("bad code size %i\n", initcodesize);
						return NULL;
					}
					curcodesize = initcodesize + 1;
				}

				break;
			}else{
				printf("Unknown block type %i\n", blocktype);
				return NULL;
			}
		}
	}

	printf("end\n");
	return NULL;
}

