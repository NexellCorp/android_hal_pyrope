/*
   1. 2ndboot

       -- first stage --

	NSIH FILE, BIN FILE
	     |
	     |  merge
		 |
    PHASE1_FILE (16KB):  [ parsed NSIH + SecondBoot.bin ]
		 |
         |  ecc gen
		 |
    PHASE2_FILE       :  [ ecc generated PHASE1_FILE    ]



       -- second stage --

	PHASE2_FILE
		 |
	     |  spacing - 0xff
		 |
    RESULT (argv[1])  :  [ spaced        PHASE2_FILE    ]
  



   2. u-boot

       -- first stage --

	NSIH FILE, BIN FILE
		 |
	     |  merge
		 |
    PHASE1_FILE       :  [ parsed NSIH(512) + 0xff(512) + u-boot.bin ]
		 |
	     |  ecc gen
		 |
    PHASE2_FILE       :  [ ecc generated PHASE1_FILE    ]
         | 
    RESULT (argv[1])

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include "NSIH.h"

#define PHASE1_FILE			"PHASE1_FILE"
#define PHASE2_FILE			"PHASE2_FILE"

#define BUFSIZE				(16 * 1024)
#define MAXPAGE				(4096)

#define DEFAULT_LOADADDR	(0x40100000)
#define DEFAULT_LAUNCHADDR	(0x40100000)


/* UTIL MACRO */
#define __ALIGN(x, a)		__ALIGN_MASK(x, (a) - 1)
#define __ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#define ALIGN(x, a)		__ALIGN((x), (a))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define IS_POWER_OF_2(n)	((n) && ((n) & ((n)-1)) == 0)

/* PRINT MACRO */
#ifdef DEBUG
#define pr_debug(msg...)	do { printf("[DEBUG] " msg); } while (0)
#else
#define pr_debug(msg...)	do {} while (0)
#endif

#define pr_error(msg...)	do { printf("[ERROR] " msg); } while (0)


//--------------------------------------------------------------------------
// BCH variables:
//--------------------------------------------------------------------------
//	k : number of information
//	m : dimension of Galois field.
//	t : number of error that can be corrected.
//	n : length of codeword = 2^m - 1
//	r : number of parity bit = m * t
//--------------------------------------------------------------------------
#define NX_BCH_SECTOR_MAX	1024
#define NX_BCH_VAR_K		(1024 * 8)
#define NX_BCH_VAR_M		(14)
#define NX_BCH_VAR_T		(60)		// 4 or 8 or 16

#define NX_BCH_VAR_N		(((1<<NX_BCH_VAR_M)-1))
#define NX_BCH_VAR_R		(NX_BCH_VAR_M * NX_BCH_VAR_T)

#define NX_BCH_VAR_TMAX		(60)
#define NX_BCH_VAR_RMAX		(NX_BCH_VAR_M * NX_BCH_VAR_TMAX)

#define NX_BCH_VAR_R32		((NX_BCH_VAR_R   +31)/32)
#define NX_BCH_VAR_RMAX32	((NX_BCH_VAR_RMAX+31)/32)

#define mm_max				14				/* Dimension of Galoise Field */
#define nn_max				32768			/* Length of codeword, n = 2**m - 1 */
#define tt_max				60				/* Number of errors that can be corrected */
#define kk_max				32768			/* Length of information bit, kk = nn - rr  */
#define rr_max				1024			/* Number of parity checks, rr = deg[g(x)] */
#define parallel_max		32				/* Number of parallel encoding/syndrome computations */


struct NSIH_INFO {
	uint32_t loadaddr;
	uint32_t launchaddr;
	char *NSIH_NAME;
};


int first_stage (const char *OUT_BINNAME, const char *IN_BINNAME, const struct NSIH_INFO *NSIH_INFO, int is_2ndboot);
int second_stage(const char *OUT_BINNAME, const char *IN_BINNAME, const int page_size);




unsigned int alpha_to[nn_max], index_of[nn_max];	// Galois field
int gg[rr_max];										// Generator polynomial
int T_G[rr_max][rr_max], T_G_R[rr_max][rr_max];		// Parallel lookahead table
int T_G_R_Temp[rr_max][rr_max];
int gen_roots[32769], gen_roots_true[32769];		// Roots of generator polynomial
int Parallel = 8;

