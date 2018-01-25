#include "mSpinJPEGdec.h"

/*-------------------------- DECODER MAIN LOOP --------------------------*/
/*-----------------------------------------------------------------------*/
/* Based on jpeg.c, main for jfif decoder by Pierre Guerrier
 * decode() will be called when the first chunk has arrived.
 * See hDMA() and hUEvent()
 */

/*--- Local/private function prototypes ---*/
static void get_SOF(FILE_t *fi);
static void get_DHT(FILE_t *fi);
static void get_DQT(FILE_t *fi);
static void get_DRI(FILE_t *fi, int *ri);
static void get_SOS(FILE_t *fi, int ri);
static void get_EOI(FILE_t *fi);

void decode(uint arg0, uint arg1)
{

	uint aux, mark;
	int restart_interval; /* RST check */
	int i,j;

	decIsStarted = true;
#if(DEBUG_MODE>0)
	io_printf(IO_STD, "[INFO] Start decoding!\n");
#endif

	// at this point, the sdramImgBuf is already created

	/* First find the SOI marker: */
	aux = get_next_MK(sdramImgBuf);

	if (aux != SOI_MK) {
#if(DEBUG_MODE>0)
		io_printf(IO_BUF, "[ERROR] cannot detect SOI\n");
#endif
		aborted_stream();
		return;
	}
#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Found the SOI marker at %d!\n", sdramImgBuf->nCharRead);
#endif

	in_frame = 0;	// are we in frame processing yet?
	restart_interval = 0;
	for (i = 0; i < 4; i++)
	  QTvalid[i] = 0;

	ENABLE_TIMER ();	// Enable timer (once)
	START_TIMER ();	// Start measuring

	/* Now process segments as they appear: */
	do {
		mark = get_next_MK(sdramImgBuf);
		switch (mark) {
		case SOF_MK: /* start of frame marker */
			get_SOF(sdramImgBuf); break;
		case DHT_MK: /* Define Huffman Table */
			get_DHT(sdramImgBuf); break;
		case DQT_MK: /* Define Quantization Table */
			get_DQT(sdramImgBuf); break;
		case DRI_MK: /* Define Restart Interval */
			get_DRI(sdramImgBuf, &restart_interval); break;
		case SOS_MK: /* start of scan */
			get_SOS(sdramImgBuf, restart_interval); break;
		case EOI_MK: /* end of image is encountered */
			get_EOI(sdramImgBuf); break;
		case COM_MK: /* comment segment is encountered */
#if(DEBUG_MODE>0)
			io_printf(IO_BUF, "[INFO] Skipping comments\n");
#endif
			skip_segment(sdramImgBuf); break;
		case EOF: /* this should not be reached! */
#if(DEBUG_MODE>0)
			io_printf(IO_BUF, "[ERROR] Ran out of input data!\n");
#endif
			// Indar: why always Ran out ofinput data?
			// aborted_stream(); break;
			// Let's assume it is correct:
			get_EOI(sdramImgBuf); break;
		default:
			if ((mark & MK_MSK) == APP_MK) {
#if(DEBUG_MODE>0)
				io_printf(IO_BUF, "[INFO] Skipping application data\n");
#endif
				skip_segment(sdramImgBuf); break;
			}
			if (RST_MK(mark)) {
				reset_prediction(); break;
			}
			/* if all else has failed ... */
#if(DEBUG_MODE>0)
			io_printf(IO_BUF, "[WARNING] Lost Sync outside scan, %d!\n", mark);
#endif
			aborted_stream(); break;
		}
	}
	while (decIsStarted);

    if(success){
#if(DEBUG_MODE>0)
		io_printf(IO_STD, "Finish decoding in %u us\n", tmeas);
#endif
        reportTmeas(tmeas);
    }
}


