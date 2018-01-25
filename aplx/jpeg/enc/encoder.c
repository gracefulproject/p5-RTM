/* For encoding using jpec, it involves three main steps:
 * 1. create jpec object      --> jpec_enc_new()
 * 2. call run                --> jpec_enc_run()
 * 3. release/free the object --> jpec_enc_ded()
 *
 * */
#include "mSpinJPEGenc.h"

/************************** from enc.c ***************************/
/*-------------------- Three main encoder functions--------------*/
/* img is a buffer that should be allocated by mSpinJPEGdec (within
 * hSDP().
 * */
jpec_enc_t *jpec_enc_new(const uchar *img, ushort w, ushort h, int q)
{
  jpec_enc_t *e = sark_alloc(1, sizeof(*e));
  e->img = img;
  e->w = w;
  e->w8 = (((w-1)>>3)+1)<<3;
  e->h = h;
  e->qual = q;
  e->bmax = (((w-1)>>3)+1) * (((h-1)>>3)+1);
  e->bnum = -1;
  e->bx = -1;
  e->by = -1;
  int bsiz = JPEC_ENC_HEAD_SIZ + e->bmax * JPEC_ENC_BLOCK_SIZ;
  /* Note, for vga image 640x480:
   *       bmax for 640x480 is 4800
   *       Thus, bsiz = 144330
   */
  e->buf = jpec_buffer_new(bsiz);
  e->hskel = sark_alloc(1, sizeof(*e->hskel));
#if(DEBUG_MODE>2)
  io_printf(IO_BUF, "[jpec_enc_t] Creating jpec_enc object for image %dx%d with bsiz=%d\n",
			e->w, e->h, bsiz);
#endif
  return e;
}

