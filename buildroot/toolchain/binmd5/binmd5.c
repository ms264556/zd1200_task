#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>         /* For _MAX_PATH definition */
#include <malloc.h>
#include <fcntl.h>
// #include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>


#if 0
// TODO:  need to fix nested include problems
#include "v54bsp/ar531x_bsp.h"
#else

#endif

#define FALSE	0
#define TRUE	1
#define EOS     '\0'

#define HDR_SIG 0
#define	MAXTYPESTRING 1024


struct stat bufFileStat;

/* Print help and exit */
void helpexit(void)
{
	printf("\nBINMD5: create a firmware download image from BIN file.");
	printf("\n");
	printf("\nUsage:");
	printf("\n");
	printf("\nbinmd5 -i<input_file> -r<ramdisk> -d -o<output_file> -e<entry_point>");
	printf("\nbinmd5 -i<input_file> -r<ramdisk> -o<output_file> -e<entry_point>");
	printf("\n");
	printf("\nbinmd5 -l -i<input_file>"); 
	printf("\n");
	printf("\n  <input_file> is a BIN format FW file.");
	printf("\n  <ramdisk> is a BIN format root file.");
	printf("\n  -d will display the MD5 of the input BIN file.");
	printf("\n  Assumed that the file is l7 compressed");
	printf("\n");
	fcloseall();
	exit(0);
}


