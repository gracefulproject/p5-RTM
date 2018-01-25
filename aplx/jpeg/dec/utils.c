#include "mSpinJPEGdec.h"

/* We emulate the C library function int fgetc(FILE *stream) to get the next character (an unsigned char)
 * from the specified stream and advances the position indicator for the stream.
 * Here, stream is a buffer in sdram. It checks the size of JPEG file in sdram and raise EOF error if
 * the last character from the buffers has already been read.
 */
int fgetc(FILE_t *fObject)
{
    uchar c;
	if(fObject->nCharRead < fObject->szFile) {
		sark_mem_cpy((void *)&c, (void *)fObject->ptrRead, 1);
		fObject->ptrRead++;    // advance one character
		fObject->nCharRead++;
#if(DEBUG_MODE>2)
		io_printf(IO_BUF, "in fgetc() char-c = 0x%x\n", c);
#endif
        return (int)c;
    }
    else {
        return EOF;
    }
}

int fseek(FILE_t *fObject, int offset, int whence)
{
	// here we don't need to implement all whence type
	// and we deal only with ptrRead, not ptrWrite
	if(whence == SEEK_CUR) {
		fObject->nCharRead += offset;
		fObject->ptrRead += offset;
	}
	else if(whence == SEEK_SET) {
		fObject->ptrRead = fObject->stream;
		fObject->ptrRead += offset;
		fObject->nCharRead = offset;
	}
}

/* Returns ceil(N/D). */
int ceil_div(int N, int D)
{
  int i = N/D;
  if (N > D*i) i++;
  return i;
}


/* Returns floor(N/D). */
int floor_div(int N, int D)
{
  int i = N/D;
  if (N < D*i) i--;
  return i;
}


/* For all components reset DC prediction value to 0. */
void reset_prediction(void)
{
  int i;
  for (i=0; i<3; i++) comp[i].PRED = 0;
}

/* Transform JPEG number format into usual 2's-complement format. */
int reformat(unsigned long S, int good)
{
  int St;

  if (!good)
    return 0;
  St = 1 << (good-1);	/* 2^(good-1) */
  if (S < (unsigned long) St)
    return (S+1+((-1) << good));
  else
    return S;
}

/* Ensure number is >=0 and <=255			   */
#define Saturate(n)	((n) > 0 ? ((n) < 255 ? (n) : 255) : 0)

void color_conversion(void)
{
  int  i, j;
  unsigned char y,cb,cr;
  signed char rcb, rcr;
  long r,g,b;
  long offset;

  for (i = 0; i < MCU_sy; i++)   /* pixel rows */
    {
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

      for (j = 0; j < MCU_sx; j++)   /* pixel columns */
    {
      int jp_0 = j >> comp[0].HDIV;
      int jp_1 = j >> comp[1].HDIV;
      int jp_2 = j >> comp[2].HDIV;

      y  = MCU_buff[inv_ndx_0 + (jp_0 >> 3)]->block[ip_0_lsbs][jp_0 & 7];
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

// For debugging: dump the content of retrieved jpg data
void dumpJPG()
{
	io_printf(IO_STD, "Dumping JPG content...\n");
	int c,r=0;
	while ((c = fgetc(sdramImgBuf)) != EOF) {
		if(r>=16) {
			r = 0;
			io_printf(IO_STD, "\n%02x ", c);
			r++;
		} else {
			io_printf(IO_STD, "%02x ", c);
			r++;
		}
	}
	io_printf(IO_STD, "\n");
	fseek(sdramImgBuf, 0, SEEK_SET);
}

