/*-----------------------------------------*/
/* Based on parse.c by Pierre Guerrier     */
/*-----------------------------------------*/

#include "mSpinJPEGdec.h"
/*----------------------------------------------------------------*/
/* find next marker of any type, returns it, positions just after */
/* EOF instead of marker if end of file met while searching ...	  */
/*----------------------------------------------------------------*/

static unsigned char bit_count;	/* available bits in the window */
static unsigned char window;

unsigned long get_bits(FILE_t *fi, int number)
{
  int i, newbit;
  unsigned long result = 0;
  uchar aux, wwindow;

  if (!number)
    return 0;

  for (i = 0; i < number; i++) {
    if (bit_count == 0) {
      wwindow = fgetc(fi);

      if (wwindow == 0xFF)
        switch (aux = fgetc(fi)) {	/* skip stuffer 0 byte */
			//case (uchar)EOF:
            case 0xFF:
#if(DEBUG_MODE>0)
                io_printf(IO_BUF, "[ERROR] Ran out of bit stream\n");
#endif
				aborted_stream();
                break;

            case 0x00:
                stuffers++;
                break;

            default:
#if(DEBUG_MODE>0)
                if (RST_MK(0xFF00 | aux))
					io_printf(IO_BUF, "[ERROR] Spontaneously found restart at %d!\n", fi->nCharRead);
                io_printf(IO_BUF, "[ERROR] Lost sync in bit stream\n");
#endif
				aborted_stream();
                break;
        }

      bit_count = 8;
    }
    else wwindow = window;
    newbit = (wwindow>>7) & 1;
    window = wwindow << 1;
    bit_count--;
    result = (result << 1) | newbit;
  }
  return result;
}


uint get_next_MK(FILE_t *fi)
{
    unsigned int c;
    int ffmet = 0;
    int locpassed = -1;

    passed--;	/* as we fetch one anyway */

    while ((c = fgetc(fi)) != (unsigned int) EOF) {
		switch (c) {
        case 0xFF:
            ffmet = 1;
            break;
        case 0x00:
            ffmet = 0;
            break;
        default:
#if(DEBUG_MODE>2)
			if (locpassed > 1)
                io_printf(IO_BUF, "[NOTE] passed %d bytes\n", locpassed);
#endif
			if (ffmet) {
#if(DEBUG_MODE>2)
				io_printf(IO_BUF, "get_next_MK() returning 0x%x\n", (0xFF00 | c));
#endif
				return (0xFF00 | c);
			}
            ffmet = 0;
            break;
        }
        locpassed++;
        passed++;
    }

    return (unsigned int) EOF;
}

