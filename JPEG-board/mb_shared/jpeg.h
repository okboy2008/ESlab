/* File : jpeg.h, header for all jpeg code */
/* Author: Pierre Guerrier, march 1998     */
/*                                         */
/* 19/01/99  Edited by Koen van Eijk       */

/* Leave structures in memory,output something and dump core in the event
   of a failure: */
#ifndef JPEG_H
#define JPEG_H

#define DEBUG 0

/*----------------------------------*/
/* JPEG format parsing markers here */
/*----------------------------------*/

#define SOI_MK	0xFFD8		/* start of image       */
#define APP_MK	0xFFE0		/* custom, up to FFEF */
#define COM_MK	0xFFFE		/* commment segment     */
#define SOF_MK	0xFFC0		/* start of frame       */
#define SOS_MK	0xFFDA		/* start of scan        */
#define DHT_MK	0xFFC4		/* Huffman table        */
#define DQT_MK	0xFFDB		/* Quant. table         */
#define DRI_MK	0xFFDD		/* restart interval     */
#define EOI_MK	0xFFD9		/* end of image         */
#define MK_MSK	0xFFF0

/* is x a restart interval ? */
#define RST_MK(x)	( (0xFFF8&(x)) == 0xFFD0 )

/*-------------------------------------------------------- */
/* all kinds of macros here				*/
/*-------------------------------------------------------- */

#define first_quad(c)   ((c) >> 4)	/* first 4 bits in file order */
#define second_quad(c)  ((c) & 15)

#define HUFF_ID(hclass, id)       (2 * (hclass) + (id))

#define DC_CLASS        0
#define AC_CLASS        1


typedef  union {
	struct {
		unsigned char B;
		unsigned char G;
		unsigned char R;
		unsigned char A;
	};
	int value;
} pixel;
/*-------------------------------------------------------*/
/* JPEG data types here					*/
/*-------------------------------------------------------*/

typedef union {			/* block of pixel-space values */
	unsigned char block[8][8];
	unsigned char linear[64];
} PBlock;

typedef union {			/* block of frequency-space values */
	int block[8][8];
	int linear[64];
} FBlock;

/* component descriptor structure */

typedef struct {
	unsigned char CID;	/* component ID */
	unsigned char IDX;	/* index of first block in MCU */

	unsigned char HS;	/* sampling factors */
	unsigned char VS;
	unsigned char HDIV;	/* sample width ratios */
	unsigned char VDIV;

	char QT;		/* QTable index, 2bits  */
	char DC_HT;		/* DC table index, 1bit */
	char AC_HT;		/* AC table index, 1bit */
	int PRED;		/* DC predictor value */
} cd_t;

/* Set a 256 kbyte maximum input size. */
#define JPGBUFFER_SIZE      0x40000

/*--------------------------------------------*/
/* global variables here		      */
/*--------------------------------------------*/

/* for every component, useful stuff */
extern cd_t comp[3];
/* decoded component buffer */
extern PBlock *MCU_buff[10];

/* between IDCT and color convert */
/* for every DCT block, component id then -1 */
extern int MCU_valid[10];
/* three quantization tables */
extern PBlock *QTable[4];
/* at most, but seen as four ... */
extern int QTvalid[4];


/* picture attributes */
/* Video frame size     */
extern int x_size, y_size;

/* down-rounded Video frame size */
/* in pixel units, multiple of MCU */
extern int rx_size, ry_size;

/* MCU size in pixels   */
extern int MCU_sx, MCU_sy;
/* picture size in units of MCUs */
extern int mx_size, my_size;

/* number of components 1,3 */
extern int n_comp;

/* processing cursor variables */
extern int in_frame, curcomp, MCU_row, MCU_column;
/* current position in MCU unit */

/* RGB buffer storage */
/* MCU after color conversion */
extern unsigned char *ColorBuffer;
/* complete final RGB image */
extern unsigned char *FrameBuffer;
extern PBlock *PBuff;
extern FBlock *FBuff;

