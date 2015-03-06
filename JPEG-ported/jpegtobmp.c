#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* real declaration of global variables here */
/* see jpeg.h for more info			*/

#include "jpeg.h"
#include "surfer.jpg.h"

/* descriptors for 3 components */
cd_t comp[3];
/* decoded DCT blocks buffer */
PBlock *MCU_buff[10];
/* components of above MCU blocks */
int MCU_valid[10];

/* quantization tables */
PBlock *QTable[4];
int QTvalid[4];

/* Video frame size     */
int x_size, y_size;
/* down-rounded Video frame size (integer MCU) */
int rx_size, ry_size;
/* MCU size in pixels   */
int MCU_sx, MCU_sy;
/* picture size in units of MCUs */
int mx_size, my_size;
/* number of components 1,3 */
int n_comp;

/* MCU after color conversion */
unsigned char *ColorBuffer;
/* complete final RGB image */
unsigned char *FrameBuffer;
/* scratch frequency buffer */
FBlock *FBuff;
/* scratch pixel buffer */
PBlock *PBuff;
/* frame started ? current component ? */
int in_frame, curcomp;
/* current position in MCU unit */
int MCU_row, MCU_column;

/* input  File stream pointer   */
FILE *fi;

/* stuff bytes in entropy coded segments */
int stuffers = 0;
/* bytes passed when searching markers */
int passed = 0;

int verbose = 0;

/* Extra global variables for 5kk03 */

int vld_count = 0;		/* Counter used by FGET and FSEEK in 5kk03.c */
unsigned int input_buffer[JPGBUFFER_SIZE / sizeof(int)];

/* End extra global variables for 5kk03 */

int JpegToBmp(char *file1, char *file2);

void convertJpeg(int* src, int* dst, int num){
	char* p_s=(char*)src;
	char* p_d=(char*)dst;
	int i;
	for(i=0;i<num;i++){
		*p_d=*(p_s+3);
		*(p_d+1)=*(p_s+2);
		*(p_d+2)=*(p_s+1);
		*(p_d+3)=*(p_s);
		p_s+=4;
		p_d+=4;
	}
}

char getChar(char** fi, int* counter){
		unsigned char c = (*(*fi));
		(*counter)++;
		(*fi)++;
		return c;
}

char GETCHAR(char** fi,int* counter){
	int* input=(int*)(*fi-*counter);
	unsigned int c = ((input[(*counter) / 4] << (8 * ((*counter) % 4))) >> 24) & 0x00ff;
	(*counter)++;
	(*fi)++;
	return c;
}