/*  Main program  */
int main(int argc, char **argv)
{
	int i;
	int rc;
	int flagI, flagD, flagR, flagS, flagT, flagM;
	int dump_header = 0;
	char *cp, *in, *rfs, opt, *ins, *inct;
	char *out = NULL;
	unsigned char sw;
	FILE *inElf, *inRfs, *inPlat, *outFile, *inSign, *inCert, *inModel;
	unsigned char buffer[16384], signature[16];
	int numread;
	u_int32_t entry_point = 0;
	u_int32_t load_address = 0;
	u_int32_t output_len  = 0;

	unsigned char * build_version = BR_BUILD_VERSION;
	unsigned char * product_type = "" ;
	unsigned char * board_type = "" ;
	unsigned char * board_class = "" ;

	flagI = flagD = flagR = flagS = flagT= flagM= 0;

	if (argc < 2)
		helpexit();

	for (i = 1; i < argc; i++)
	{
		cp = argv[i];
		if (*cp == '-')
		{
			opt = *(++cp);
			if (islower(opt))
			{
				opt = toupper(opt);
			}
			cp++;

			switch (opt)
			{
			/* -L  --  Display header info */
			case 'L':
				dump_header = 1;
				break;

			/* -Vversion -- version string */
			case 'V':
				build_version = (unsigned char *) cp;
				break;

			/* -Bboard_type -- board_type string */
			case 'B':
				board_type = (unsigned char *) cp;
				break;

			/* -Cboard_class -- board_class string */
			case 'C':
				board_class = (unsigned char *) cp;
				break;

			/* -Pproduct_type -- product string */
			case 'P':
				product_type = (unsigned char *) cp;
				break;

			/* -Iinputfile -- Input ELF file, required argument */
			case 'I':
				if ((inElf = fopen((unsigned char *)cp, "rb")) == NULL)
				{
					printf("Cannot open input file %s\n", cp);
					return 2;
				}
				else
				{
					flagI = 1;
					printf("opened input file %s\n", cp);
				}
				in = cp;
				break;

			/* -Rinputfile -- Input root file syatem */
			case 'R':
				if ((inRfs = fopen((unsigned char *)cp, "rb")) == NULL)
				{
					printf("Cannot open root file %s\n", cp);
					return 2;
				}
				else
				{
					flagR = 1;
					printf("opened root file %s\n", cp);
				}
				rfs = cp;
				break;

			/* -Minputfile -- Input model support file, required argument */
			case 'M':
				if ((inModel = fopen((unsigned char *)cp, "rb")) == NULL) {
					printf("Cannot open model support file %s\n", cp);
					return 2;
				} else {
					flagM = 1;
					printf("opened model support file %s\n", cp);
				}
				ins = cp;
				break;

			/* -Sinputfile -- Input signature file, required argument */
			case 'S':
				if ((inSign = fopen((unsigned char *)cp, "rb")) == NULL)
				{
					printf("Cannot open signature file %s\n", cp);
					return 2;
				}
				else
				{
					flagS = 1;
					printf("opened signature file %s\n", cp);
				}
				ins = cp;
				break;

			/* -Tinputfile -- Input certificate file, required argument */
			case 'T':
				if ((inCert = fopen((unsigned char *)cp, "rb")) == NULL)
				{
					printf("Cannot open certificate file %s\n", cp);
					return 2;
				}
				else
				{
					flagT = 1;
					printf("opened certificate file %s\n", cp);
				}
				inct = cp;
				break;

			/* -D  --  Display MD5 of input ELF file */
			case 'D':
				flagD = 1;
				break;

			/* -Ooutputfile */
			case 'O':
				out = (unsigned char *)cp;
				break;

			/* -Eentry_point */
			case 'E':
				entry_point = strtoul((unsigned char*)cp, NULL, 0);
                printf("entry_point = 0x%x -- argv[]='%s'\n", entry_point, cp);
				break;

			/* -Aload_address */
			case 'A':
				load_address = strtoul((unsigned char*)cp, NULL, 0);
                printf("load_address = 0x%x -- argv[]='%s'\n", load_address, cp);
				break;

			/* -? -H  --  Print help info. */
			case '?':
			case 'H':
			default:
				helpexit();
				break;

			} // end switch
		} // end if
	} // end for loop parse cmd line

	/* check cmd line arg consistancy */
	if( flagI == 0 )
	{
		printf("Input file required.\n");
		return(3);
	}
	

	if ( out == NULL ) {
		printf("Output file required.\n");
		return(4);
	}

#if 0 /* powerpc entry point is 0x0 */
	if ( entry_point == 0 ) {
		printf("Entry_point required.\n");
		return(3);
	}
#endif

#if 0 /* powerpc entry point is 0x0 */ && ( BINHDR_VERSION > BINHDR_VERSION_2 )
	if ( load_address == 0 ) {
		printf("Load_address required.\n");
		return(3);
	}
#endif

	if ((outFile = fopen((unsigned char *)out, "wb")) == NULL)
	{
		printf("Cannot open output file %s for writing\n", out);
		return 3;
	}


	/* Signature for this Header */


	/* Read and check input file */
	while ((i = fread(buffer, 1, sizeof buffer, inElf)) > 0)
	{
		rc = fwrite(buffer, sizeof(unsigned char ), i, outFile);
		if ( rc != i ) {
			printf("fwrite(%d) = %d ... failed\n", i, rc);
			fcloseall();
			return 5;
		}
		output_len += rc;
	}
	fclose(inElf);
	printf("Elf end @0x%x\n", output_len);

	if (flagR)
	{
		int pad_size = ((output_len + 0xffff) & 0xffff0000) - output_len;
		
		if (pad_size)
		{
			for (i = 0; i < sizeof(buffer); i++)
			{
				buffer[i] = 0xff;
			}
			while (pad_size > 0)
			{
				i = (pad_size > sizeof(buffer))? sizeof(buffer) : pad_size;
				rc = fwrite(buffer, sizeof(unsigned char ), i, outFile);
				if ( rc != i ) {
					printf("fwrite(%d) = %d ... failed\n", i, rc);
					fcloseall();
					return 5;
				}
				pad_size   -= rc;
				output_len += rc;
			}
		}
	    if(flagD)
	    {
			printf("Root = 0x%x\n", output_len);
		}
		while ((i = fread(buffer, 1, sizeof buffer, inRfs)) > 0)
		{
			rc = fwrite(buffer, sizeof(unsigned char ), i, outFile);
			if ( rc != i ) {
				printf("fwrite(%d) = %d ... failed\n", i, rc);
				fcloseall();
				return 5;
			}
			output_len += rc;
		}
		fclose(inRfs);
	}
	if (flagM)
	{
		/* Support info of this Image attached */
		printf("Support = 0x%x\n", output_len);

		while ((i = fread(buffer, 1, sizeof buffer, inModel)) > 0)
		{
			rc = fwrite(buffer, sizeof(unsigned char ), i, outFile);
			if ( rc != i ) {
				printf("fwrite(%d) = %d ... failed\n", i, rc);
				fcloseall();
				return 5;
			}
			output_len += rc;
		}
		fclose(inModel);
	}
	if (flagS)
	{
		/* Signature of this Image attached */
		printf("Signature = 0x%x\n", output_len);

		while ((i = fread(buffer, 1, sizeof buffer, inSign)) > 0)
		{
			rc = fwrite(buffer, sizeof(unsigned char ), i, outFile);
			if ( rc != i ) {
				printf("fwrite(%d) = %d ... failed\n", i, rc);
				fcloseall();
				return 5;
			}
			output_len += rc;
		}
		fclose(inSign);
	}
	if (flagT)
	{
		printf("Certificate = 0x%x\n", output_len);
		while ((i = fread(buffer, 1, sizeof buffer, inCert)) > 0)
		{
			rc = fwrite(buffer, sizeof(unsigned char ), i, outFile);
			if ( rc != i ) {
				printf("fwrite(%d) = %d ... failed\n", i, rc);
				fcloseall();
				return 5;
			}
			output_len += rc;
		}
		fclose(inCert);
	}

	
	rewind(outFile);

	/* If display of MD5 enabled */
	if(flagD)
	{
		printf("MD5 = ");
		for (i = 0; i < sizeof signature; i++)
		{
			printf("%02X", signature[i]);
		}
		printf("\n");
		printf("Size = %d\n", output_len);
	}

	fcloseall();
	return 0;
}

