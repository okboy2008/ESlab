#include <comik.h>
#include <global_memmap.h>
#include <math.h>
#include <5kk03-utils.h>
#include <jpeg.h>
#include <surfer.jpg.h>

#define min(a,b) ((a)>(b))?(b):(a)
#define max(a,b) ((a)>(b))?(a):(b)

unsigned char FrameBuffer1[2304];
unsigned char ColorBuffer1[192];
FBlock FBuff1;
PBlock PBuff1;
FBlock* FBuff;
PBlock* PBuff;

unsigned char* FrameBuffer;
unsigned char* ColorBuffer;

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
		mk_mon_debug_info("found SOI marker");
		// fprintf(stderr, "%ld:\tINFO:\tFound the SOI marker!\n", counter);
	in_frame = 0;
	restart_interval = 0;
	for (i = 0; i < 4; i++)
		QTvalid[i] = 0;

	/* Now process segments as they appear: */
	do {
		mark = get_next_MK2(&fi,&counter);
		mk_mon_debug_info("mark:");
		mk_mon_debug_info(mark);
		// printf("1: %x, %d\n",mark,counter);

		switch (mark) {
		case SOF_MK:
			if (verbose)
				mk_mon_debug_info("found SOF marker");
				// fprintf(stderr, "%ld:\tINFO:\tFound the SOF marker!\n", counter);
			in_frame = 1;
			get_size2(&fi,&counter);	/* header size, don't care */

			/* load basic image parameters */
			//fgetc(fi);	/* precision, 8bit, don't care */
			GETCHAR(&fi,&counter);
			y_size = get_size2(&fi,&counter);
			x_size = get_size2(&fi,&counter);
			if (verbose){
				mk_mon_debug_info("Image size is: x,y");
				mk_mon_debug_info(x_size);
				mk_mon_debug_info(y_size);
			}
				// fprintf(stderr, "\tINFO:\tImage size is %d by %d\n", x_size, y_size);

			//n_comp = fgetc(fi);	/* # of components */
			n_comp=GETCHAR(&fi,&counter);
			if (verbose) {
				mk_mon_debug_info("INFO");
				// fprintf(stderr, "\tINFO:\t");
				switch (n_comp) {
				case 1:
					mk_mon_debug_info("Monochrome");
					// fprintf(stderr, "Monochrome");
					break;
				case 3:
					mk_mon_debug_info("Color");
					// fprintf(stderr, "Color");
					break;
				default:
					mk_mon_debug_info("Not a");
					// fprintf(stderr, "Not a");
					break;
				}
				mk_mon_debug_info("JPEG image");
				// fprintf(stderr, " JPEG image!\n");
			}

			for (i = 0; i < n_comp; i++) {
				/* component specifiers */
				comp[i].CID = GETCHAR(&fi,&counter);
				aux = GETCHAR(&fi,&counter);
				comp[i].HS = first_quad(aux);
				comp[i].VS = second_quad(aux);
				comp[i].QT = GETCHAR(&fi,&counter);
			}
			if ((n_comp > 1) && verbose){
				mk_mon_debug_info("Color format is ...");
			}
				// fprintf(stderr,
					// "\tINFO:\tColor format is %d:%d:%d, H=%d\n",
					// comp[0].HS * comp[0].VS, comp[1].HS * comp[1].VS, comp[2].HS * comp[2].VS,
					// comp[1].HS);

			if (init_MCU() == -1)
				return -1;

			/* dimension scan buffer for YUV->RGB conversion */
			FrameBuffer=FrameBuffer1;
			ColorBuffer=ColorBuffer1;
			FBuff=&FBuff1;
			PBuff=&PBuff1;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
			// FrameBuffer = (unsigned char *)malloc((size_t) x_size * y_size * n_comp);
			// ColorBuffer = (unsigned char *)malloc((size_t) MCU_sx * MCU_sy * n_comp);
			// FBuff = (FBlock *) malloc(sizeof(FBlock));
			// PBuff = (PBlock *) malloc(sizeof(PBlock));
			// need replacement
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
			break;

		case DHT_MK:
			if (verbose)
				mk_mon_debug_info("INFO: Defining Huffman Tables");
				// fprintf(stderr, "%ld:\tINFO:\tDefining Huffman Tables\n", counter);
			if (load_huff_tables2(&fi,&counter) == -1)
				return -1;
			break;

		case DQT_MK:
			if (verbose)
				mk_mon_debug_info("INFO: Defining Quantization Tables");
				// fprintf(stderr, "%ld:\tINFO:\tDefining Quantization Tables\n", counter);
			if (load_quant_tables2(&fi,&counter) == -1)
				return -1;
			break;

		case DRI_MK:
//			get_size(fi);	/* skip size */
			get_size2(&fi,&counter);
			restart_interval = get_size2(&fi,&counter);
			if (verbose)
				mk_mon_debug_info("INFO: Defining Restart Interval");
				// fprintf(stderr, "%ld:\tINFO:\tDefining Restart Interval %d\n", counter,
					// restart_interval);
			break;

		case SOS_MK:	/* lots of things to do here */
			if (verbose)
				mk_mon_debug_info("INFO: Found the SOS marker");
				// fprintf(stderr, "%ld:\tINFO:\tFound the SOS marker!\n", counter);
			get_size2(&fi,&counter);	/* don't care */
			aux = GETCHAR(&fi,&counter);
			if (aux != (unsigned int)n_comp) {
				mk_mon_debug_info("Bad component interleaving!");
				// fprintf(stderr, "\tERROR:\tBad component interleaving!\n");
				return -1;
			}

			for (i = 0; i < n_comp; i++) {
				aux = GETCHAR(&fi,&counter);
				if (aux != comp[i].CID) {
					mk_mon_debug_info("Bad Component Order");
					// fprintf(stderr, "\tERROR:\tBad Component Order!\n");
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


int main(int argc, char**argv)
{
	TIME start,stop,diff;

	volatile char    *image       = (char*)surfer_jpg;
	volatile char    *image1       = (char*)(shared_pt_REMOTEADDR+4*1024*1024);
	// volatile pixel_fb *FrameBuffer = (pixel_fb *)shared_pt_REMOTEADDR; 
	// volatile float* edgeBuffer = (float *)(shared_pt_REMOTEADDR+8*1024*1024);

	// Sync with the monitor.
	mk_mon_sync();
	// Enable stack checking.
	start_stack_check();

	// Paint framebuffer.
	// for(i = 0; i < 1024*768/8*3; i++)FrameBuffer[i].value = 0x00000000;

	// Get current time.
	start = hw_tifu_systimer_get();
	int counter=0;
	int i;
	unsigned int c,c1;
	for(i=0;i<30;i++){
		mk_mon_debug_info(i);
		c=GETCHAR(&image,&counter);
		// c1=getChar(&image1,&counter);
		mk_mon_debug_info(c);
		// mk_mon_debug_info(c1);
	}



	stop = hw_tifu_systimer_get();
	diff = stop - start;
	mk_mon_debug_info(LO_64(diff));

	// Signal the monitor we are done.
	mk_mon_debug_tile_finished();
	return i;
}
