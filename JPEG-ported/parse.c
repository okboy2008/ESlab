/*-----------------------------------------*/
/* File : parse.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	   */
/*-----------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeg.h"

/*---------------------------------------------------------------------*/

/* utility and counter to return the number of bits from file */
/* right aligned, masked, first bit towards MSB's		*/

static unsigned char bit_count;	/* available bits in the window */
static unsigned char window;

unsigned long get_bits(FILE * fi, int number)
{
	int i, newbit;
	unsigned long result = 0;
	unsigned char aux, wwindow;

	if (!number)
		return 0;

	for (i = 0; i < number; i++) {
		if (bit_count == 0) {
			wwindow = fgetc(fi);

			if (wwindow == 0xFF)
				switch (aux = fgetc(fi)) {	/* skip stuffer 0 byte */
				case 0xFF:
					fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", ftell(fi));
					aborted_stream(fi);
					break;

				case 0x00:
					stuffers++;
					break;

				default:
					if (RST_MK(0xFF00 | aux))
						fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n",
							ftell(fi));
					fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", ftell(fi));
					aborted_stream(fi);
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

void clear_bits(void)
{
	bit_count = 0;
}

unsigned char get_one_bit(FILE * fi)
{
	int newbit;
	unsigned char aux, wwindow;

	if (bit_count == 0) {
		wwindow = fgetc(fi);

		if (wwindow == 0xFF)
			switch (aux = fgetc(fi)) {	/* skip stuffer 0 byte */
			case 0xFF:
				fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", ftell(fi));
				aborted_stream(fi);
				break;

			case 0x00:
				stuffers++;
				break;

			default:
				if (RST_MK(0xFF00 | aux))
					fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n", ftell(fi));
				fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", ftell(fi));
				aborted_stream(fi);
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

/*----------------------------------------------------------*/

unsigned int get_size(FILE * fi)
{
	unsigned char aux;

	aux = fgetc(fi);
//	printf("old aux: %x\n",aux);
	return (aux << 8) | fgetc(fi);	/* big endian */
}

/*----------------------------------------------------------*/

void skip_segment(FILE * fi)
{				/* skip a segment we don't want */
	unsigned int size;
	char tag[5];
	int i;

	size = get_size(fi);
	if (size > 5) {
		for (i = 0; i < 4; i++)
			tag[i] = fgetc(fi);
		tag[4] = '\0';
		if (verbose)
			fprintf(stderr, "\tINFO:\tTag is %s\n", tag);
		size -= 4;
	}
	printf("counter before fseek: %d\n",ftell(fi));
	fseek(fi, size - 2, SEEK_CUR);
	printf("counter after fseek: %d\n",ftell(fi));
}

/*----------------------------------------------------------------*/
/* find next marker of any type, returns it, positions just after */
/* EOF instead of marker if end of file met while searching ...	  */
/*----------------------------------------------------------------*/

unsigned int get_next_MK(FILE * fi)
{
	unsigned int c;
	int ffmet = 0;
	int locpassed = -1;

	passed--;		/* as we fetch one anyway */

	while ((c = fgetc(fi)) != (unsigned int)EOF) {
		printf("in old loop, c = %x\n",c);
		switch (c) {
		case 0xFF:
			ffmet = 1;
			break;
		case 0x00:
			ffmet = 0;
			break;
		default:
			if (locpassed > 1)
				fprintf(stderr, "NOTE: passed %d bytes\n", locpassed);
			if (ffmet){
//				printf("old c: %x\n",c);
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

/*----------------------------------------------------------*/
/* loading and allocating of quantization table             */
/* table elements are in ZZ order (same as unpack output)   */
/*----------------------------------------------------------*/

int load_quant_tables(FILE * fi)
{
	char aux;
	unsigned int size, n, i, id, x;

	size = get_size(fi);	/* this is the tables' size */
	n = (size - 2) / 65;

	for (i = 0; i < n; i++) {
		aux = fgetc(fi);
		if (first_quad(aux) > 0) {
			fprintf(stderr, "\tERROR:\tBad QTable precision!\n");
			return -1;
		}
		id = second_quad(aux);
		if (verbose)
			fprintf(stderr, "\tINFO:\tLoading table %d\n", id);
		QTable[id] = (PBlock *) malloc(sizeof(PBlock));
		if (QTable[id] == NULL) {
			fprintf(stderr, "\tERROR:\tCould not allocate table storage!\n");
			exit(1);
		}
		QTvalid[id] = 1;
		for (x = 0; x < 64; x++)
			QTable[id]->linear[x] = fgetc(fi);
		/*
		   -- This is useful to print out the table content --
		   for (x = 0; x < 64; x++)
		   fprintf(stderr, "%d\n", QTable[id]->linear[x]);
		 */
	}
	return 0;
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
			MCU_buff[k] = (PBlock *) malloc(sizeof(PBlock));
			if (MCU_buff[k] == NULL) {
				fprintf(stderr, "\tERROR:\tCould not allocate MCU buffers!\n");
				exit(1);
			}
			k++;
			if (k == 10) {
				fprintf(stderr, "\tERROR:\tMax subsampling exceeded!\n");
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

int process_MCU(FILE * fi)
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
			fprintf(stderr, "%ld:\tINFO:\tProcessing stripe %d/%d\n", ftell(fi), MCU_row + 1, my_size);
	}

	for (curcomp = 0; MCU_valid[curcomp] != -1; curcomp++) {
		unpack_block(fi, FBuff, MCU_valid[curcomp]);	/* pass index to HT,QT,pred */
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


/////////////////////////////////////////////////////////////////////////////////////////
//
//   modified
//
/////////////////////////////////////////////////////////////////////////////////////////

unsigned char get_one_bit1(char ** fi, int * counter)
{
	int newbit;
	unsigned char aux, wwindow;

	if (bit_count == 0) {
		wwindow = getChar(fi,counter);

		if (wwindow == 0xFF)
			switch (aux = getChar(fi,counter)) {	/* skip stuffer 0 byte */
			case 0xFF:
				fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", *counter);
				return -1;
				break;

			case 0x00:
				stuffers++;
				break;

			default:
				if (RST_MK(0xFF00 | aux))
					fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n", *counter);
				fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", *counter);
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

unsigned char get_one_bit2(char ** fi, int * counter)
{
	int newbit;
	unsigned char aux, wwindow;

	if (bit_count == 0) {
		wwindow = GETCHAR(fi,counter);

		if (wwindow == 0xFF)
			switch (aux = GETCHAR(fi,counter)) {	/* skip stuffer 0 byte */
			case 0xFF:
				fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", *counter);
				return -1;
				break;

			case 0x00:
				stuffers++;
				break;

			default:
				if (RST_MK(0xFF00 | aux))
					fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n", *counter);
				fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", *counter);
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

unsigned long get_bits1(char ** fi, int * counter, int number)
{
	int i, newbit;
	unsigned long result = 0;
	unsigned char aux, wwindow;

	if (!number)
		return 0;

	for (i = 0; i < number; i++) {
		if (bit_count == 0) {
			wwindow = getChar(fi,counter);

			if (wwindow == 0xFF)
				switch (aux = getChar(fi,counter)) {	/* skip stuffer 0 byte */
				case 0xFF:
					fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", *counter);
					return -1;
					break;

				case 0x00:
					stuffers++;
					break;

				default:
					if (RST_MK(0xFF00 | aux))
						fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n",
							*counter);
					fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", *counter);
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
					fprintf(stderr, "%ld:\tERROR:\tRan out of bit stream\n", *counter);
					return -1;
					break;

				case 0x00:
					stuffers++;
					break;

				default:
					if (RST_MK(0xFF00 | aux))
						fprintf(stderr, "%ld:\tERROR:\tSpontaneously found restart!\n",
							*counter);
					fprintf(stderr, "%ld:\tERROR:\tLost sync in bit stream\n", *counter);
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

unsigned int get_next_MK1(char ** fi,int* counter)
{
	unsigned int c;
	int ffmet = 0;
	int locpassed = -1;
//	int counter=*c;

	passed--;		/* as we fetch one anyway */
	c = (*(*fi));
	
//	printf("out loop, c = %x\n",c);
	
	while ( (*counter)<201*sizeof(int)) {
		c = (*(*fi));
		c = c<<24;
		c = c>>24;
		printf("in loop, c = %x\n",c);
		(*counter)++;
		(*fi)++;
		switch (c) {
		case 0xFF:
			ffmet = 1;
			break;
		case 0x00:
			ffmet = 0;
			break;
		default:
			if (locpassed > 1)
				fprintf(stderr, "NOTE: passed %d bytes\n", locpassed);
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

unsigned int get_next_MK2(char ** fi,int* counter)
{
	unsigned int c;
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
		printf("in loop, c = %x\n",c);
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
				fprintf(stderr, "NOTE: passed %d bytes\n", locpassed);
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

unsigned int get_size1(char ** fi, int * counter)
{
	unsigned char aux,aux1;

	aux = *(*fi);
//	printf("__aux: %x\n",aux);
	
	(*fi)++;
	(*counter)++;
	aux1 = *(*fi);
//	printf("__aux1: %x\n",aux1);
	
	(*fi)++;
	(*counter)++;
//	printf("aux: %x\n",aux);
//	printf("aux1: %x\n",aux1);
	return (aux << 8) | aux1;	/* big endian */
}

unsigned int get_size2(char ** fi, int * counter)
{
	unsigned char aux,aux1;
//	int * input=(int *)(*fi-*counter);

//	aux = *(*fi);
////	printf("__aux: %x\n",aux);
//	
//	(*fi)++;
//	(*counter)++;
	aux=GETCHAR(fi,counter);
	aux1 = GETCHAR(fi,counter);
////	printf("__aux1: %x\n",aux1);
//	
//	(*fi)++;
//	(*counter)++;
	printf("aux: %x, aux1: %x\n",aux,aux1);
//	printf("aux1: %x\n",aux1);
	return ((aux << 8) | aux1);	/* big endian */
}


int load_quant_tables1(char ** fi, int * counter)
{
	char aux;
	unsigned int size, n, i, id, x;

	size = get_size1(fi,counter);	/* this is the tables' size */
	n = (size - 2) / 65;

	for (i = 0; i < n; i++) {
		aux = getChar(fi,counter);
		if (first_quad(aux) > 0) {
			fprintf(stderr, "\tERROR:\tBad QTable precision!\n");
			return -1;
		}
		id = second_quad(aux);
		if (verbose)
			fprintf(stderr, "\tINFO:\tLoading table %d\n", id);
		QTable[id] = (PBlock *) malloc(sizeof(PBlock));
		if (QTable[id] == NULL) {
			fprintf(stderr, "\tERROR:\tCould not allocate table storage!\n");
			exit(1);
		}
		QTvalid[id] = 1;
		for (x = 0; x < 64; x++)
			QTable[id]->linear[x] = getChar(fi,counter);
		/*
		   -- This is useful to print out the table content --
		   for (x = 0; x < 64; x++)
		   fprintf(stderr, "%d\n", QTable[id]->linear[x]);
		 */
	}
	return 0;
}

int load_quant_tables2(char ** fi, int * counter)
{
	char aux;
	unsigned int size, n, i, id, x;
	printf("load quant tables2\n");
	size = get_size2(fi,counter);	/* this is the tables' size */
	printf("counter=%d,size=%d",*counter,size);
	n = (size - 2) / 65;

	for (i = 0; i < n; i++) {
		aux = GETCHAR(fi,counter);
		printf("counter=%d,aux=%x",*counter,aux);
		if (first_quad(aux) > 0) {
			fprintf(stderr, "\tERROR:\tBad QTable precision!\n");
			return -1;
		}
		id = second_quad(aux);
		if (verbose)
			fprintf(stderr, "\tINFO:\tLoading table %d\n", id);
		QTable[id] = (PBlock *) malloc(sizeof(PBlock));
		if (QTable[id] == NULL) {
			fprintf(stderr, "\tERROR:\tCould not allocate table storage!\n");
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

int process_MCU1(char ** fi, int * counter)
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
			fprintf(stderr, "%ld:\tINFO:\tProcessing stripe %d/%d\n", *counter, MCU_row + 1, my_size);
	}

	for (curcomp = 0; MCU_valid[curcomp] != -1; curcomp++) {
		unpack_block1(fi, counter, FBuff, MCU_valid[curcomp]);	/* pass index to HT,QT,pred */
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
			fprintf(stderr, "%ld:\tINFO:\tProcessing stripe %d/%d\n", *counter, MCU_row + 1, my_size);
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

void fseek1(char ** fi,int * counter, int offset, char* pos){
	(*fi)=(char *)(pos+offset);
	*counter+=offset;	
}

void fseek2(char ** fi,int * counter, int offset, char* pos){
	(*fi)=(char *)(pos+offset);
	*counter+=offset;	
}

void skip_segment1(char ** fi, int * counter)
{				/* skip a segment we don't want */
	unsigned int size;
	char tag[5];
	int i;

	printf("addr before getsize1: %x\n",*fi);
	size = get_size1(fi,counter);
	printf("addr after getsize1: %x\n",*fi);
	if (size > 5) {
		for (i = 0; i < 4; i++){
			printf("addr before getChar: %x\n",*fi);
			tag[i] = getChar(fi,counter);
			printf("addr after getChar: %x\n",*fi);
		}
		tag[4] = '\0';
		if (verbose)
			fprintf(stderr, "\tINFO:\tTag is %s\n", tag);
		size -= 4;
	}
	printf("counter before fseek1: %d, addr: %x \n",*counter,*fi);
	fseek1(fi,counter, size - 2, *fi);
	printf("counter after fseek1: %d, addr: %x \n",*counter,*fi);
}

void skip_segment2(char ** fi, int * counter)
{				/* skip a segment we don't want */
	unsigned int size;
	char tag[5];
	int i;

//	printf("addr before getsize1: %x\n",*fi);
	size = get_size2(fi,counter);
//	printf("addr after getsize1: %x\n",*fi);
	if (size > 5) {
		for (i = 0; i < 4; i++){
//			printf("addr before getChar: %x\n",*fi);
			tag[i] = GETCHAR(fi,counter);
//			printf("addr after getChar: %x\n",*fi);
		}
		tag[4] = '\0';
		if (verbose)
			fprintf(stderr, "\tINFO:\tTag is %s\n", tag);
		size -= 4;
	}
//	printf("counter before fseek1: %d, addr: %x \n",*counter,*fi);
	fseek2(fi,counter, size - 2, *fi);
//	printf("counter after fseek1: %d, addr: %x \n",*counter,*fi);
}