int JpegToBmp1(char *fi, char *file2)
{
	unsigned int aux, mark;
	int n_restarts, restart_interval, leftover;	/* RST check */
	int i, j;
	int counter=0;

	/* First find the SOI marker: */
	aux = get_next_MK1(&fi,&counter);
	if (aux != SOI_MK)
		return -1;

	if (verbose)
		fprintf(stderr, "%ld:\tINFO:\tFound the SOI marker!\n", counter);
	in_frame = 0;
	restart_interval = 0;
	for (i = 0; i < 4; i++)
		QTvalid[i] = 0;

	/* Now process segments as they appear: */
	do {
		mark = get_next_MK1(&fi,&counter);
		printf("1: %x, %d\n",mark,counter);

		switch (mark) {
		case SOF_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the SOF marker!\n", counter);
			in_frame = 1;
			get_size1(&fi,&counter);	/* header size, don't care */

			/* load basic image parameters */
			//fgetc(fi);	/* precision, 8bit, don't care */
			getChar(&fi,&counter);
			y_size = get_size1(&fi,&counter);
			x_size = get_size1(&fi,&counter);
			if (verbose)
				fprintf(stderr, "\tINFO:\tImage size is %d by %d\n", x_size, y_size);

			//n_comp = fgetc(fi);	/* # of components */
			n_comp=getChar(&fi,&counter);
			if (verbose) {
				fprintf(stderr, "\tINFO:\t");
				switch (n_comp) {
				case 1:
					fprintf(stderr, "Monochrome");
					break;
				case 3:
					fprintf(stderr, "Color");
					break;
				default:
					fprintf(stderr, "Not a");
					break;
				}
				fprintf(stderr, " JPEG image!\n");
			}

			for (i = 0; i < n_comp; i++) {
				/* component specifiers */
				comp[i].CID = getChar(&fi,&counter);
				aux = getChar(&fi,&counter);
				comp[i].HS = first_quad(aux);
				comp[i].VS = second_quad(aux);
				comp[i].QT = getChar(&fi,&counter);
			}
			if ((n_comp > 1) && verbose)
				fprintf(stderr,
					"\tINFO:\tColor format is %d:%d:%d, H=%d\n",
					comp[0].HS * comp[0].VS, comp[1].HS * comp[1].VS, comp[2].HS * comp[2].VS,
					comp[1].HS);

			if (init_MCU() == -1)
				return -1;

			/* dimension scan buffer for YUV->RGB conversion */
			printf("buffer size: frame size=%d, color size=%d\n",x_size * y_size * n_comp,MCU_sx * MCU_sy * n_comp);
			FrameBuffer = (unsigned char *)malloc((size_t) x_size * y_size * n_comp);
			ColorBuffer = (unsigned char *)malloc((size_t) MCU_sx * MCU_sy * n_comp);
			FBuff = (FBlock *) malloc(sizeof(FBlock));
			PBuff = (PBlock *) malloc(sizeof(PBlock));

			if ((FrameBuffer == NULL) || (ColorBuffer == NULL) || (FBuff == NULL) || (PBuff == NULL)) {
				fprintf(stderr, "\tERROR:\tCould not allocate pixel storage!\n");
				exit(1);
			}
			break;

		case DHT_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Huffman Tables\n", counter);
			if (load_huff_tables1(&fi,&counter) == -1)
				return -1;
			break;

		case DQT_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Quantization Tables\n", counter);
			if (load_quant_tables1(&fi,&counter) == -1)
				return -1;
			break;

		case DRI_MK:
//			get_size(fi);	/* skip size */
			get_size1(&fi,&counter);
			restart_interval = get_size1(&fi,&counter);
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Restart Interval %d\n", counter,
					restart_interval);
			break;

		case SOS_MK:	/* lots of things to do here */
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the SOS marker!\n", counter);
			get_size1(&fi,&counter);	/* don't care */
			aux = getChar(&fi,&counter);
			if (aux != (unsigned int)n_comp) {
				fprintf(stderr, "\tERROR:\tBad component interleaving!\n");
				return -1;
			}

			for (i = 0; i < n_comp; i++) {
				aux = getChar(&fi,&counter);
				if (aux != comp[i].CID) {
					fprintf(stderr, "\tERROR:\tBad Component Order!\n");
					return -1;
				}
				aux = getChar(&fi,&counter);
				comp[i].DC_HT = first_quad(aux);
				comp[i].AC_HT = second_quad(aux);
			}
			get_size1(&fi,&counter);
//			fgetc(fi);	/* skip things */
			getChar(&fi,&counter);

			MCU_column = 0;
			MCU_row = 0;
			clear_bits();
			reset_prediction();
///////////////////////////////////////////////////////////////////////////////////////////////
//
//		stop here
//
///////////////////////////////////////////////////////////////////////////////////////////////
			/* main MCU processing loop here */
			if (restart_interval) {
				n_restarts = ceil_div(mx_size * my_size, restart_interval) - 1;
				leftover = mx_size * my_size - n_restarts * restart_interval;
				/* final interval may be incomplete */

				for (i = 0; i < n_restarts; i++) {
					for (j = 0; j < restart_interval; j++)
						process_MCU1(&fi,&counter);
					/* proc till all EOB met */

					aux = get_next_MK1(&fi,&counter);
					if (!RST_MK(aux)) {
						fprintf(stderr, "%ld:\tERROR:\tLost Sync after interval!\n", counter);
						return -1;
					} else if (verbose)
						fprintf(stderr, "%ld:\tINFO:\tFound Restart Marker\n", counter);

					reset_prediction();
					clear_bits();
				}	/* intra-interval loop */
			} else
				leftover = mx_size * my_size;

			/* process till end of row without restarts */
			for (i = 0; i < leftover; i++)
				process_MCU1(&fi,&counter);

			in_frame = 0;
			break;

		case EOI_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the EOI marker!\n", counter);
			if (in_frame)
				return -1;

			if (verbose)
				fprintf(stderr, "\tINFO:\tTotal skipped bytes %d, total stuffers %d\n", passed,
					stuffers);
			//fclose(fi);
		
			write_bmp(file2);

			free_structures();
			return 0;
			break;

		case COM_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tSkipping comments\n", counter);
			skip_segment1(&fi,&counter);
			break;

		case EOF:
			if (verbose)
				fprintf(stderr, "%ld:\tERROR:\tRan out of input data!\n", counter);
			return -1;

		default:
			if ((mark & MK_MSK) == APP_MK) {
				printf("in APP_MK mark: %x\n",mark);
				if (verbose)
					fprintf(stderr, "%ld:\tINFO:\tSkipping application data\n", counter);
				skip_segment1(&fi,&counter);
				break;
			}
			if (RST_MK(mark)) {
				reset_prediction();
				break;
			}
			/* if all else has failed ... */
			fprintf(stderr, "%ld:\tWARNING:\tLost Sync outside scan, %d!\n", counter, mark);
			return -1;
			break;
		}		/* end switch */
	} while (1);

	return 0;
}