unsigned int iSectorSize;
unsigned int iNX_BCH_VAR_K;							// (512 * 8) or (1024 * 8)
unsigned int iNX_BCH_VAR_M;							// 13 or 14
unsigned int iNX_BCH_VAR_T;							// 24 or 60
unsigned int iNX_BCH_VAR_N;
unsigned int iNX_BCH_VAR_R;

unsigned int iNX_BCH_VAR_TMAX;
unsigned int iNX_BCH_VAR_RMAX;

unsigned int iNX_BCH_VAR_R32;
unsigned int iNX_BCH_VAR_RMAX32;

unsigned int iNX_BCH_OFFSET;

//------------------------------------------------------------------------------
// matin
void generate_gf()
/* Generate GF(2**mm) from the primitive polynomial p(X) in p[0]..p[mm]
   The lookup table looks like:
   index -> polynomial form:   alpha_to[ ] contains j = alpha**i;
   polynomial form -> index form:  index_of[j = alpha**i] = i
   alpha_to[1] = 2 is the primitive element of GF(2**mm)
 */
{
	unsigned int i, mask, p;
	// Primitive polynomials
	// mm = 2^?
	if (iNX_BCH_VAR_M == 13)	p = 0x25AF;
	if (iNX_BCH_VAR_M == 14)	p = 0x41D5;

	// Galois field implementation with shift registers
	// Ref: L&C, Chapter 6.7, pp. 217
	mask = 1 ;
	alpha_to[iNX_BCH_VAR_M] = 0 ;

	for (i = 0; i < iNX_BCH_VAR_M; i++)
	{
		alpha_to[i] = mask ;
		index_of[alpha_to[i]] = i ;

		if (((p>>i)&0x1) != 0)	alpha_to[iNX_BCH_VAR_M] ^= mask ;

		mask <<= 1 ;
	}

	index_of[alpha_to[iNX_BCH_VAR_M]] = iNX_BCH_VAR_M ;
	mask >>= 1 ;

	for (i = iNX_BCH_VAR_M + 1; i < (iNX_BCH_VAR_N); i++)
	{
		if (alpha_to[i-1] >= mask)
			alpha_to[i] = alpha_to[iNX_BCH_VAR_M] ^ ((alpha_to[i-1] ^ mask) << 1) ;
		else alpha_to[i] = alpha_to[i-1] << 1 ;

		index_of[alpha_to[i]] = i ;
	}
	index_of[0] = -1 ;
}