uchar *jpec_enc_run(jpec_enc_t *e, int *len)
{
#if(DEBUG_MODE>=2)
  io_printf(IO_STD, "[jpec_enc_run] Do jpg header\n");
#endif
  jpec_enc_open(e);
  int cntr = 0;

#ifndef FOR_PAPER_ICCES
  while (jpec_enc_next_block(e)) {
#else
  /* for ICCES:
   * 3556 : Elephant-Wallpapers-vga.ras 640 480
   * 5542 : Elephant-Wallpapers-svga.ras 800 600
   * 9097 : Elephant-Wallpapers-xga.ras 1024 768
   * */
  while (cntr<numProcBlock) { // for VGA resolution
#endif

#if(DEBUG_MODE>2)
    io_printf(IO_STD, "[jpec_enc_run] Processing dct on block\n");
#endif
	jpec_enc_block_dct(e);
#if(DEBUG_MODE>2)
    io_printf(IO_STD, "[jpec_enc_run] Processing quantization on block\n");
#endif
	jpec_enc_block_quant(e);
#if(DEBUG_MODE>2)
    io_printf(IO_STD, "[jpec_enc_run] Processing zz on block\n");
#endif
	jpec_enc_block_zz(e);
#if(DEBUG_MODE>2)
    io_printf(IO_STD, "[jpec_enc_run] Processing huffman on block\n");
#endif
	e->hskel->encode_block(e->hskel->opq, &e->block, e->buf);
#if(DEBUG_MODE>0)
    io_printf(IO_BUF, "cntr=%d\n",cntr);
#endif
    cntr++;
  }
  jpec_enc_close(e);
  *len = e->buf->len;
#if(DEBUG_MODE>=2)
  io_printf(IO_STD, "[jpec_enc_run] Output at 0x%x\n", e->buf->stream);
#endif
  return e->buf->stream;
}

void jpec_enc_del(jpec_enc_t *e)
{
  if (e->buf) jpec_buffer_del(e->buf);
  sark_free(e->hskel);
  sark_free(e);
}

/*----------------------------------------------------------------*/



/* Update the internal quantization matrix according to the asked quality */
 void jpec_enc_init_dqt(jpec_enc_t *e)
{
  float qualf = (float) e->qual;
  float scale = (e->qual < 50) ? (50/qualf) : (2 - qualf/50);
  for (int i = 0; i < 64; i++) {
	int a = (int) ((float) jpec_qzr[i]*scale + 0.5);
	a = (a < 1) ? 1 : ((a > 255) ? 255 : a);
	e->dqt[i] = a;
  }
}

 void jpec_enc_open(jpec_enc_t *e)
{
  jpec_huff_skel_init(e->hskel);
  jpec_enc_init_dqt(e);
  jpec_enc_write_soi(e);
  jpec_enc_write_app0(e);
  jpec_enc_write_dqt(e);
  jpec_enc_write_sof0(e);
  jpec_enc_write_dht(e);
  jpec_enc_write_sos(e);
}

 void jpec_enc_close(jpec_enc_t *e)
{
  e->hskel->del(e->hskel->opq);
  jpec_buffer_write_2bytes(e->buf, 0xFFD9); /* EOI marker */
}

 void jpec_enc_write_soi(jpec_enc_t *e)
{
  jpec_buffer_write_2bytes(e->buf, 0xFFD8); /* SOI marker */
}

 void jpec_enc_write_app0(jpec_enc_t *e)
{
  jpec_buffer_write_2bytes(e->buf, 0xFFE0); /* APP0 marker */
  jpec_buffer_write_2bytes(e->buf, 0x0010); /* segment length */
  jpec_buffer_write_byte(e->buf, 0x4A);     /* 'J' */
  jpec_buffer_write_byte(e->buf, 0x46);     /* 'F' */
  jpec_buffer_write_byte(e->buf, 0x49);     /* 'I' */
  jpec_buffer_write_byte(e->buf, 0x46);     /* 'F' */
  jpec_buffer_write_byte(e->buf, 0x00);     /* '\0' */
  jpec_buffer_write_2bytes(e->buf, 0x0101); /* v1.1 */
  jpec_buffer_write_byte(e->buf, 0x00);     /* no density unit */
  jpec_buffer_write_2bytes(e->buf, 0x0001); /* X density = 1 */
  jpec_buffer_write_2bytes(e->buf, 0x0001); /* Y density = 1 */
  jpec_buffer_write_byte(e->buf, 0x00);     /* thumbnail width = 0 */
  jpec_buffer_write_byte(e->buf, 0x00);     /* thumbnail height = 0 */
}

 void jpec_enc_write_dqt(jpec_enc_t *e)
{
  jpec_buffer_write_2bytes(e->buf, 0xFFDB); /* DQT marker */
  jpec_buffer_write_2bytes(e->buf, 0x0043); /* segment length */
  jpec_buffer_write_byte(e->buf, 0x00);     /* table 0, 8-bit precision (0) */
  for (int i = 0; i < 64; i++) {
	jpec_buffer_write_byte(e->buf, e->dqt[jpec_zz[i]]);
  }
}

 void jpec_enc_write_sof0(jpec_enc_t *e) {
  jpec_buffer_write_2bytes(e->buf, 0xFFC0); /* SOF0 marker */
  jpec_buffer_write_2bytes(e->buf, 0x000B); /* segment length */
  jpec_buffer_write_byte(e->buf, 0x08);     /* 8-bit precision */
  jpec_buffer_write_2bytes(e->buf, e->h);
  jpec_buffer_write_2bytes(e->buf, e->w);
  jpec_buffer_write_byte(e->buf, 0x01);     /* 1 component only (grayscale) */
  jpec_buffer_write_byte(e->buf, 0x01);     /* component ID = 1 */
  jpec_buffer_write_byte(e->buf, 0x11);     /* no subsampling */
  jpec_buffer_write_byte(e->buf, 0x00);     /* quantization table 0 */
}

 void jpec_enc_write_dht(jpec_enc_t *e)
{
  jpec_buffer_write_2bytes(e->buf, 0xFFC4);          /* DHT marker */
  jpec_buffer_write_2bytes(e->buf, 19 + jpec_dc_nb_vals); /* segment length */
  jpec_buffer_write_byte(e->buf, 0x00);              /* table 0 (DC), type 0 (0 = Y, 1 = UV) */
  for (int i = 0; i < 16; i++) {
	jpec_buffer_write_byte(e->buf, jpec_dc_nodes[i+1]);
  }
  for (int i = 0; i < jpec_dc_nb_vals; i++) {
	jpec_buffer_write_byte(e->buf, jpec_dc_vals[i]);
  }
  jpec_buffer_write_2bytes(e->buf, 0xFFC4);           /* DHT marker */
  jpec_buffer_write_2bytes(e->buf, 19 + jpec_ac_nb_vals);
  jpec_buffer_write_byte(e->buf, 0x10);               /* table 1 (AC), type 0 (0 = Y, 1 = UV) */
  for (int i = 0; i < 16; i++) {
	jpec_buffer_write_byte(e->buf, jpec_ac_nodes[i+1]);
  }
  for (int i = 0; i < jpec_ac_nb_vals; i++) {
	jpec_buffer_write_byte(e->buf, jpec_ac_vals[i]);
  }
}

 void jpec_enc_write_sos(jpec_enc_t *e)
{
  jpec_buffer_write_2bytes(e->buf, 0xFFDA); /* SOS marker */
  jpec_buffer_write_2bytes(e->buf, 8);      /* segment length */
  jpec_buffer_write_byte(e->buf, 0x01);     /* nb. components */
  jpec_buffer_write_byte(e->buf, 0x01);     /* Y component ID */
  jpec_buffer_write_byte(e->buf, 0x00);     /* Y HT = 0 */
  /* segment end */
  jpec_buffer_write_byte(e->buf, 0x00);
  jpec_buffer_write_byte(e->buf, 0x3F);
  jpec_buffer_write_byte(e->buf, 0x00);
}

 int jpec_enc_next_block(jpec_enc_t *e)
{
  int rv = (++e->bnum >= e->bmax) ? 0 : 1;
  if (rv) {
	e->bx =   (e->bnum << 3) % e->w8;
	e->by = ( (e->bnum << 3) / e->w8 ) << 3;
  }
  return rv;
}

 void jpec_enc_block_dct(jpec_enc_t *e)
{
#define JPEC_BLOCK(col,row) e->img[(((e->by + row) < e->h) ? e->by + row : e->h-1) * \
							e->w + (((e->bx + col) < e->w) ? e->bx + col : e->w-1)]
  const float* coeff = jpec_dct;
  float tmp[64];
  for (int row = 0; row < 8; row++) {
	/* NOTE: the shift by 256 allows resampling from [0 255] to [–128 127] */
	float s0 = (float) (JPEC_BLOCK(0, row) + JPEC_BLOCK(7, row) - 256);
	float s1 = (float) (JPEC_BLOCK(1, row) + JPEC_BLOCK(6, row) - 256);
	float s2 = (float) (JPEC_BLOCK(2, row) + JPEC_BLOCK(5, row) - 256);
	float s3 = (float) (JPEC_BLOCK(3, row) + JPEC_BLOCK(4, row) - 256);

	float d0 = (float) (JPEC_BLOCK(0, row) - JPEC_BLOCK(7, row));
	float d1 = (float) (JPEC_BLOCK(1, row) - JPEC_BLOCK(6, row));
	float d2 = (float) (JPEC_BLOCK(2, row) - JPEC_BLOCK(5, row));
	float d3 = (float) (JPEC_BLOCK(3, row) - JPEC_BLOCK(4, row));

	tmp[8 * row]     = coeff[3]*(s0+s1+s2+s3);
	tmp[8 * row + 1] = coeff[0]*d0+coeff[2]*d1+coeff[4]*d2+coeff[6]*d3;
	tmp[8 * row + 2] = coeff[1]*(s0-s3)+coeff[5]*(s1-s2);
	tmp[8 * row + 3] = coeff[2]*d0-coeff[6]*d1-coeff[0]*d2-coeff[4]*d3;
	tmp[8 * row + 4] = coeff[3]*(s0-s1-s2+s3);
	tmp[8 * row + 5] = coeff[4]*d0-coeff[0]*d1+coeff[6]*d2+coeff[2]*d3;
	tmp[8 * row + 6] = coeff[5]*(s0-s3)+coeff[1]*(s2-s1);
	tmp[8 * row + 7] = coeff[6]*d0-coeff[4]*d1+coeff[2]*d2-coeff[0]*d3;
  }
  for (int col = 0; col < 8; col++) {
	float s0 = tmp[     col] + tmp[56 + col];
	float s1 = tmp[ 8 + col] + tmp[48 + col];
	float s2 = tmp[16 + col] + tmp[40 + col];
	float s3 = tmp[24 + col] + tmp[32 + col];

	float d0 = tmp[     col] - tmp[56 + col];
	float d1 = tmp[ 8 + col] - tmp[48 + col];
	float d2 = tmp[16 + col] - tmp[40 + col];
	float d3 = tmp[24 + col] - tmp[32 + col];

	e->block.dct[     col] = coeff[3]*(s0+s1+s2+s3);
	e->block.dct[ 8 + col] = coeff[0]*d0+coeff[2]*d1+coeff[4]*d2+coeff[6]*d3;
	e->block.dct[16 + col] = coeff[1]*(s0-s3)+coeff[5]*(s1-s2);
	e->block.dct[24 + col] = coeff[2]*d0-coeff[6]*d1-coeff[0]*d2-coeff[4]*d3;
	e->block.dct[32 + col] = coeff[3]*(s0-s1-s2+s3);
	e->block.dct[40 + col] = coeff[4]*d0-coeff[0]*d1+coeff[6]*d2+coeff[2]*d3;
	e->block.dct[48 + col] = coeff[5]*(s0-s3)+coeff[1]*(s2-s1);
	e->block.dct[56 + col] = coeff[6]*d0-coeff[4]*d1+coeff[2]*d2-coeff[0]*d3;
  }
#undef JPEC_BLOCK
}

 void jpec_enc_block_quant(jpec_enc_t *e)
{
  //assert(e && e->bnum >= 0);
  for (int i = 0; i < 64; i++) {
	e->block.quant[i] = (int) (e->block.dct[i]/e->dqt[i]);
  }
}

 void jpec_enc_block_zz(jpec_enc_t *e)
{
  //assert(e && e->bnum >= 0);
  e->block.len = 0;
  for (int i = 0; i < 64; i++) {
	if ((e->block.zz[i] = e->block.quant[jpec_zz[i]])) e->block.len = i + 1;
  }
}