int JpegToBmp2(char *fi, char *file2)
{
	unsigned int aux, mark;
	int n_restarts, restart_interval, leftover;	/* RST check */
	int i, j;
	int counter=0;

	/* First find the SOI marker: */
	aux = get_next_MK2(&fi,&counter);
	if (aux != SOI_MK)
		return -1;

	if (verbose)
		fprintf(stderr, "%ld:\tINFO:\tFound the SOI marker!\n", counter);
	in_frame = 0;
	restart_interval = 0;
	for (i = 0; i < 4; i++)
		QTvalid[i] = 0;

	/* Now process segments as they appear: */
	do {
		mark = get_next_MK2(&fi,&counter);
		printf("1: %x, %d\n",mark,counter);

		switch (mark) {
		case SOF_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the SOF marker!\n", counter);
			in_frame = 1;
			get_size2(&fi,&counter);	/* header size, don't care */

			/* load basic image parameters */
			//fgetc(fi);	/* precision, 8bit, don't care */
			GETCHAR(&fi,&counter);
			y_size = get_size2(&fi,&counter);
			x_size = get_size2(&fi,&counter);
			if (verbose)
				fprintf(stderr, "\tINFO:\tImage size is %d by %d\n", x_size, y_size);

			//n_comp = fgetc(fi);	/* # of components */
			n_comp=GETCHAR(&fi,&counter);
			if (verbose) {
				fprintf(stderr, "\tINFO:\t");
				switch (n_comp) {
				case 1:
					fprintf(stderr, "Monochrome");
					break;
				case 3:
					fprintf(stderr, "Color");
					break;
				default:
					fprintf(stderr, "Not a");
					break;
				}
				fprintf(stderr, " JPEG image!\n");
			}

			for (i = 0; i < n_comp; i++) {
				/* component specifiers */
				comp[i].CID = GETCHAR(&fi,&counter);
				aux = GETCHAR(&fi,&counter);
				comp[i].HS = first_quad(aux);
				comp[i].VS = second_quad(aux);
				comp[i].QT = GETCHAR(&fi,&counter);
			}
			if ((n_comp > 1) && verbose)
				fprintf(stderr,
					"\tINFO:\tColor format is %d:%d:%d, H=%d\n",
					comp[0].HS * comp[0].VS, comp[1].HS * comp[1].VS, comp[2].HS * comp[2].VS,
					comp[1].HS);

			if (init_MCU() == -1)
				return -1;

			/* dimension scan buffer for YUV->RGB conversion */
//			printf("buffer size: frame size=%d, color size=%d\n",x_size * y_size * n_comp,MCU_sx * MCU_sy * n_comp);
			FrameBuffer = (unsigned char *)malloc((size_t) x_size * y_size * n_comp);
			ColorBuffer = (unsigned char *)malloc((size_t) MCU_sx * MCU_sy * n_comp);
			FBuff = (FBlock *) malloc(sizeof(FBlock));
			PBuff = (PBlock *) malloc(sizeof(PBlock));

			if ((FrameBuffer == NULL) || (ColorBuffer == NULL) || (FBuff == NULL) || (PBuff == NULL)) {
				fprintf(stderr, "\tERROR:\tCould not allocate pixel storage!\n");
				exit(1);
			}
			break;

		case DHT_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Huffman Tables\n", counter);
			if (load_huff_tables2(&fi,&counter) == -1)
				return -1;
			break;

		case DQT_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Quantization Tables\n", counter);
			if (load_quant_tables2(&fi,&counter) == -1)
				return -1;
			break;

		case DRI_MK:
//			get_size(fi);	/* skip size */
			get_size2(&fi,&counter);
			restart_interval = get_size2(&fi,&counter);
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Restart Interval %d\n", counter,
					restart_interval);
			break;

		case SOS_MK:	/* lots of things to do here */
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the SOS marker!\n", counter);
			get_size2(&fi,&counter);	/* don't care */
			aux = GETCHAR(&fi,&counter);
			if (aux != (unsigned int)n_comp) {
				fprintf(stderr, "\tERROR:\tBad component interleaving!\n");
				return -1;
			}

			for (i = 0; i < n_comp; i++) {
				aux = GETCHAR(&fi,&counter);
				if (aux != comp[i].CID) {
					fprintf(stderr, "\tERROR:\tBad Component Order!\n");
					return -1;
				}
				aux = GETCHAR(&fi,&counter);
				comp[i].DC_HT = first_quad(aux);
				comp[i].AC_HT = second_quad(aux);
			}
			get_size2(&fi,&counter);
//			fgetc(fi);	/* skip things */
			GETCHAR(&fi,&counter);

			MCU_column = 0;
			MCU_row = 0;
			clear_bits();
			reset_prediction();

			/* main MCU processing loop here */
			if (restart_interval) {
				n_restarts = ceil_div(mx_size * my_size, restart_interval) - 1;
				leftover = mx_size * my_size - n_restarts * restart_interval;
				/* final interval may be incomplete */

				for (i = 0; i < n_restarts; i++) {
					for (j = 0; j < restart_interval; j++)
						process_MCU2(&fi,&counter);
					/* proc till all EOB met */

					aux = get_next_MK2(&fi,&counter);
					if (!RST_MK(aux)) {
						fprintf(stderr, "%ld:\tERROR:\tLost Sync after interval!\n", counter);
						return -1;
					} else if (verbose)
						fprintf(stderr, "%ld:\tINFO:\tFound Restart Marker\n", counter);

					reset_prediction();
					clear_bits();
				}	/* intra-interval loop */
			} else
				leftover = mx_size * my_size;

			/* process till end of row without restarts */
			for (i = 0; i < leftover; i++)
				process_MCU2(&fi,&counter);

			in_frame = 0;
			break;

		case EOI_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the EOI marker!\n", counter);
			if (in_frame)
				return -1;

			if (verbose)
				fprintf(stderr, "\tINFO:\tTotal skipped bytes %d, total stuffers %d\n", passed,
					stuffers);
			//fclose(fi);
			int i;
			printf("framebuffer:\n");
			for(i=0;i<10;i++){
				printf("%d:%x\n",i,FrameBuffer[i]);
			}
			write_bmp(file2);

			free_structures();
			return 0;
			break;

		case COM_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tSkipping comments\n", counter);
			skip_segment2(&fi,&counter);
			break;

		case EOF:
			if (verbose)
				fprintf(stderr, "%ld:\tERROR:\tRan out of input data!\n", counter);
			return -1;

		default:
			if ((mark & MK_MSK) == APP_MK) {
				printf("in APP_MK mark: %x\n",mark);
				if (verbose)
					fprintf(stderr, "%ld:\tINFO:\tSkipping application data\n", counter);
				skip_segment2(&fi,&counter);
				break;
			}
			if (RST_MK(mark)) {
				reset_prediction();
				break;
			}
			/* if all else has failed ... */
			fprintf(stderr, "%ld:\tWARNING:\tLost Sync outside scan, %d!\n", counter, mark);
			return -1;
			break;
		}		/* end switch */
	} while (1);

	return 0;
}