void gen_poly()
/* Compute generator polynomial of the tt-error correcting Binary BCH code
 * g(x) = LCM{M_1(x), M_2(x), ..., M_2t(x)},
 * where M_i(x) is the minimal polynomial of alpha^i by cyclotomic cosets
 */
{
	int i, j, iii, jjj, Temp ;
	int rr, kk;

	// Initialization of gen_roots
	for (i = 0; i <= (iNX_BCH_VAR_N); i++)
	{
		gen_roots_true[i] = 0;
		gen_roots[i] = 0;
	}

	// Cyclotomic cosets of gen_roots
	for (i = 1; i <= 2*iNX_BCH_VAR_T ; i++)
	{
		for (j = 0; j < iNX_BCH_VAR_M; j++)
		{
			Temp = ((1<<j) * i) % (iNX_BCH_VAR_N);
			gen_roots_true[Temp] = 1;
		}
	}

	rr = 0;		// Count the number of parity check bits
	for (i = 0; i < (iNX_BCH_VAR_N); i++)
	{
		if (gen_roots_true[i] == 1)
		{
			rr++;
			gen_roots[rr] = i;
		}
	}
	kk = (iNX_BCH_VAR_N) - rr;

	// Compute generator polynomial based on its roots
	gg[0] = 2 ;	// g(x) = (X + alpha) initially
	gg[1] = 1 ;
	for (i = 2; i <= rr; i++)
	{
		gg[i] = 1 ;
		for (j = i - 1; j > 0; j--)
		{
			if (gg[j] != 0)
				gg[j] = gg[j-1]^ alpha_to[(index_of[gg[j]] + index_of[alpha_to[gen_roots[i]]]) % (iNX_BCH_VAR_N)] ;
			else
				gg[j] = gg[j-1] ;
		}
		gg[0] = alpha_to[(index_of[gg[0]] + index_of[alpha_to[gen_roots[i]]]) % (iNX_BCH_VAR_N)] ;
	}

	// for parallel encoding and syndrome computation
	// Max parallalism is rr
	if (Parallel > rr)
		Parallel = rr ;

	// Construct parallel lookahead matrix T_g, and T_g**r from gg(x)
	// Ref: Parallel CRC, Shieh, 2001
	for (i = 0; i < rr; i++)
		for (j = 0; j < rr; j++)
			T_G[i][j] = 0;

	for (i = 1; i < rr; i++)
		T_G[i][i-1] = 1 ;

	for (i = 0; i < rr; i++)
		T_G[i][rr - 1] = gg[i] ;

	for (i = 0; i < rr; i++)
		for (j = 0; j < rr; j++)
			T_G_R[i][j] = T_G[i][j];

	// Compute T_G**R Matrix
	for (iii = 1; iii < Parallel; iii++)
	{
		for (i = 0; i < rr; i++)
			for (j = 0; j < rr; j++)
			{
				Temp = 0;
				for (jjj = 0; jjj < rr; jjj++)
					Temp = Temp ^ T_G_R[i][jjj] * T_G[jjj][j];

				T_G_R_Temp[i][j] = Temp;
			}

		for (i = 0; i < rr; i++)
			for (j = 0; j < rr; j++)
				T_G_R[i][j] = T_G_R_Temp[i][j];
	}
}

void parallel_encode_bch( unsigned int *pData32, unsigned int *pECC )
/* Parallel computation of n - k parity check bits.
 * Use lookahead matrix T_G_R.
 * The incoming streams are fed into registers from the right hand
 */
{
	unsigned int i, j, iii, Temp, bb_temp[rr_max] ;
	unsigned int loop_count ;
	unsigned int bb[rr_max/32];

	// Determine the number of loops required for parallelism.
	loop_count = iSectorSize;

	// Initialize the parity bits.
	for (i = 0; i < rr_max/32; i++)
		bb[i] = 0;

	// Compute parity checks
	// S(t) = T_G_R [ S(t-1) + M(t) ]
	// Ref: Parallel CRC, Shieh, 2001

	for( iii = 0; iii<loop_count; iii++ )
	{
		for (i = 0; i < iNX_BCH_VAR_R; i++)
			bb_temp[i] = (bb[i>>5]>>(i&0x1F)) & 0x00000001 ;

		for( i=0; i<Parallel; i++ ) // 8bit
			bb_temp[iNX_BCH_VAR_R - Parallel + i] ^= (pData32[iii>>2]>>(i+(iii<<3))) & 0x00000001;

		for (i = 0; i < iNX_BCH_VAR_R; i++)
		{
			Temp = 0;
			for (j = 0; j < iNX_BCH_VAR_R; j++)
				Temp ^= (bb_temp[j] * T_G_R[i][j]);

			if(Temp)
				bb[i>>5] |=  Temp<<(i&0x1F);
			else
				bb[i>>5] &=  ~(1<<(i&0x1F));
		}
	}

	iii = ((iNX_BCH_VAR_R+(32-1)) >>5);

	for(i=0; i<iii; i++)
	{
		Temp = 0;
		for(j=0; j<32; j++)
		{
			Temp |= ((bb[i]>>(31-j))&0x00000001)<<j;	// reverse each bit every 32 bit
		}
		bb[i] = Temp;
	}

	for(i=0; i<iii; i++)
	{
		Temp = 0;
		for(j=0; j<32; j+=8)
		{
			Temp |= ((bb[i]>>(24-j))&0x000000FF)<<j;	// reverse endian
		}
		bb[i]= Temp;
	}

	for (j = 0; j < iii; j++)
	{
		*pECC++ = bb[j];
	}
}

