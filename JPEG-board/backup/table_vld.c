/*-----------------------------------------*/
/* File : table_vld.c, utilities for jfif view */
/* Author : Pierre Guerrier, march 1998	   */
/*-----------------------------------------*/

#include <stdlib.h>
#include <stdio.h>

#include "jpeg.h"

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

/*----------------------------------------------------------*/
/* Loading of Huffman table, with leaves drop ability	    */
/*----------------------------------------------------------*/

int load_huff_tables(FILE * fi)
{
	char aux;
	int size, hclass, id, max;
	int LeavesN, LeavesT, i;
	int AuxCode;

	size = get_size(fi);	/* this is the tables' size */
	printf("load_huff_tables size: %d\n",size);

	size -= 2;

	while (size > 0) {

		aux = fgetc(fi);
		printf("load_huff_tables aux: %x\n",aux);
		hclass = first_quad(aux);	/* AC or DC */
		id = second_quad(aux);	/* table no */
		if (id > 1) {
			fprintf(stderr, "\tERROR:\tBad HTable identity %d!\n", id);
			return -1;
		}
		id = HUFF_ID(hclass, id);
		if (verbose)
			fprintf(stderr, "\tINFO:\tLoading Table %d\n", id);
		size--;

		LeavesT = 0;
		AuxCode = 0;

		for (i = 0; i < 16; i++) {
			LeavesN = fgetc(fi);
			printf("load_huff_tables LeavesN: %x\n",LeavesN);

			ValPtr[id][i] = LeavesT;
			MinCode[id][i] = AuxCode * 2;
			AuxCode = MinCode[id][i] + LeavesN;

			MaxCode[id][i] = (LeavesN) ? (AuxCode - 1) : (-1);
			LeavesT += LeavesN;
		}
		size -= 16;

		if (LeavesT > MAX_SIZE(hclass)) {
			max = MAX_SIZE(hclass);
			fprintf(stderr, "\tWARNING:\tTruncating Table by %d symbols\n", LeavesT - max);
		} else
			max = LeavesT;

		for (i = 0; i < max; i++)
			HTable[id][i] = fgetc(fi);	/* load in raw order */

		for (i = max; i < LeavesT; i++)
			fgetc(fi);	/* skip if we don't load */
		size -= LeavesT;

		if (verbose)
			fprintf(stderr, "\tINFO:\tUsing %d words of table memory\n", LeavesT);

	}			/* loop on tables */
	return 0;
}

/*-----------------------------------*/
/* extract a single symbol from file */
/* using specified huffman table ... */
/*-----------------------------------*/