void memmove1 ( void * destination, void * source, size_t num ){
	int i;
	for(i=0;i<num;i++){
		*((char*)(destination+i))=*((char*)(source+i));
	}
}

int main()
{
	int counter=0;
	int surfer[201];
	int i;
	convertJpeg(surfer_jpg,surfer,201);
//	get_next_MK() test
//	char *file1="./surfer.jpg";
//	fi = fopen(file1, "rb");
//	if (fi == NULL) {
//		printf("unable to open the file %s\n", file1);
//		return 0;
//	}
//	int aux;
//	for(i=0;i<3;i++){
//		printf("loop %d:\n",i);
//		aux = get_next_MK(fi);
//		printf("old value: %x\n",aux);
//	}
//	printf("old getsize: %d\n",get_size(fi));
//	printf("old fgetc: %x\n",fgetc(fi));
//	
	char *test=(char*)surfer;
	char *test2=(char*)surfer_jpg;
//	for(i=0;i<3;i++){
//		printf("loop %d:\n",i);
//		printf("old addr: %x\n",test);
//		aux=get_next_MK2(&test,&counter);
//		printf("new addr: %x\n",test);
//		printf("new value: %x\n",aux);
//	}
//	
//	
//	
////	printf("EOF: %x\n",(unsigned int)EOF);
////	char cc=*((char*)surfer);
//	printf("counter: %d\n",counter);
//	printf("new getsize: %d\n",get_size1(&test,&counter));
//	printf("counter: %d\n",counter);
//	printf("new getchar: %x\n",getChar(&test,&counter));
//	printf("counter: %d\n",counter);
//	


//	JpegToBmp("./surfer.jpg", "surfer.bmp");
//	JpegToBmp1(test,"surfer2.bmp");
	JpegToBmp2(test2,"surfer3.bmp");
	char str[] = "memmove can be very useful......";
  	memmove (str+20,str+15,11);
  	puts (str);
  	return 0;

}