/* process statistics */
/* number of stuff bytes in file */
extern int stuffers;
/* number of bytes skipped looking for markers */
extern int passed;

extern int verbose;

/* Counter used by FGET and FSEEK in 5kk03.c */
extern int vld_count;
extern unsigned int input_buffer[JPGBUFFER_SIZE / sizeof(int)];

///////////////////////////////////////////////////////////////////////////////////////
//	main
///////////////////////////////////////////////////////////////////////////////////////
char GETCHAR(char** fi,int* counter){
	int* input=(int*)(*fi-*counter);
	unsigned int c = ((input[(*counter) / 4] << (8 * ((*counter) % 4))) >> 24) & 0x00ff;
	(*counter)++;
	(*fi)++;
	return c;
}

char getChar(char** fi, int* counter){
		unsigned char c = (*(*fi));
		(*counter)++;
		(*fi)++;
		return c;
}

// memmove.h
void memmove ( void * destination, void * source, size_t num ){
	int i;
	for(i=0;i<num;i++){
		*((char*)(destination+i))=*((char*)(source+i));
	}
}
//////////////////////////////////////////////////////////////////
 #define EOF -1
 
/*-----------------------------------------*/
/* File : utils.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	   */
/*-----------------------------------------*/

// #include <stdlib.h>
// #include <stdio.h>

// #include "jpeg.h"

/* Prints a data block in frequency space. */
// void show_FBlock(FBlock * S)
// {
	// int i, j;

	// for (i = 0; i < 8; i++) {
		// for (j = 0; j < 8; j++)
			// fprintf(stderr, "\t%d", S->block[i][j]);
		// fprintf(stderr, "\n");
	// }
// }

// /* Prints a data block in pixel space. */
// void show_PBlock(PBlock * S)
// {
	// int i, j;

	// for (i = 0; i < 8; i++) {
		// for (j = 0; j < 8; j++)
			// fprintf(stderr, "\t%d", S->block[i][j]);
		// fprintf(stderr, "\n");
	// }
// }

/* Prints the next 800 bits read from file `fi'. */


/*-------------------------------------------*/
/* core dump generator for forensic analysis */
/*-------------------------------------------*/

// void suicide(void)
// {
	// int *P;

	// fflush(stdout);
	// fflush(stderr);
	// P = NULL;
	// *P = 1;
// }

/*-------------------------------------------*/

/*----------------------------------------------------------*/

/* Returns ceil(N/D). */
int ceil_div(int N, int D)
{
	int i = N / D;

	if (N > D * i)
		i++;
	return i;
}

/* Returns floor(N/D). */
int floor_div(int N, int D)
{
	int i = N / D;

	if (N < D * i)
		i--;
	return i;
}

/*----------------------------------------------------------*/

/* For all components reset DC prediction value to 0. */
void reset_prediction(void)
{
	int i;

	for (i = 0; i < 3; i++)
		comp[i].PRED = 0;
}

/*---------------------------------------------------------*/

/* Transform JPEG number format into usual 2's-complement format. */
int reformat(unsigned long S, int good)
{
	int St;

	if (!good)
		return 0;
	St = 1 << (good - 1);	/* 2^(good-1) */
	if (S < (unsigned long)St)
		return (S + 1 + ((-1) << good));
	else
		return S;
}

/*----------------------------------------------------------*/

void free_structures(void)
{
	int i;

	for (i = 0; i < 4; i++)
		if (QTvalid[i])
			mk_free(QTable[i]);

	if (ColorBuffer != NULL)
		mk_free(ColorBuffer);
	if (FrameBuffer != NULL)
		mk_free(FrameBuffer);
	if (PBuff != NULL)
		mk_free(PBuff);
	if (FBuff != NULL)
		mk_free(FBuff);

	for (i = 0; MCU_valid[i] != -1; i++)
		mk_free(MCU_buff[i]);
}

/*-------------------------------------------*/
/* this is to save final RGB image to disk   */
/* using the sunraster uncompressed format   */
/*-------------------------------------------*/

/* Sun raster header */