int	MakeECCChunk (unsigned int *pdwSrcAddr, unsigned int *pdwDstAddr, int SrcSize)
{
	int iSector, iSecCount;

	iSecCount = (SrcSize + iSectorSize-1) / iSectorSize;

	if (iSecCount > 7)	// only use 8 pages per block
	{
		printf ("MakeECCChuck : Error - size(%d) must be less than or equal to %d\n",
				SrcSize, 7*iSectorSize );	// only use 8 pages per block
		return 0;
	}

	memset ((void *)pdwDstAddr, 0, iSectorSize*8);	// only use 8 pages per block

	for (iSector=0 ; iSector < iSecCount ; iSector++)
	{
		memcpy ((void *)(pdwDstAddr+((iSector+1)*(iSectorSize/4))), (const void *)(pdwSrcAddr+(iSector*(iSectorSize/4))), iSectorSize);
		parallel_encode_bch ((pdwDstAddr+((iSector+1)*(iSectorSize/4))), pdwDstAddr+((iSector+1)*(iSectorSize/8/4)));

		printf (".");
	}

	printf ("\n");
	return (iSecCount+1) * iSectorSize;
}

//------------------------------------------------------------------------------
unsigned int String2Hex ( const char *pstr )
{
	char ch;
	unsigned int value;

	value = 0;

	while ( *pstr != '\0' )
	{
		ch = *pstr++;

		if ( ch >= '0' && ch <= '9' )
		{
			value = value * 16 + ch - '0';
		}
		else if ( ch >= 'a' && ch <= 'f' )
		{
			value = value * 16 + ch - 'a' + 10;
		}
		else if ( ch >= 'A' && ch <= 'F' )
		{
			value = value * 16 + ch - 'A' + 10;
		}
	}

	return value;
}

unsigned int String2Dec ( const char *pstr )
{
	char ch;
	unsigned int value;

	value = 0;

	while ( *pstr != '\0' )
	{
		ch = *pstr++;

		if ( ch >= '0' && ch <= '9' )
		{
			value = value * 10 + ch - '0';
		}
	}

	return value;
}

void Hex2StringByte(unsigned char data, unsigned char *string)
{
	unsigned char hex;

	hex = data >> 4;
	if (hex >= 10)
		hex += 'A' - 10;
	else
		hex += '0';
	*string++ = hex;

	hex = data & 0xF;
	if (hex >= 10)
		hex += 'A' - 10;
	else
		hex += '0';
	*string++ = hex;

	*string = ' ';
}

int ProcessNSIH (const char *pfilename, unsigned char *pOutData)
{
	FILE *fp;
	char ch;
	int writesize, skipline, line, bytesize, i;
	unsigned int writeval;

	fp = fopen (pfilename, "rb");
	if (!fp)
	{
		printf ("ProcessNSIH : ERROR - Failed to open %s file.\n", pfilename);
		return 0;
	}

	bytesize = 0;
	writeval = 0;
	writesize = 0;
	skipline = 0;
	line = 0;

	while (0 == feof (fp))
	{
		ch = fgetc (fp);

		if (skipline == 0)
		{
			if (ch >= '0' && ch <= '9')
			{
				writeval = writeval * 16 + ch - '0';
				writesize += 4;
			}
			else if (ch >= 'a' && ch <= 'f')
			{
				writeval = writeval * 16 + ch - 'a' + 10;
				writesize += 4;
			}
			else if (ch >= 'A' && ch <= 'F')
			{
				writeval = writeval * 16 + ch - 'A' + 10;
				writesize += 4;
			}
			else
			{
				if (writesize == 8 || writesize == 16 || writesize == 32)
				{
					for (i=0 ; i<writesize/8 ; i++)
					{
						pOutData[bytesize++] = (unsigned char)(writeval & 0xFF);
						writeval >>= 8;
					}
				}
				else
				{
					if (writesize != 0)
						printf ("ProcessNSIH : Error at %d line.\n", line+1);
				}

				writesize = 0;
				skipline = 1;
			}
		}

		if (ch == '\n')
		{
			line++;
			skipline = 0;
			writeval = 0;
		}
	}

	printf ("ProcessNSIH : %d line processed.\n", line+1);
	printf ("ProcessNSIH : %d bytes generated.\n", bytesize);

	fclose (fp);

	return bytesize;
}