/*-----------------------------------------------------------------*/
/*		MAIN		MAIN		MAIN		   */
/*-----------------------------------------------------------------*/

int JpegToBmp(char *file1, char *file2)
{
	unsigned int aux, mark;
	int n_restarts, restart_interval, leftover;	/* RST check */
	int i, j;

	fi = fopen(file1, "rb");
	if (fi == NULL) {
		printf("unable to open the file %s\n", file1);
		return 0;
	}

	/* First find the SOI marker: */
	aux = get_next_MK(fi);
	if (aux != SOI_MK)
		aborted_stream(fi);

	if (verbose)
		fprintf(stderr, "%ld:\tINFO:\tFound the SOI marker!\n", ftell(fi));
	in_frame = 0;
	restart_interval = 0;
	for (i = 0; i < 4; i++)
		QTvalid[i] = 0;

	/* Now process segments as they appear: */
	do {
		mark = get_next_MK(fi);
		printf("0: %x, %d\n",mark,ftell(fi));

		switch (mark) {
		case SOF_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the SOF marker!\n", ftell(fi));
			in_frame = 1;
			get_size(fi);	/* header size, don't care */

			/* load basic image parameters */
			fgetc(fi);	/* precision, 8bit, don't care */
			y_size = get_size(fi);
			x_size = get_size(fi);
			if (verbose)
				fprintf(stderr, "\tINFO:\tImage size is %d by %d\n", x_size, y_size);

			n_comp = fgetc(fi);	/* # of components */
			if (verbose) {
				fprintf(stderr, "\tINFO:\t");
				switch (n_comp) {
				case 1:
					fprintf(stderr, "Monochrome");
					break;
				case 3:
					fprintf(stderr, "Color");
					break;
				default:
					fprintf(stderr, "Not a");
					break;
				}
				fprintf(stderr, " JPEG image!\n");
			}

			for (i = 0; i < n_comp; i++) {
				/* component specifiers */
				comp[i].CID = fgetc(fi);
				aux = fgetc(fi);
				comp[i].HS = first_quad(aux);
				comp[i].VS = second_quad(aux);
				comp[i].QT = fgetc(fi);
			}
			if ((n_comp > 1) && verbose)
				fprintf(stderr,
					"\tINFO:\tColor format is %d:%d:%d, H=%d\n",
					comp[0].HS * comp[0].VS, comp[1].HS * comp[1].VS, comp[2].HS * comp[2].VS,
					comp[1].HS);

			if (init_MCU() == -1)
				aborted_stream(fi);

			/* dimension scan buffer for YUV->RGB conversion */
			FrameBuffer = (unsigned char *)malloc((size_t) x_size * y_size * n_comp);
			ColorBuffer = (unsigned char *)malloc((size_t) MCU_sx * MCU_sy * n_comp);
			FBuff = (FBlock *) malloc(sizeof(FBlock));
			PBuff = (PBlock *) malloc(sizeof(PBlock));

			if ((FrameBuffer == NULL) || (ColorBuffer == NULL) || (FBuff == NULL) || (PBuff == NULL)) {
				fprintf(stderr, "\tERROR:\tCould not allocate pixel storage!\n");
				exit(1);
			}
			break;

		case DHT_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Huffman Tables\n", ftell(fi));
			if (load_huff_tables(fi) == -1)
				aborted_stream(fi);
			break;

		case DQT_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Quantization Tables\n", ftell(fi));
			if (load_quant_tables(fi) == -1)
				aborted_stream(fi);
			break;

		case DRI_MK:
			get_size(fi);	/* skip size */
			restart_interval = get_size(fi);
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tDefining Restart Interval %d\n", ftell(fi),
					restart_interval);
			break;

		case SOS_MK:	/* lots of things to do here */
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the SOS marker!\n", ftell(fi));
			get_size(fi);	/* don't care */
			aux = fgetc(fi);
			if (aux != (unsigned int)n_comp) {
				fprintf(stderr, "\tERROR:\tBad component interleaving!\n");
				aborted_stream(fi);
			}

			for (i = 0; i < n_comp; i++) {
				aux = fgetc(fi);
				if (aux != comp[i].CID) {
					fprintf(stderr, "\tERROR:\tBad Component Order!\n");
					aborted_stream(fi);
				}
				aux = fgetc(fi);
				comp[i].DC_HT = first_quad(aux);
				comp[i].AC_HT = second_quad(aux);
			}
			get_size(fi);
			fgetc(fi);	/* skip things */

			MCU_column = 0;
			MCU_row = 0;
			clear_bits();
			reset_prediction();

			/* main MCU processing loop here */
			if (restart_interval) {
				n_restarts = ceil_div(mx_size * my_size, restart_interval) - 1;
				leftover = mx_size * my_size - n_restarts * restart_interval;
				/* final interval may be incomplete */

				for (i = 0; i < n_restarts; i++) {
					for (j = 0; j < restart_interval; j++)
						process_MCU(fi);
					/* proc till all EOB met */

					aux = get_next_MK(fi);
					if (!RST_MK(aux)) {
						fprintf(stderr, "%ld:\tERROR:\tLost Sync after interval!\n", ftell(fi));
						aborted_stream(fi);
					} else if (verbose)
						fprintf(stderr, "%ld:\tINFO:\tFound Restart Marker\n", ftell(fi));

					reset_prediction();
					clear_bits();
				}	/* intra-interval loop */
			} else
				leftover = mx_size * my_size;

			/* process till end of row without restarts */
			for (i = 0; i < leftover; i++)
				process_MCU(fi);

			in_frame = 0;
			break;

		case EOI_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tFound the EOI marker!\n", ftell(fi));
			if (in_frame)
				aborted_stream(fi);

			if (verbose)
				fprintf(stderr, "\tINFO:\tTotal skipped bytes %d, total stuffers %d\n", passed,
					stuffers);
			fclose(fi);

			write_bmp(file2);

			free_structures();
			return 0;
			break;

		case COM_MK:
			if (verbose)
				fprintf(stderr, "%ld:\tINFO:\tSkipping comments\n", ftell(fi));
			skip_segment(fi);
			break;

		case EOF:
			if (verbose)
				fprintf(stderr, "%ld:\tERROR:\tRan out of input data!\n", ftell(fi));
			aborted_stream(fi);

		default:
			if ((mark & MK_MSK) == APP_MK) {
				if (verbose)
					fprintf(stderr, "%ld:\tINFO:\tSkipping application data\n", ftell(fi));
				skip_segment(fi);
				break;
			}
			if (RST_MK(mark)) {
				reset_prediction();
				break;
			}
			/* if all else has failed ... */
			fprintf(stderr, "%ld:\tWARNING:\tLost Sync outside scan, %d!\n", ftell(fi), mark);
			aborted_stream(fi);
			break;
		}		/* end switch */
	} while (1);

	return 0;
}