typedef struct {
	unsigned long MAGIC;
	unsigned long Width;
	unsigned long Heigth;
	unsigned long Depth;
	unsigned long Length;
	unsigned long Type;
	unsigned long CMapType;
	unsigned long CMapLength;
} sunraster;

///////////////////////////////////////////////////////////////////////////////////////
//	end
///////////////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------*/
/* File : color.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	   */
/*-----------------------------------------*/

// #include <stdlib.h>
// #include <stdio.h>

// #include "jpeg.h"

/* Ensure number is >=0 and <=255			   */
#define Saturate(n)	((n) > 0 ? ((n) < 255 ? (n) : 255) : 0)

/*---------------------------------------*/
/* rules for color conversion:	         */
/*  r = y		+1.402	v	 */
/*  g = y -0.34414u	-0.71414v	 */
/*  b = y +1.772  u			 */
/* Approximations: 1.402 # 7/5 = 1.400	 */
/*		.71414 # 357/500 = 0.714 */
/*		.34414 # 43/125	= 0.344	 */
/*		1.772  = 443/250	 */
/*---------------------------------------*/
/* Approximations: 1.402 # 359/256 = 1.40234 */
/*		.71414 # 183/256 = 0.71484 */
/*		.34414 # 11/32 = 0.34375 */
/*		1.772 # 227/128 = 1.7734 */
/*----------------------------------*/