/************************** from buf.c ***************************/
/*---------------------------------------------------------------*/
/*
jpec_buffer_t *jpec_buffer_new(void) {
  return jpec_buffer_new2(-1);
}*/

//ori: jpec_buffer_t *jpec_buffer_new2(int siz) {
jpec_buffer_t *jpec_buffer_new(int siz)
{
  jpec_buffer_t *b = sark_alloc(1, sizeof(*b));
  b->stream = sark_xalloc(sv->sdram_heap, siz, 0, ALLOC_LOCK);
  if(b==NULL || b->stream==NULL) {
#if(DEBUG_MODE>2)
	  // io_printf(IO_STD, "[RTE] jpec_buffer_new(): Memory allocation error!\n");
#endif
	  rt_error(RTE_MALLOC);
  }
  else {
#if(DEBUG_MODE>2)
	  io_printf(IO_BUF, "[INFO] Output at 0x%x\n", b->stream);
#endif
  }
  b->siz = siz;
  b->len = 0;
  return b;
}

void jpec_buffer_del(jpec_buffer_t *b)
{
  if (b->stream) sark_free(b->stream);
  sark_free(b);
}

void jpec_buffer_write_byte(jpec_buffer_t *b, int val)
{
  if (b->siz == b->len) {
	int nsiz = (b->siz > 0) ? 2 * b->siz : JPEC_BUFFER_INIT_SIZ;
	// Sark doesn't have realloc
	/*
	void* tmp = realloc(b->stream, nsiz);
	b->stream = (uint8_t *) tmp;
	*/
	void *tmp = sark_alloc(1, nsiz);
	sark_mem_cpy(tmp, b->stream, b->siz);
	sark_free(b->stream);
	b->stream = (uchar *)tmp;
	b->siz = nsiz;
  }
  b->stream[b->len++] = (uchar) val;
}

void jpec_buffer_write_2bytes(jpec_buffer_t *b, int val)
{
  jpec_buffer_write_byte(b, (val >> 8) & 0xFF);
  jpec_buffer_write_byte(b, val & 0xFF);
}