void print_usage(char *argv[])
{
	printf("\n");
	printf("usage: options\n");
	printf("-h help                                                      \n");
	printf("-t 2ndboot|bootloader                                        \n");
	printf("-o output file name                                          \n");
	printf("-i input file name                                           \n");
	printf("-n nsih file name                                            \n");
	printf("-p page size  (KiB)   , default %d                           \n", MAXPAGE);
	printf("-l load address       , default %x                           \n", DEFAULT_LOADADDR);
	printf("-e launch address     , default %x                           \n", DEFAULT_LAUNCHADDR);
	printf("\nExample:\n");
	printf("%s -t 2ndboot -o nand_2ndboot.bin -i pyrope_2ndboot_NAND.bin -n NSIH.txt -p 4096 -l 0x40100000 -e 0x40100000 \n", argv[0]);
	printf("%s -t bootloader -o nand_bootloader.bin -i u-boot.bin -n NSIH.txt -p 4096 -l 0x40100000 -e 0x40100000 \n", argv[0]);
	printf("\n");
}

static size_t get_file_size( FILE *fd )
{
	size_t fileSize;
	long curPos;

	curPos = ftell( fd );

	fseek(fd, 0, SEEK_END);
	fileSize = ftell( fd );
	fseek(fd, curPos, SEEK_SET );

	return fileSize;
}


























//------------------------------------------------------------------------------

#define BUILD_NONE					0x0
#define BUILD_2NDBOOT				0x1
#define BUILD_BOOTLOADER			0x2

int main (int argc, char** argv)
{
	int page_size = MAXPAGE;
	uint32_t sector_size = 0;

	struct NSIH_INFO NSIH_INFO;
	uint32_t loadaddr = DEFAULT_LOADADDR;
	uint32_t launchaddr = DEFAULT_LAUNCHADDR;

	char *bin_type = NULL;
	char *out_file = NULL;
	char *in_file = NULL;
	char *nsih_file = NULL;
	int build_type = 0;

	int opt;


	// arguments parsing 
	while (-1 != (opt = getopt(argc, argv, "ht:o:i:n:p:l:e:"))) {
		switch(opt) {
			case 't':
				bin_type = optarg;
				break;
			case 'o':
				out_file = optarg;
				break;
			case 'i':
				in_file = optarg;
				break;
			case 'n':
				nsih_file = optarg;
				break;
			case 'p':
				page_size = strtoul (optarg, NULL, 10);
				break;
			case 'l':
				loadaddr = strtoul (optarg, NULL, 16);
				break;
			case 'e':
				launchaddr = strtoul (optarg, NULL, 16);
				break;
			case 'h':
			default:
				print_usage(argv);
				exit(0);
				break;
		}
	}


	// argument check
	if (!bin_type || !out_file || !in_file || !nsih_file)
	{
		print_usage(argv);
		return -1;
	}

	if (page_size < 512 || !IS_POWER_OF_2 (page_size))
	{
		pr_error ("Page size must bigger than 512 and must power-of-2\n");
		return -1;
	}


	if (!strcmp(bin_type, "2ndboot"))
		build_type = BUILD_2NDBOOT;
	else if(!strcmp(bin_type, "bootloader"))
		build_type = BUILD_BOOTLOADER;
	pr_debug ("build type : %d\n", build_type);

	if (build_type == BUILD_NONE)
	{
		pr_error ("Enter 2ndboot or bootloader\n");
		return -1;
	}

	if (!strcmp(out_file, PHASE1_FILE) || !strcmp(out_file, PHASE2_FILE)
		|| !strcmp(in_file, PHASE1_FILE) || !strcmp(in_file, PHASE2_FILE)
		|| !strcmp(nsih_file, PHASE1_FILE) || !strcmp(nsih_file, PHASE2_FILE))
	{
		pr_error ("Do not using %s or %s as file name.\n", PHASE1_FILE, PHASE2_FILE);
		return -1;
	}



	// get sector size
	sector_size = (page_size < 1024) ? 512 : 1024;
	if (sector_size == 512)
	{
		iSectorSize			=	512;
		iNX_BCH_VAR_T		=	24;
		iNX_BCH_VAR_M		=	13;
	}
	else if (sector_size == 1024)
	{
		iSectorSize			=	1024;
		iNX_BCH_VAR_T		=	60;
		iNX_BCH_VAR_M		=	14;
	}
	pr_debug("page size: %d, sector size: %d\n", page_size, sector_size);



	NSIH_INFO.loadaddr = loadaddr;
	NSIH_INFO.launchaddr = launchaddr;
	NSIH_INFO.NSIH_NAME = nsih_file;

	//  make image
	if (build_type == BUILD_2NDBOOT)
	{
		first_stage  (PHASE2_FILE, in_file, &NSIH_INFO, 1);
		second_stage (out_file, PHASE2_FILE, page_size);
	}
	else	// BUILD_BOOTLOADER
	{
		first_stage  (out_file, in_file, &NSIH_INFO, 0);
	}

	return 0;
}