void color_conversion(void)
{
	int i, j;
	unsigned char y, cb, cr;
	signed char rcb, rcr;
	long r, g, b;
	long offset;

	for (i = 0; i < MCU_sy; i++) {	/* pixel rows */
		int ip_0 = i >> comp[0].VDIV;
		int ip_1 = i >> comp[1].VDIV;
		int ip_2 = i >> comp[2].VDIV;
		int inv_ndx_0 = comp[0].IDX + comp[0].HS * (ip_0 >> 3);
		int inv_ndx_1 = comp[1].IDX + comp[1].HS * (ip_1 >> 3);
		int inv_ndx_2 = comp[2].IDX + comp[2].HS * (ip_2 >> 3);
		int ip_0_lsbs = ip_0 & 7;
		int ip_1_lsbs = ip_1 & 7;
		int ip_2_lsbs = ip_2 & 7;
		int i_times_MCU_sx = i * MCU_sx;

		for (j = 0; j < MCU_sx; j++) {	/* pixel columns */
			int jp_0 = j >> comp[0].HDIV;
			int jp_1 = j >> comp[1].HDIV;
			int jp_2 = j >> comp[2].HDIV;

			y = MCU_buff[inv_ndx_0 + (jp_0 >> 3)]->block[ip_0_lsbs][jp_0 & 7];
			cb = MCU_buff[inv_ndx_1 + (jp_1 >> 3)]->block[ip_1_lsbs][jp_1 & 7];
			cr = MCU_buff[inv_ndx_2 + (jp_2 >> 3)]->block[ip_2_lsbs][jp_2 & 7];

			rcb = cb - 128;
			rcr = cr - 128;

			r = y + ((359 * rcr) >> 8);
			g = y - ((11 * rcb) >> 5) - ((183 * rcr) >> 8);
			b = y + ((227 * rcb) >> 7);

			offset = 3 * (i_times_MCU_sx + j);
			ColorBuffer[offset + 2] = Saturate(r);
			ColorBuffer[offset + 1] = Saturate(g);
			ColorBuffer[offset + 0] = Saturate(b);
			/* note that this is SunRaster color ordering */
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//	end of color.c
////////////////////////////////////////////////////////////////////////////////////////

/*---------------------------------------------*/
/* File : fast_idct.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	       */
/* IDCT code by Geert Janssen	     	       */
/*---------------------------------------------*/

// #include <stdio.h>
// #include <string.h>
// #include <stdlib.h>
// #include <math.h>
#include <limits.h>

// #include "jpeg.h"

#define Y(i,j)		Y[8*i+j]
#define X(i,j)		(output->block[i][j])

/* This version is IEEE compliant using 16-bit arithmetic. */

/* The number of bits coefficients are scaled up before 2-D IDCT: */
#define S_BITS	         3
/* The number of bits in the fractional part of a fixed point constant: */
#define C_BITS		14

#define SCALE(x,n)	((x) << (n))

/* This version is vital in passing overall mean error test. */
#define DESCALE(x, n)	(((x) + (1 << ((n)-1)) - ((x) < 0)) >> (n))

#define ADD(x, y)	((x) + (y))
#define SUB(x, y)	((x) - (y))
#define CMUL(C, x)	(((C) * (x) + (1 << (C_BITS-1))) >> C_BITS)

/* Butterfly: but(a,b,x,y) = rot(sqrt(2),4,a,b,x,y) */
#define but(a,b,x,y)	{ x = SUB(a,b); y = ADD(a,b); }

/* Inverse 1-D Discrete Cosine Transform.
   Result Y is scaled up by factor sqrt(8).
   Original Loeffler algorithm.
*/
static void idct_1d(int *Y)
{
	int z1[8], z2[8], z3[8];

	/* Stage 1: */
	but(Y[0], Y[4], z1[1], z1[0]);
	/* rot(sqrt(2), 6, Y[2], Y[6], &z1[2], &z1[3]); */
	z1[2] = SUB(CMUL(8867, Y[2]), CMUL(21407, Y[6]));
	z1[3] = ADD(CMUL(21407, Y[2]), CMUL(8867, Y[6]));
	but(Y[1], Y[7], z1[4], z1[7]);
	/* z1[5] = CMUL(sqrt(2), Y[3]);
	   z1[6] = CMUL(sqrt(2), Y[5]);
	 */
	z1[5] = CMUL(23170, Y[3]);
	z1[6] = CMUL(23170, Y[5]);

	/* Stage 2: */
	but(z1[0], z1[3], z2[3], z2[0]);
	but(z1[1], z1[2], z2[2], z2[1]);
	but(z1[4], z1[6], z2[6], z2[4]);
	but(z1[7], z1[5], z2[5], z2[7]);

	/* Stage 3: */
	z3[0] = z2[0];
	z3[1] = z2[1];
	z3[2] = z2[2];
	z3[3] = z2[3];
	/* rot(1, 3, z2[4], z2[7], &z3[4], &z3[7]); */
	z3[4] = SUB(CMUL(13623, z2[4]), CMUL(9102, z2[7]));
	z3[7] = ADD(CMUL(9102, z2[4]), CMUL(13623, z2[7]));
	/* rot(1, 1, z2[5], z2[6], &z3[5], &z3[6]); */
	z3[5] = SUB(CMUL(16069, z2[5]), CMUL(3196, z2[6]));
	z3[6] = ADD(CMUL(3196, z2[5]), CMUL(16069, z2[6]));

	/* Final stage 4: */
	but(z3[0], z3[7], Y[7], Y[0]);
	but(z3[1], z3[6], Y[6], Y[1]);
	but(z3[2], z3[5], Y[5], Y[2]);
	but(z3[3], z3[4], Y[4], Y[3]);
}

/* Inverse 2-D Discrete Cosine Transform. */
void IDCT(const FBlock * input, PBlock * output)
{
	int Y[64];
	int k, l;

	/* Pass 1: process rows. */
	for (k = 0; k < 8; k++) {

		/* Prescale k-th row: */
		for (l = 0; l < 8; l++)
			Y(k, l) = SCALE(input->block[k][l], S_BITS);

		/* 1-D IDCT on k-th row: */
		idct_1d(&Y(k, 0));
		/* Result Y is scaled up by factor sqrt(8)*2^S_BITS. */
	}

	/* Pass 2: process columns. */
	for (l = 0; l < 8; l++) {
		int Yc[8];

		for (k = 0; k < 8; k++)
			Yc[k] = Y(k, l);
		/* 1-D IDCT on l-th column: */
		idct_1d(Yc);
		/* Result is once more scaled up by a factor sqrt(8). */
		for (k = 0; k < 8; k++) {
			int r = 128 + DESCALE(Yc[k], S_BITS + 3);	/* includes level shift */

			/* Clip to 8 bits unsigned: */
			r = r > 0 ? (r < 255 ? r : 255) : 0;
			X(k, l) = r;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////
//	end
///////////////////////////////////////////////////////////////////////////////////////



/*-----------------------------------------*/
/* File : parse.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	   */
/*-----------------------------------------*/

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #include "jpeg.h"

/*---------------------------------------------------------------------*/

/* utility and counter to return the number of bits from file */
/* right aligned, masked, first bit towards MSB's		*/

static unsigned char bit_count;	/* available bits in the window */
static unsigned char window;


void clear_bits(void)
{
	bit_count = 0;
}

/*----------------------------------------------------------*/
/* initialise MCU block descriptors	                    */
/*----------------------------------------------------------*/

int init_MCU(void)
{
	int i, j, k, n, hmax = 0, vmax = 0;

	for (i = 0; i < 10; i++)
		MCU_valid[i] = -1;

	k = 0;

	for (i = 0; i < n_comp; i++) {
		if (comp[i].HS > hmax)
			hmax = comp[i].HS;
		if (comp[i].VS > vmax)
			vmax = comp[i].VS;
		n = comp[i].HS * comp[i].VS;

		comp[i].IDX = k;
		for (j = 0; j < n; j++) {
			MCU_valid[k] = i;
			MCU_buff[k] = (PBlock *) mk_malloc(sizeof(PBlock));
			if (MCU_buff[k] == NULL) {
				// fprintf(stderr, "\tERROR:\tCould not allocate MCU buffers!\n");
				exit(1);
			}
			k++;
			if (k == 10) {
				// fprintf(stderr, "\tERROR:\tMax subsampling exceeded!\n");
				return -1;
			}
		}
	}

	MCU_sx = 8 * hmax;
	MCU_sy = 8 * vmax;
	for (i = 0; i < n_comp; i++) {
		comp[i].HDIV = (hmax / comp[i].HS > 1);	/* if 1 shift by 0 */
		comp[i].VDIV = (vmax / comp[i].VS > 1);	/* if 2 shift by one */
	}

	mx_size = ceil_div(x_size, MCU_sx);
	my_size = ceil_div(y_size, MCU_sy);
	rx_size = MCU_sx * floor_div(x_size, MCU_sx);
	ry_size = MCU_sy * floor_div(y_size, MCU_sy);

	return 0;
}

/*----------------------------------------------------------*/
/* this takes care for processing all the blocks in one MCU */
/*----------------------------------------------------------*/

unsigned char get_one_bit2(char ** fi, int * counter)
{
	int newbit;
	unsigned char aux, wwindow;

	if (bit_count == 0) {
		wwindow = GETCHAR(fi,counter);

		if (wwindow == 0xFF)
			switch (aux = GETCHAR(fi,counter)) {	/* skip stuffer 0 byte */
			case 0xFF:
				// fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", *counter);
				return -1;
				break;

			case 0x00:
				stuffers++;
				break;

			default:
				if (RST_MK(0xFF00 | aux))
					;
					// fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n", *counter);
				// fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", *counter);
				return -1;
				break;
			}

		bit_count = 8;
	} else
		wwindow = window;

	newbit = (wwindow >> 7) & 1;
	window = wwindow << 1;
	bit_count--;
	return newbit;
}

unsigned long get_bits2(char ** fi, int * counter, int number)
{
	int i, newbit;
	unsigned long result = 0;
	unsigned char aux, wwindow;

	if (!number)
		return 0;

	for (i = 0; i < number; i++) {
		if (bit_count == 0) {
			wwindow = GETCHAR(fi,counter);

			if (wwindow == 0xFF)
				switch (aux = GETCHAR(fi,counter)) {	/* skip stuffer 0 byte */
				case 0xFF:
					// fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", *counter);
					return -1;
					break;

				case 0x00:
					stuffers++;
					break;

				default:
					if (RST_MK(0xFF00 | aux))
						;
						// fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n",
							// *counter);
					// fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", *counter);
					return -1;
					break;
				}

			bit_count = 8;
		} else
			wwindow = window;
		newbit = (wwindow >> 7) & 1;
		window = wwindow << 1;
		bit_count--;
		result = (result << 1) | newbit;
	}
	return result;
}

unsigned int get_next_MK2(char ** fi,int* counter)
{
	unsigned char c;
	int ffmet = 0;
	int locpassed = -1;
//	int counter=*c;

	passed--;		/* as we fetch one anyway */
//	c = (*(*fi));
//	int * input=(int*)(*fi-*counter);
	
//	printf("out loop, c = %x\n",c);
	
	while ( (*counter)<201*sizeof(int)) {
//		c = (*(*fi));
//		c = c<<24;
//		c = c>>24;
		c=GETCHAR(fi,counter);
		mk_mon_debug_info(*counter);
		mk_mon_debug_info(c);
		// printf("in loop, c = %x\n",c);
//		(*counter)++;
//		(*fi)++;
		switch (c) {
		case 0xFF:
			ffmet = 1;
			break;
		case 0x00:
			ffmet = 0;
			break;
		default:
			if (locpassed > 1)
				;
				// fprintf(stderr, "NOTE: passed %d bytes\n", locpassed);
			if (ffmet){
//				printf("new c: %x\n",c);
				return (0xFF00 | c);
			}
			
			ffmet = 0;
			break;
		}
		
		locpassed++;
		passed++;
	}

	return (unsigned int)EOF;
}

unsigned int get_size2(char ** fi, int * counter)
{
	unsigned char aux,aux1;
	aux=GETCHAR(fi,counter);
	aux1=GETCHAR(fi,counter);
	mk_mon_debug_info(aux);
	mk_mon_debug_info(aux1);
	return (aux << 8) | aux1;	/* big endian */
}

int load_quant_tables2(char ** fi, int * counter)
{
	char aux;
	unsigned int size, n, i, id, x;

	size = get_size2(fi,counter);	/* this is the tables' size */
	mk_mon_debug_info(*counter);
	mk_mon_debug_info(size);
	n = (size - 2) / 65;

	for (i = 0; i < n; i++) {
		aux = GETCHAR(fi,counter);
		mk_mon_debug_info(*counter);
		mk_mon_debug_info(aux);
		if (first_quad(aux) > 0) {
			// fprintf(stderr, "\tERROR:\tBad QTable precision!\n");
			return -1;
		}
		id = second_quad(aux);
		if (verbose)
			;
			// fprintf(stderr, "\tINFO:\tLoading table %d\n", id);
		QTable[id] = (PBlock *) mk_malloc(sizeof(PBlock));
		if (QTable[id] == NULL) {
			// fprintf(stderr, "\tERROR:\tCould not allocate table storage!\n");
			exit(1);
		}
		QTvalid[id] = 1;
		for (x = 0; x < 64; x++)
			QTable[id]->linear[x] = GETCHAR(fi,counter);
		/*
		   -- This is useful to print out the table content --
		   for (x = 0; x < 64; x++)
		   fprintf(stderr, "%d\n", QTable[id]->linear[x]);
		 */
	}
	return 0;
}

int process_MCU2(char ** fi, int * counter)
{
	int i;
	long offset;
	int goodrows, goodcolumns;

	if (MCU_column == mx_size) {
		MCU_column = 0;
		MCU_row++;
		if (MCU_row == my_size) {
			in_frame = 0;
			return 0;
		}
		if (verbose)
			;
			// fprintf(stderr, "%ld:\tINFO:\tProcessing stripe %d/%d\n", *counter, MCU_row + 1, my_size);
	}

	for (curcomp = 0; MCU_valid[curcomp] != -1; curcomp++) {
		unpack_block2(fi, counter, FBuff, MCU_valid[curcomp]);	/* pass index to HT,QT,pred */
		IDCT(FBuff, MCU_buff[curcomp]);
	}

	/* YCrCb to RGB color space transform here */
	if (n_comp > 1)
		color_conversion();
	else
		memmove(ColorBuffer, MCU_buff[0], 64);

	/* cut last row/column as needed */
	if ((y_size != ry_size) && (MCU_row == (my_size - 1)))
		goodrows = y_size - ry_size;
	else
		goodrows = MCU_sy;

	if ((x_size != rx_size) && (MCU_column == (mx_size - 1)))
		goodcolumns = x_size - rx_size;
	else
		goodcolumns = MCU_sx;

	offset = n_comp * (MCU_row * MCU_sy * x_size + MCU_column * MCU_sx);

	for (i = 0; i < goodrows; i++)
		memmove(FrameBuffer + offset + n_comp * i * x_size, ColorBuffer + n_comp * i * MCU_sx,
			n_comp * goodcolumns);

	MCU_column++;
	return 1;
}

void fseek2(char ** fi,int * counter, int offset, char* pos){
	(*fi)=(char *)(pos+offset);
	*counter+=offset;	
}

void skip_segment2(char ** fi, int * counter)
{				/* skip a segment we don't want */
	unsigned int size;
	char tag[5];
	int i;

	// printf("addr before getsize1: %x\n",*fi);
	size = get_size2(fi,counter);
	// printf("addr after getsize1: %x\n",*fi);
	if (size > 5) {
		for (i = 0; i < 4; i++){
			// printf("addr before getChar: %x\n",*fi);
			tag[i] = GETCHAR(fi,counter);
			// printf("addr after getChar: %x\n",*fi);
		}
		tag[4] = '\0';
		if (verbose)
			;
		size -= 4;
	}
	// printf("counter before fseek1: %d, addr: %x \n",*counter,*fi);
	fseek2(fi,counter, size - 2, *fi);
	// printf("counter after fseek1: %d, addr: %x \n",*counter,*fi);
}
///////////////////////////////////////////////////////////////////////////////////////
//	end
///////////////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------*/
/* File : table_vld.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	   */
/*-----------------------------------------*/

// #include <stdlib.h>
// #include <stdio.h>

// #include "jpeg.h"

/*--------------------------------------*/
/* private huffman.c defines and macros */
/*--------------------------------------*/

/* Memory size of HTables; */
#define MAX_SIZE(hclass)		((hclass)?162:14)

/*--------------------------------------*/
/* some static structures for storage */
/*--------------------------------------*/

static unsigned char DC_Table0[MAX_SIZE(DC_CLASS)], DC_Table1[MAX_SIZE(DC_CLASS)];

static unsigned char AC_Table0[MAX_SIZE(AC_CLASS)], AC_Table1[MAX_SIZE(AC_CLASS)];

static unsigned char *HTable[4] = {
	&DC_Table0[0], &DC_Table1[0],
	&AC_Table0[0], &AC_Table1[0]
};

static int MinCode[4][16];
static int MaxCode[4][16];
static int ValPtr[4][16];

int load_huff_tables2(char ** fi, int * counter)
{
	char aux;
	int size, hclass, id, max;
	int LeavesN, LeavesT, i;
	int AuxCode;

	size = get_size2(fi,counter);	/* this is the tables' size */
	// printf("load_huff_tables1 size: %d\n",size);

	size -= 2;

	while (size > 0) {

		aux = GETCHAR(fi,counter);
		// printf("load_huff_tables1 aux: %x\n",aux);
		hclass = first_quad(aux);	/* AC or DC */
		id = second_quad(aux);	/* table no */
		if (id > 1) {
			// fprintf(stderr, "\tERROR:\tBad HTable identity %d!\n", id);
			return -1;
		}
		id = HUFF_ID(hclass, id);
		if (verbose)
			;
		size--;

		LeavesT = 0;
		AuxCode = 0;
		LeavesN = 0;

		for (i = 0; i < 16; i++) {
//			LeavesN = GETCHAR(fi,counter)<<24;
//			LeavesN = LeavesN>>24;
			LeavesN = GETCHAR(fi,counter);
			// printf("load_huff_tables1 LeavesN: %x\n",LeavesN);

			ValPtr[id][i] = LeavesT;
			MinCode[id][i] = AuxCode * 2;
			AuxCode = MinCode[id][i] + LeavesN;

			MaxCode[id][i] = (LeavesN) ? (AuxCode - 1) : (-1);
			LeavesT += LeavesN;
		}
		size -= 16;

		if (LeavesT > MAX_SIZE(hclass)) {
			max = MAX_SIZE(hclass);
			// fprintf(stderr, "\tWARNING:\tTruncating Table by %d symbols\n", LeavesT - max);
		} else
			max = LeavesT;

		for (i = 0; i < max; i++)
			HTable[id][i] = GETCHAR(fi,counter);	/* load in raw order */

		for (i = max; i < LeavesT; i++)
			GETCHAR(fi,counter);	/* skip if we don't load */
		size -= LeavesT;

		if (verbose)
			;

	}			/* loop on tables */
	return 0;
}

unsigned char get_symbol2(char ** fi, int * counter, int select)
{
	long code = 0;
	int length;
	int index;

	for (length = 0; length < 16; length++) {
		code = (2 * code) | get_one_bit2(fi,counter);
		if (code <= MaxCode[select][length])
			break;
	}

	index = ValPtr[select][length] + code - MinCode[select][length];

	if (index < MAX_SIZE(select / 2))
		return HTable[select][index];

	// fprintf(stderr, "%ld:\tWARNING:\tOverflowing symbol table !\n", *counter);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//	end
///////////////////////////////////////////////////////////////////////////////////////

/*-------------------------------------------*/
/* File : huffman.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	     */
/*-------------------------------------------*/

// #include <stdlib.h>
// #include <stdio.h>

// #include "jpeg.h"

/*--------------------------------------*/
/* private huffman.c defines and macros */
/*--------------------------------------*/

#define HUFF_EOB		0x00
#define HUFF_ZRL		0xF0

/*------------------------------------------*/
/* some constants for on-the-fly IQ and IZZ */
/*------------------------------------------*/

static const int const G_ZZ[] = {
	0, 1, 8, 16, 9, 2, 3, 10,
	17, 24, 32, 25, 18, 11, 4, 5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

/*-------------------------------------------------*/
/* here we unpack, predict, unquantify and reorder */
/* a complete 8*8 DCT block ...			   */
/*-------------------------------------------------*/

void unpack_block2(char ** fi, int * counter, FBlock * T, int select)
{
	unsigned int i, run, cat;
	int value;
	unsigned char symbol;

	/* Init the block with 0's: */
	for (i = 0; i < 64; i++)
		T->linear[i] = 0;

	/* First get the DC coefficient: */
	symbol = get_symbol2(fi, counter, HUFF_ID(DC_CLASS, comp[select].DC_HT));
	value = reformat(get_bits2(fi, counter, symbol), symbol);

	value += comp[select].PRED;
	comp[select].PRED = value;
	T->linear[0] = value * QTable[(int)comp[select].QT]->linear[0];

	/* Now get all 63 AC values: */
	for (i = 1; i < 64; i++) {
		symbol = get_symbol2(fi, counter, HUFF_ID(AC_CLASS, comp[select].AC_HT));
		if (symbol == HUFF_EOB)
			break;
		if (symbol == HUFF_ZRL) {
			i += 15;
			continue;
		}
		cat = symbol & 0x0F;
		run = (symbol >> 4) & 0x0F;
		i += run;
		value = reformat(get_bits2(fi, counter, cat), cat);

		/* Dequantify and ZigZag-reorder: */
		T->linear[G_ZZ[i]] = value * QTable[(int)comp[select].QT]->linear[i];
	}
}

///////////////////////////////////////////////////////////////////////////////////////
//	end
///////////////////////////////////////////////////////////////////////////////////////


#endif