/*--- Function implementation ---*/
void get_SOF(FILE_t *fi)
{
	uint aux, i;
#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Found the SOF marker at-%d!\n", fi->nCharRead-1);
#endif

	in_frame = 1;
	get_size(fi);	/* header size, don't care */
	fgetc(fi);	/* precision, 8bit, don't care */
	y_size = get_size(fi); x_size = get_size(fi); /* Video frame size ??? */
	n_comp = fgetc(fi);	/* # of components */

#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Image size is %d by %d\n", x_size, y_size);
	uchar clrStr[12];
	switch (n_comp){
	case 1: io_printf(clrStr, "Monochrome"); break;
	case 3: io_printf(clrStr, "Color"); break;
	default: io_printf(clrStr, "Not a"); break;
	}
	io_printf(IO_BUF, "[INFO] %s JPEG image!\n", clrStr);
#endif

	for (i = 0; i < n_comp; i++) {
		/* component specifiers */
		comp[i].CID = fgetc(fi);
		aux = fgetc(fi);
		comp[i].HS = first_quad(aux);
		comp[i].VS = second_quad(aux);
		comp[i].QT = fgetc(fi);
	}

#if(DEBUG_MODE>0)
	if (n_comp > 1)
		io_printf(IO_BUF, "[INFO] Color format is %d:%d:%d, H=%d\n",
							comp[0].HS * comp[0].VS,
							comp[1].HS * comp[1].VS,
							comp[2].HS * comp[2].VS,
							comp[1].HS);
#endif

	if (init_MCU() == -1) {
#if(DEBUG_MODE>0)
		io_printf(IO_BUF, "[ERROR] Cannot init MCUs\n");
#endif
		aborted_stream();
	}
	/* dimension scan buffer for YUV->RGB conversion */
	uint sz;
	sz = x_size * y_size * n_comp;
	FrameBuffer = (uchar *)sark_xalloc(sv->sdram_heap, sz, 0, ALLOC_LOCK);
	sz = MCU_sx * MCU_sy * n_comp;
	ColorBuffer = (uchar *)sark_xalloc(sv->sdram_heap, sz, 0, ALLOC_LOCK);
	FBuff = (FBlock *) sark_alloc(1, sizeof(FBlock));
	PBuff = (PBlock *) sark_alloc(1, sizeof(PBlock));

	if ((FrameBuffer == NULL) || (ColorBuffer == NULL) ||
		(FBuff == NULL) || (PBuff == NULL) ) {
#if(DEBUG_MODE>0)
			io_printf(IO_BUF, "[ERROR] Could not allocate pixel storage!\n");
#endif
			aborted_stream();
	}
}

void get_DHT(FILE_t *fi)
{
#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Defining Huffman Tables found at-%d\n", fi->nCharRead);
#endif
	if (load_huff_tables(fi) == -1) {
#if(DEBUG_MODE>0)
		io_printf(IO_BUF, "[ERROR] Cannot load Huffman Table\n");
#endif
		aborted_stream();
	}
}

void get_DQT(FILE_t *fi)
{
#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Defining Quantization Tables at-%d\n", fi->nCharRead);
#endif
	if (load_quant_tables(fi) == -1) {
#if(DEBUG_MODE>0)
		io_printf(IO_BUF, "[ERROR] Cannot load Quantization Table\n");
#endif
		aborted_stream();
	}
}

void get_DRI(FILE_t *fi, int *ri)
{
	get_size(fi);	/* skip size */
	*ri = get_size(fi);
#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Defining Restart Interval %d\n", *ri);
#endif
}

void get_SOS(FILE_t *fi, int restart_interval)
{
	uint aux;
	int n_restarts, leftover; /* RST check */
	int i,j;

#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Found the SOS marker at-%d!\n", fi->nCharRead);
#endif
	get_size(fi); /* don't care */
	aux = fgetc(fi);
	if (aux != (unsigned int) n_comp) {
#if(DEBUG_MODE>0)
		io_printf(IO_BUF, "[ERROR] Bad component interleaving!\n");
#endif
		aborted_stream();
	}

	for (i = 0; i < n_comp; i++) {
		aux = fgetc(fi);
		if (aux != comp[i].CID) {
#if(DEBUG_MODE>0)
			io_printf(IO_BUF, "[ERROR] Bad Component Order!\n");
#endif
			aborted_stream();
		}
		aux = fgetc(fi);
		comp[i].DC_HT = first_quad(aux);
		comp[i].AC_HT = second_quad(aux);
	}
	get_size(fi); fgetc(fi);	/* skip things */

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
#if(DEBUG_MODE>0)
				io_printf(IO_BUF, "[ERROR] Lost Sync after interval!\n");
#endif
				aborted_stream();
			}
#if(DEBUG_MODE>0)
			else io_printf(IO_BUF, "[INFO] Found Restart Marker at-%d\n", fi->nCharRead);
#endif

			reset_prediction();
			clear_bits();
		}		/* intra-interval loop */
	}
	else
		leftover = mx_size * my_size;

	  /* process till end of row without restarts */
	for (i = 0; i < leftover; i++)
		process_MCU(fi);

	in_frame = 0;
}

void get_EOI(FILE_t *fi)
{
	tmeas = READ_TIMER (); // Get elapsed time

#if(DEBUG_MODE>0)
	io_printf(IO_BUF, "[INFO] Found the EOI marker at-%d!\n", fi->nCharRead);
#endif
	if (in_frame) aborted_stream();

	//Indar: previous version:
	//aborted_stream(ON_FINISH);
	decIsStarted = false;
	success = true;
	emitDecodeDone();

	free_structures();
	// if closeImgBufAfterDecoding is active, then release the buffers
	if(closeImgBufAfterDecoding) {
		closeImgBuf();
	}
}