int first_stage (const char *OUT_BINNAME, const char *IN_BINNAME, const struct NSIH_INFO *NSIH_INFO, int is_2ndboot)
{
	FILE	*bin_fp;
	FILE	*phase1_fp, *phase2_fp;
	unsigned int BinFileSize, SrcLeft, SrcReadSize, DstSize, DstTotalSize;
	static unsigned int pdwSrcData[7*NX_BCH_SECTOR_MAX/4], pdwDstData[8*NX_BCH_SECTOR_MAX/4];

	unsigned char *outbuf = NULL;
	struct NX_SecondBootInfo *bootinfo;
	int outbuf_sz = 0;
	int bin_offset = iSectorSize;

	int ret = 0;


	bin_fp = fopen (IN_BINNAME, "rb");
	if (!bin_fp)
	{
		printf ("ERROR : Failed to open %s file.\n", IN_BINNAME);
		return -1;
	}

	BinFileSize = get_file_size (bin_fp);
	pr_debug ("BinFileSize 1: %d\n", BinFileSize);


	// calulation buffer size
	outbuf_sz = ALIGN(BinFileSize, BUFSIZE);
	pr_debug ("outbuf_sz: %d\n", outbuf_sz);

	if (is_2ndboot && (iSectorSize + BinFileSize) > outbuf_sz)
	{
		pr_error ("BIN is out of range.\n");
		fclose (bin_fp);
		return -1;
	}

	// buffer allocation
	outbuf = malloc(outbuf_sz);
	if (!outbuf)
	{
		pr_error ("Not enough memory.");
		fclose (bin_fp);
		return -1;
	}
	memset (outbuf, 0xff, outbuf_sz);


	//--------------------------------------------------------------------------
	// NSIH parsing
	ret = ProcessNSIH (NSIH_INFO->NSIH_NAME, outbuf);
	if (ret != 512)
	{
		pr_error ("NSIH Parsing failed.\n");
		fclose (bin_fp);
		return -1;
	}

	bootinfo = (struct NX_SecondBootInfo *)outbuf;
	bootinfo->LOADSIZE = BinFileSize;
	bootinfo->LOADADDR = NSIH_INFO->loadaddr;
	bootinfo->LAUNCHADDR = NSIH_INFO->launchaddr;


	//--------------------------------------------------------------------------
	// BIN processing
	if (is_2ndboot)
		bin_offset = 512;
	else
		bin_offset = 1024;

	fread (outbuf + bin_offset, 1, BinFileSize, bin_fp);
	fclose (bin_fp);


	pr_debug ("bootinfo sz: %d loadaddr: 0x%x, launchaddr 0x%x\n",
		bootinfo->LOADSIZE, bootinfo->LOADADDR, bootinfo->LAUNCHADDR);


	phase1_fp = fopen (PHASE1_FILE, "wb");
	if (!phase1_fp)
	{
		pr_error ("failed to create %s file.\n", PHASE1_FILE);
		return -1;
	}
	fwrite (outbuf, 1, outbuf_sz, phase1_fp);

	fclose (phase1_fp);


	phase1_fp = fopen (PHASE1_FILE, "rb");

	BinFileSize = get_file_size (phase1_fp);
	pr_debug ("BinFileSize 2: %d\n", BinFileSize);



	if (is_2ndboot && BinFileSize > BUFSIZE)
	{
		printf ("WARNING : %s is too big for Second boot image, only will used first %d bytes.\n", IN_BINNAME, BUFSIZE);
		BinFileSize = BUFSIZE;
	}

	pr_debug ("iSectorSize: %d\n", iSectorSize);

	iNX_BCH_VAR_K		=	(iSectorSize * 8);

	iNX_BCH_VAR_N		=	(((1<<iNX_BCH_VAR_M)-1));
	iNX_BCH_VAR_R		=	(iNX_BCH_VAR_M * iNX_BCH_VAR_T);

	iNX_BCH_VAR_TMAX	=	(60);
	iNX_BCH_VAR_RMAX	=	(iNX_BCH_VAR_M * iNX_BCH_VAR_TMAX);

	iNX_BCH_VAR_R32		=	((iNX_BCH_VAR_R   +31)/32);
	iNX_BCH_VAR_RMAX32	=	((iNX_BCH_VAR_RMAX+31)/32);

	iNX_BCH_OFFSET		= 8;

	SrcLeft = BinFileSize;

	//--------------------------------------------------------------------------
	// Encoding - ECC Generation
	printf ("\n");

	DstTotalSize = 0;

	// generate the Galois Field GF(2**mm)
	generate_gf ();
	// Compute the generator polynomial and lookahead matrix for BCH code
	gen_poly ();



	phase2_fp = fopen (OUT_BINNAME, "wb");
	if (!phase2_fp)
	{
		printf ("ERROR : Failed to create %s file.\n", OUT_BINNAME);
		fclose (phase1_fp);
		return -1;
	}

	while (SrcLeft > 0)
	{
		SrcReadSize = (SrcLeft > (iSectorSize*7)) ? (iSectorSize*7) : SrcLeft;
		fread (pdwSrcData, 1, SrcReadSize, phase1_fp);
		SrcLeft -= SrcReadSize;
		pr_debug ("SrcLeft: %x, SrcReadSize: %x\n", SrcLeft, SrcReadSize);
		DstSize = MakeECCChunk (pdwSrcData, pdwDstData, SrcReadSize);
		fwrite (pdwDstData, 1, DstSize, phase2_fp);
		
		DstTotalSize += DstSize;
	}

	//--------------------------------------------------------------------------
	printf ("\n");
	printf ("%d bytes(%d sector) generated.\n", DstTotalSize, (DstTotalSize+iSectorSize-1)/iSectorSize);

	fclose (phase1_fp);
	fclose (phase2_fp);


	return 0;
}