unsigned char get_symbol(FILE * fi, int select)
{
	long code = 0;
	int length;
	int index;

	for (length = 0; length < 16; length++) {
		code = (2 * code) | get_one_bit(fi);
		if (code <= MaxCode[select][length])
			break;
	}

	index = ValPtr[select][length] + code - MinCode[select][length];

	if (index < MAX_SIZE(select / 2))
		return HTable[select][index];

	fprintf(stderr, "%ld:\tWARNING:\tOverflowing symbol table !\n", ftell(fi));
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
//
//   modified
//
/////////////////////////////////////////////////////////////////////////////////////////

int load_huff_tables1(char ** fi, int * counter)
{
	char aux;
	int size, hclass, id, max;
	int LeavesN, LeavesT, i;
	int AuxCode;

	size = get_size1(fi,counter);	/* this is the tables' size */
	printf("load_huff_tables1 size: %d\n",size);

	size -= 2;

	while (size > 0) {

		aux = getChar(fi,counter);
		printf("load_huff_tables1 aux: %x\n",aux);
		hclass = first_quad(aux);	/* AC or DC */
		id = second_quad(aux);	/* table no */
		if (id > 1) {
			fprintf(stderr, "\tERROR:\tBad HTable identity %d!\n", id);
			return -1;
		}
		id = HUFF_ID(hclass, id);
		if (verbose)
			fprintf(stderr, "\tINFO:\tLoading Table %d\n", id);
		size--;

		LeavesT = 0;
		AuxCode = 0;
		LeavesN = 0;

		for (i = 0; i < 16; i++) {
			LeavesN = getChar(fi,counter)<<24;
			LeavesN = LeavesN>>24;
			printf("load_huff_tables1 LeavesN: %x\n",LeavesN);

			ValPtr[id][i] = LeavesT;
			MinCode[id][i] = AuxCode * 2;
			AuxCode = MinCode[id][i] + LeavesN;

			MaxCode[id][i] = (LeavesN) ? (AuxCode - 1) : (-1);
			LeavesT += LeavesN;
		}
		size -= 16;

		if (LeavesT > MAX_SIZE(hclass)) {
			max = MAX_SIZE(hclass);
			fprintf(stderr, "\tWARNING:\tTruncating Table by %d symbols\n", LeavesT - max);
		} else
			max = LeavesT;

		for (i = 0; i < max; i++)
			HTable[id][i] = getChar(fi,counter);	/* load in raw order */

		for (i = max; i < LeavesT; i++)
			getChar(fi,counter);	/* skip if we don't load */
		size -= LeavesT;

		if (verbose)
			fprintf(stderr, "\tINFO:\tUsing %d words of table memory\n", LeavesT);

	}			/* loop on tables */
	return 0;
}

int load_huff_tables2(char ** fi, int * counter)
{
	char aux;
	int size, hclass, id, max;
	int LeavesN, LeavesT, i;
	int AuxCode;

	size = get_size2(fi,counter);	/* this is the tables' size */
	printf("load_huff_tables1 size: %d\n",size);

	size -= 2;

	while (size > 0) {

		aux = GETCHAR(fi,counter);
		printf("load_huff_tables1 aux: %x\n",aux);
		hclass = first_quad(aux);	/* AC or DC */
		id = second_quad(aux);	/* table no */
		if (id > 1) {
			fprintf(stderr, "\tERROR:\tBad HTable identity %d!\n", id);
			return -1;
		}
		id = HUFF_ID(hclass, id);
		if (verbose)
			fprintf(stderr, "\tINFO:\tLoading Table %d\n", id);
		size--;

		LeavesT = 0;
		AuxCode = 0;
		LeavesN = 0;

		for (i = 0; i < 16; i++) {
//			LeavesN = GETCHAR(fi,counter)<<24;
//			LeavesN = LeavesN>>24;
			LeavesN = GETCHAR(fi,counter);
			printf("load_huff_tables1 LeavesN: %x\n",LeavesN);

			ValPtr[id][i] = LeavesT;
			MinCode[id][i] = AuxCode * 2;
			AuxCode = MinCode[id][i] + LeavesN;

			MaxCode[id][i] = (LeavesN) ? (AuxCode - 1) : (-1);
			LeavesT += LeavesN;
		}
		size -= 16;

		if (LeavesT > MAX_SIZE(hclass)) {
			max = MAX_SIZE(hclass);
			fprintf(stderr, "\tWARNING:\tTruncating Table by %d symbols\n", LeavesT - max);
		} else
			max = LeavesT;

		for (i = 0; i < max; i++)
			HTable[id][i] = GETCHAR(fi,counter);	/* load in raw order */

		for (i = max; i < LeavesT; i++)
			GETCHAR(fi,counter);	/* skip if we don't load */
		size -= LeavesT;

		if (verbose)
			fprintf(stderr, "\tINFO:\tUsing %d words of table memory\n", LeavesT);

	}			/* loop on tables */
	return 0;
}

unsigned char get_symbol1(char ** fi, int * counter, int select)
{
	long code = 0;
	int length;
	int index;

	for (length = 0; length < 16; length++) {
		code = (2 * code) | get_one_bit1(fi,counter);
		if (code <= MaxCode[select][length])
			break;
	}

	index = ValPtr[select][length] + code - MinCode[select][length];

	if (index < MAX_SIZE(select / 2))
		return HTable[select][index];

	fprintf(stderr, "%ld:\tWARNING:\tOverflowing symbol table !\n", *counter);
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

	fprintf(stderr, "%ld:\tWARNING:\tOverflowing symbol table !\n", *counter);
	return 0;
}


