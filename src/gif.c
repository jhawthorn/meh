
#include <stdio.h>
#include <string.h>

#include "meh.h"

#define GIF87a 0
#define GIF89a 1

unsigned char *loadgif(FILE *infile, int *bufwidth, int *bufheight){
	int version;
	unsigned char *palette;
	int pallen;
	unsigned char *buf = NULL;
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

				buf = malloc((*bufwidth) * (*bufheight) * 3);

				{
					unsigned int initcodesize, curcodesize;
					unsigned int clearcode, endcode, newcode;
					int bytesleft = 0;
					int i;

					/* Get code size */
					initcodesize = fgetc(infile);
					if(initcodesize < 1 || initcodesize > 9){
						printf("bad code size %i\n", initcodesize);
						return NULL;
					}
					curcodesize = initcodesize + 1;
					clearcode = 1 << initcodesize;
					endcode = clearcode + 1;
					newcode = endcode + 1;

					int count = 0;
					for(;;){
						int byte0 = -1, byte1 = -1, byteshift = 0;
						if(1){
							bytesleft = fgetc(infile);
							printf("%i\n", bytesleft);
							if(bytesleft < 0){
								printf("unexpected EOF (0)\n");
								return NULL;
							}else if(bytesleft == 0)
								break;
						}
						printf("start %i, %i\n", count++, bytesleft);
						for(;;){
							int code;
							/* get code */
							
							while(byte1 == -1){
								if((byte1 = fgetc(infile)) == EOF){
									printf("unexpected EOF (1)\n");
									return NULL;
								}
								if(byte0 == -1){
									byte0 = byte1;
								}
								bytesleft--;
							}
							if(bytesleft < 0){
								break;
							}
							code = (byte0 | byte1 << 8) >> byteshift & ((1 << curcodesize) - 1);
							byteshift += curcodesize;
							while(byteshift >= 8){
								byte0 = byte1;
								byte1 = -1;
								byteshift -= 8;
							}

							if(code == clearcode){
								printf("clearcode\n");
								curcodesize = initcodesize + 1;
								newcode = endcode + 1;
							}else if(code == endcode){
								printf("endcode with %i bytes left\n", bytesleft);
								break;
							}else{
								newcode++;
							}

							/* process code */
							if(newcode == 1 << curcodesize){
								curcodesize++;
							}
						}
					}
					
					/*for(i = 0; i < (*bufwidth) * (*bufheight); i++){
					}*/
					printf("good end\n");
					return buf;
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