uint get_size(FILE_t *fi)
{
	uint aux;

	aux = (uint)fgetc(fi);
    return (aux << 8) | fgetc(fi);	/* big endian */
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
            MCU_buff[k] = (PBlock *) sark_alloc(1, sizeof(PBlock));
            if (MCU_buff[k] == NULL) {
#if(DEBUG_MODE>0)
                io_printf(IO_BUF, "[ERROR] Could not allocate MCU buffers!\n");
#endif
                return -1;
            }
            k++;
            if (k == 10) {
#if(DEBUG_MODE>0)
                io_printf(IO_BUF, "[ERROR] Max subsampling exceeded!\n");
#endif
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

    mx_size = ceil_div(x_size,MCU_sx);
    my_size = ceil_div(y_size,MCU_sy);
    rx_size = MCU_sx * floor_div(x_size,MCU_sx);
    ry_size = MCU_sy * floor_div(y_size,MCU_sy);

    return 0;
}


/*----------------------------------------------------------*/
/* loading and allocating of quantization table             */
/* table elements are in ZZ order (same as unpack output)   */
/*----------------------------------------------------------*/

int load_quant_tables(FILE_t *fi)
{
  char aux;
  unsigned int size, n, i, id, x;

  size = get_size(fi); /* this is the tables' size */
  n = (size - 2) / 65;

  for (i = 0; i < n; i++) {
    aux = fgetc(fi);
    if (first_quad(aux) > 0) {
#if(DEBUG_MODE>0)
      io_printf(IO_BUF, "[ERROR] Bad QTable precision!\n");
#endif
      return -1;
    }
    id = second_quad(aux);
#if(DEBUG_MODE>0)
      io_printf(IO_BUF, "[INFO] Loading table %d\n", id);
#endif
    QTable[id] = (PBlock *) sark_alloc(1, sizeof(PBlock));
    if (QTable[id] == NULL) {
#if(DEBUG_MODE>0)
      io_printf(IO_BUF, "[ERROR] Could not allocate table storage!\n");
#endif
	  aborted_stream();
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


void skip_segment(FILE_t *fi)	/* skip a segment we don't want */
{
  uint size;
  char	tag[5];
  int i;

  size = get_size(fi);	// read two bytes from fi which signifies
						// length of segment excluding the marker
  if (size > 5) {
    for (i = 0; i < 4; i++)
	  tag[i] = (char)fgetc(fi); // read four characters
    tag[4] = '\0';
#if(DEBUG_MODE>0)
      io_printf(IO_BUF, "[INFO] Tag is %s\n", tag);
#endif
	size -= 4;		    // because 4 characters have been read
  }
  fseek(fi, size-2, SEEK_CUR);	// because 2 bytes is used for "size" tag
}

void clear_bits(void)
{
  bit_count = 0;
}


/*----------------------------------------------------------*/
/* this takes care for processing all the blocks in one MCU */
/*----------------------------------------------------------*/

int process_MCU(FILE_t *fi)
{
  int  i;
  long offset;
  int  goodrows, goodcolumns;

  if (MCU_column == mx_size) {
    MCU_column = 0;
    MCU_row++;
    if (MCU_row == my_size) {
      in_frame = 0;
      return 0;
    }
#if(DEBUG_MODE>0)
    io_printf(IO_BUF, "[INFO] Processing stripe %d/%d\n", MCU_row+1, my_size);
#endif
  }

  for (curcomp = 0; MCU_valid[curcomp] != -1; curcomp++) {
    unpack_block(fi, FBuff, MCU_valid[curcomp]); /* pass index to HT,QT,pred */
    IDCT(FBuff, MCU_buff[curcomp]);
  }

  /* YCrCb to RGB color space transform here */
  if (n_comp > 1)
    color_conversion();
  else
    //memmove(ColorBuffer, MCU_buff[0], 64);
    sark_mem_cpy(ColorBuffer, MCU_buff[0], 64);

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
    /*
    memmove(FrameBuffer + offset + n_comp * i * x_size,
        ColorBuffer + n_comp * i * MCU_sx,
        n_comp * goodcolumns);
    */
    sark_mem_cpy(FrameBuffer + offset + n_comp * i * x_size,
                 ColorBuffer + n_comp * i * MCU_sx, n_comp * goodcolumns);
  MCU_column++;
  return 1;
}

uchar get_one_bit(FILE_t *fi)
{
  int newbit;
  uchar aux, wwindow;

  if (bit_count == 0) {
    wwindow = fgetc(fi);

    if (wwindow == 0xFF)
      switch (aux = fgetc(fi)) {	/* skip stuffer 0 byte */
	  //case (uchar)EOF:
      case 0xFF:
#if(DEBUG_MODE>0)
        io_printf(IO_BUF, "[ERROR] Ran out of bit stream\n");
#endif
		aborted_stream();
        break;

      case 0x00:
        stuffers++;
        break;

      default:
#if(DEBUG_MODE>0)
        if (RST_MK(0xFF00 | aux))
            io_printf(IO_BUF, "[ERROR] Spontaneously found restart!\n");
        io_printf(IO_BUF, "[ERROR] Lost sync in bit stream\n");
#endif
		aborted_stream();
        break;
      }

    bit_count = 8;
  }
  else
    wwindow = window;

  newbit = (wwindow >> 7) & 1;
  window = wwindow << 1;
  bit_count--;
  return newbit;
}