void write_bmp(const char *const file2)
{
	FILE *fpBMP;

	int i, j;

	// Header and 3 bytes per pixel
	unsigned long ulBitmapSize = ceil_div(24*x_size, 32)*4*y_size+54; 
	char ucaBitmapSize[4];

	ucaBitmapSize[3] = (ulBitmapSize & 0xFF000000) >> 24;
	ucaBitmapSize[2] = (ulBitmapSize & 0x00FF0000) >> 16;
	ucaBitmapSize[1] = (ulBitmapSize & 0x0000FF00) >> 8;
	ucaBitmapSize[0] = (ulBitmapSize & 0x000000FF);

	/* Create bitmap file */
	fpBMP = fopen(file2, "wb");
	if (fpBMP == 0)
		return;

	/* Write header */
	/* All values are in big endian order (LSB first) */

	// BMP signature + filesize
	fprintf(fpBMP, "%c%c%c%c%c%c%c%c%c%c", 66, 77, ucaBitmapSize[0],
		ucaBitmapSize[1], ucaBitmapSize[2], ucaBitmapSize[3], 0, 0, 0, 0);

	// Image offset, infoheader size, image width
	fprintf(fpBMP, "%c%c%c%c%c%c%c%c%c%c", 54, 0, 0, 0, 40, 0, 0, 0, (x_size & 0x00FF), (x_size & 0xFF00) >> 8);

	// Image height, number of panels, num bits per pixel
	fprintf(fpBMP, "%c%c%c%c%c%c%c%c%c%c", 0, 0, (y_size & 0x00FF), (y_size & 0xFF00) >> 8, 0, 0, 1, 0, 24, 0);

	// Compression type 0, Size of image in bytes 0 because uncompressed
	fprintf(fpBMP, "%c%c%c%c%c%c%c%c%c%c", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	fprintf(fpBMP, "%c%c%c%c%c%c%c%c%c%c", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	fprintf(fpBMP, "%c%c%c%c", 0, 0, 0, 0);

	for (i = y_size - 1; i >= 0; i--) {
		/* in bitmaps the bottom line of the image is at the
		   beginning of the file */
		for (j = 0; j < x_size; j++) {
			putc(FrameBuffer[3 * (i * x_size + j) + 0], fpBMP);
			putc(FrameBuffer[3 * (i * x_size + j) + 1], fpBMP);
			putc(FrameBuffer[3 * (i * x_size + j) + 2], fpBMP);
		}
		for (j = 0; j < x_size % 4; j++)
			putc(0, fpBMP);
	}

	fclose(fpBMP);
}