int second_stage(const char *OUT_BINNAME, const char *IN_BINNAME, const int page_size)
{
	FILE *in_fp;
	FILE *out_fp;

	char *data = NULL;

	int data_in_page = page_size;
	int BinFileSize;
	int Cnt, i;
	int ret = 0;


	// buffer allocation
	data = malloc (page_size);
	if (!data)
	{
		ret = -10;
		goto err;
	}


	in_fp = fopen (IN_BINNAME, "rb");
	if (!in_fp) {
		ret = -20;
		goto err_in_fp;
	}
	out_fp = fopen (OUT_BINNAME, "wb");
	if (!out_fp) {
		ret = -30;
		goto err_out_fp;
	}


	// size calc.
	BinFileSize = get_file_size (in_fp);
	pr_debug ("BinFileSize 3: %d\n", BinFileSize);

	data_in_page = MIN (data_in_page, MAXPAGE);
	Cnt = DIV_ROUND_UP (BinFileSize, data_in_page);
	pr_debug ("Cnt: %d\n", Cnt);


	// file writing
	for (i = 0; i < Cnt; i++)
	{
		memset (data, 0xff, page_size);

		fread (data, 1, data_in_page, in_fp);
		fwrite (data, 1, page_size, out_fp);
	}

	fclose (in_fp);
	fclose (out_fp);

	return 0;

err_out_fp:
	fclose (out_fp);
err_in_fp:
	printf ("err:%d\n", ret);
err:
	return ret;
}
