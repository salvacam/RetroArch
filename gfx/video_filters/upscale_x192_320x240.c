/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2018 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Compile: gcc -o upscale_x192_320x240.so -shared upscale_x192_320x240.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation upscale_x192_320x240_get_implementation
#define softfilter_thread_data upscale_x192_320x240_softfilter_thread_data
#define filter_data upscale_x192_320x240_filter_data
#endif

typedef struct
{
   void (*upscale_x192_320x240)(
         uint16_t *dst, const uint16_t *src,
         uint16_t dst_stride, uint16_t src_stride);
} upscale_function_t;

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned colfmt;
   unsigned width;
   unsigned height;
   int first;
   int last;
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   upscale_function_t function;
};

/*******************************************************************
 * Approximately bilinear scaler, 248x192 to 320x240
 * Copyright (C) 2014 hi-ban, Nebuleon <nebuleon.fumika@gmail.com>
 * (Optimisations by jdgleaver)
 *******************************************************************/

#define UPSCALE_192__WEIGHT(A, B, out, tmp) \
   *(out) = ((A + B + ((A ^ B) & 0x821)) >> 1)

/* Upscales a 256x192 image to 320x240 using an approximate bilinear
 * resampling algorithm that only uses integer math */
void upscale_256x192_to_320x240(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{
   /* There are 80 blocks of 3 pixels horizontally,
    * and 80 blocks of 2 pixels vertically
    * Each block of 3x2 becomes 4x3 */
   uint32_t block_x;
   uint32_t block_y;

   for (block_y = 0; block_y < 48; block_y++) 
   { 
      const uint16_t *block_src = src + block_y * src_stride * 4;
      uint16_t *block_dst       = dst + block_y * dst_stride * 5;

      for (block_x = 0; block_x < 64; block_x++)
      {
         const uint16_t *block_src_ptr = block_src;
         uint16_t *block_dst_ptr       = block_dst;

         uint16_t  _1,  _2,  _3,  _4,
                   _5,  _6,  _7,  _8,
                   _9, _10, _11, _12,
                  _13, _14, _15, _16;

         uint16_t   _1_2;
         uint16_t   _2_3;
         uint16_t   _3_4;

         uint16_t   _5_6;
         uint16_t   _6_7;
         uint16_t   _7_8;

         uint16_t  _9_10;
         uint16_t _10_11;
         uint16_t _11_12;

         uint16_t _13_14;
         uint16_t _14_15;
         uint16_t _15_16;

         uint16_t tmp;

         /* Horizontally:
          * Before(3):
          * (a)(b)(c)(d)
          * After(4):
          * (a)(ab)(bc)(cd)(d)
          *
          * Vertically:
          * Before(4): After(5):
          * (a)        (a)
          * (b)        (ab)
          * (c)        (bc)
          * (d)        (cd)
          *            (d)
          */

         /* -- Row 1 -- */
         _1 = *(block_src_ptr    );
         _2 = *(block_src_ptr + 1);
         _3 = *(block_src_ptr + 2);
         _4 = *(block_src_ptr + 3);

         *(block_dst_ptr    ) = _1;
         UPSCALE_192__WEIGHT(_1, _2, &_1_2, tmp);
         *(block_dst_ptr + 1) = _1_2;
         UPSCALE_192__WEIGHT(_2, _3, &_2_3, tmp);
         *(block_dst_ptr + 2) = _2_3;
         UPSCALE_192__WEIGHT(_3, _4, &_3_4, tmp);
         *(block_dst_ptr + 3) = _3_4;
         *(block_dst_ptr + 4) = _4;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 2 -- */
         _5 = *(block_src_ptr    );
         _6 = *(block_src_ptr + 1);
         _7 = *(block_src_ptr + 2);
         _8 = *(block_src_ptr + 3);

         UPSCALE_192__WEIGHT(_1, _5, block_dst_ptr, tmp);
         UPSCALE_192__WEIGHT(_5, _6, &_5_6, tmp);
         UPSCALE_192__WEIGHT(_1_2, _5_6, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_6, _7, &_6_7, tmp);
         UPSCALE_192__WEIGHT(_2_3, _6_7, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_7, _8, &_7_8, tmp);
         UPSCALE_192__WEIGHT(_3_4, _7_8, block_dst_ptr + 3, tmp);
         UPSCALE_192__WEIGHT(_4, _8, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 3 -- */
          _9 = *(block_src_ptr    );
         _10 = *(block_src_ptr + 1);
         _11 = *(block_src_ptr + 2);
         _12 = *(block_src_ptr + 3);


         UPSCALE_192__WEIGHT(_5, _9, block_dst_ptr, tmp);
         UPSCALE_192__WEIGHT(_9, _10, &_9_10, tmp);
         UPSCALE_192__WEIGHT(_5_6, _9_10, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_10, _11, &_10_11, tmp);
         UPSCALE_192__WEIGHT(_6_7, _10_11, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_11, _12, &_11_12, tmp);
         UPSCALE_192__WEIGHT(_7_8, _11_12, block_dst_ptr + 3, tmp);
         UPSCALE_192__WEIGHT(_8, _12, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 4 -- */
         _13 = *(block_src_ptr    );
         _14 = *(block_src_ptr + 1);
         _15 = *(block_src_ptr + 2);
         _16 = *(block_src_ptr + 3);


         UPSCALE_192__WEIGHT(_9, _13, block_dst_ptr, tmp);
         UPSCALE_192__WEIGHT(_13, _14, &_13_14, tmp);
         UPSCALE_192__WEIGHT(_9_10, _13_14, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_14, _15, &_14_15, tmp);
         UPSCALE_192__WEIGHT(_10_11, _14_15, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_15, _16, &_15_16, tmp);
         UPSCALE_192__WEIGHT(_11_12, _15_16, block_dst_ptr + 3, tmp);
         UPSCALE_192__WEIGHT(_12, _16, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 5 -- */
         *(block_dst_ptr    ) = _13;
         UPSCALE_192__WEIGHT(_13, _14, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_14, _15, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_15, _16, block_dst_ptr + 3, tmp);
         *(block_dst_ptr + 4) = _16;

         block_src += 4;
         block_dst += 5;
      }
   }
}

/* Upscales a 248x192 image to 310x240 (padding the result
 * to 320x240 via letterboxing) using an approximate bilinear
 * resampling algorithm that only uses integer math */
void upscale_248x192_to_320x240(uint16_t *dst, const uint16_t *src,
      uint16_t dst_stride, uint16_t src_stride)
{
   /* There are 80 blocks of 3 pixels horizontally,
    * and 80 blocks of 2 pixels vertically
    * Each block of 3x2 becomes 4x3 */
   uint32_t block_x;
   uint32_t block_y;

   for (block_y = 0; block_y < 48; block_y++) 
   { 

      memset(dst, 0, sizeof(uint16_t) * src_stride * 5);
      const uint16_t *block_src = src + block_y * src_stride * 4;
      uint16_t *block_dst       = dst + block_y * dst_stride * 5;

      for (block_x = 0; block_x < 62; block_x++)
      {
         const uint16_t *block_src_ptr = block_src;
         uint16_t *block_dst_ptr       = block_dst;

         uint16_t  _1,  _2,  _3,  _4,
                   _5,  _6,  _7,  _8,
                   _9, _10, _11, _12,
                  _13, _14, _15, _16;

         uint16_t   _1_2;
         uint16_t   _2_3;
         uint16_t   _3_4;

         uint16_t   _5_6;
         uint16_t   _6_7;
         uint16_t   _7_8;

         uint16_t  _9_10;
         uint16_t _10_11;
         uint16_t _11_12;

         uint16_t _13_14;
         uint16_t _14_15;
         uint16_t _15_16;

         uint16_t tmp;

         /* Horizontally:
          * Before(3):
          * (a)(b)(c)(d)
          * After(4):
          * (a)(ab)(bc)(cd)(d)
          *
          * Vertically:
          * Before(4): After(5):
          * (a)        (a)
          * (b)        (ab)
          * (c)        (bc)
          * (d)        (cd)
          *            (d)
          */

         /* -- Row 1 -- */
         _1 = *(block_src_ptr    );
         _2 = *(block_src_ptr + 1);
         _3 = *(block_src_ptr + 2);
         _4 = *(block_src_ptr + 3);

         *(block_dst_ptr    ) = _1;
         UPSCALE_192__WEIGHT(_1, _2, &_1_2, tmp);
         *(block_dst_ptr + 1) = _1_2;
         UPSCALE_192__WEIGHT(_2, _3, &_2_3, tmp);
         *(block_dst_ptr + 2) = _2_3;
         UPSCALE_192__WEIGHT(_3, _4, &_3_4, tmp);
         *(block_dst_ptr + 3) = _3_4;
         *(block_dst_ptr + 4) = _4;

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 2 -- */
         _5 = *(block_src_ptr    );
         _6 = *(block_src_ptr + 1);
         _7 = *(block_src_ptr + 2);
         _8 = *(block_src_ptr + 3);

         UPSCALE_192__WEIGHT(_1, _5, block_dst_ptr, tmp);
         UPSCALE_192__WEIGHT(_5, _6, &_5_6, tmp);
         UPSCALE_192__WEIGHT(_1_2, _5_6, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_6, _7, &_6_7, tmp);
         UPSCALE_192__WEIGHT(_2_3, _6_7, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_7, _8, &_7_8, tmp);
         UPSCALE_192__WEIGHT(_3_4, _7_8, block_dst_ptr + 3, tmp);
         UPSCALE_192__WEIGHT(_4, _8, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 3 -- */
          _9 = *(block_src_ptr    );
         _10 = *(block_src_ptr + 1);
         _11 = *(block_src_ptr + 2);
         _12 = *(block_src_ptr + 3);


         UPSCALE_192__WEIGHT(_5, _9, block_dst_ptr, tmp);
         UPSCALE_192__WEIGHT(_9, _10, &_9_10, tmp);
         UPSCALE_192__WEIGHT(_5_6, _9_10, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_10, _11, &_10_11, tmp);
         UPSCALE_192__WEIGHT(_6_7, _10_11, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_11, _12, &_11_12, tmp);
         UPSCALE_192__WEIGHT(_7_8, _11_12, block_dst_ptr + 3, tmp);
         UPSCALE_192__WEIGHT(_8, _12, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 4 -- */
         _13 = *(block_src_ptr    );
         _14 = *(block_src_ptr + 1);
         _15 = *(block_src_ptr + 2);
         _16 = *(block_src_ptr + 3);


         UPSCALE_192__WEIGHT(_9, _13, block_dst_ptr, tmp);
         UPSCALE_192__WEIGHT(_13, _14, &_13_14, tmp);
         UPSCALE_192__WEIGHT(_9_10, _13_14, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_14, _15, &_14_15, tmp);
         UPSCALE_192__WEIGHT(_10_11, _14_15, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_15, _16, &_15_16, tmp);
         UPSCALE_192__WEIGHT(_11_12, _15_16, block_dst_ptr + 3, tmp);
         UPSCALE_192__WEIGHT(_12, _16, block_dst_ptr + 4, tmp);

         block_src_ptr += src_stride;
         block_dst_ptr += dst_stride;

         /* -- Row 5 -- */
         *(block_dst_ptr    ) = _13;
         UPSCALE_192__WEIGHT(_13, _14, block_dst_ptr + 1, tmp);
         UPSCALE_192__WEIGHT(_14, _15, block_dst_ptr + 2, tmp);
         UPSCALE_192__WEIGHT(_15, _16, block_dst_ptr + 3, tmp);
         *(block_dst_ptr + 4) = _16;

         block_src += 4;
         block_dst += 5;
      }
      
      memset(dst, 0, sizeof(uint16_t) * src_stride * 5);
      
   }
}

/*******************************************************************
 *******************************************************************/

static unsigned upscale_x192_320x240_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned upscale_x192_320x240_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned upscale_x192_320x240_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *upscale_x192_320x240_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)simd;
   (void)config;
   (void)userdata;

   if (!filt) {
      return NULL;
   }
   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers) {
      free(filt);
      return NULL;
   }

   return filt;
}

static void upscale_x192_320x240_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   if (((width == 248) || (width == 256)) && (height == 192))
   {
      *out_width  = 320;
      *out_height = 240;
   }
   else
   {
      *out_width  = width;
      *out_height = height;
   }
}

static void upscale_x192_320x240_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void upscale_x192_320x240_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;   
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;

   if (height == 192)
   {
      if (width == 256)
      {
         upscale_256x192_to_320x240(output, input, out_stride, in_stride);
         return;
      }
      else if (width == 248)
      {
         upscale_248x192_to_320x240(output, input, out_stride, in_stride);
         return;
      }
   }

   /* Input buffer is of dimensions that cannot be upscaled
    * > Simply copy input to output */

   /* If source and destination buffers have the
    * same pitch, perform fast copy of raw pixel data */
   if (in_stride == out_stride)
      memcpy(output, input, thr->out_pitch * height);
   else
   {
      /* Otherwise copy pixel data line-by-line */
      unsigned y;
      for (y = 0; y < height; y++)
      {
         memcpy(output, input, width * sizeof(uint16_t));
         input  += in_stride;
         output += out_stride;
      }
   }
}

static void upscale_x192_320x240_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   /* We are guaranteed single threaded operation
    * (filt->threads = 1) so we don't need to loop
    * over threads and can cull some code. This only
    * makes the tiniest performance difference, but
    * every little helps when running on an o3DS... */
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[0];

   thr->out_data = (uint8_t*)output;
   thr->in_data = (const uint8_t*)input;
   thr->out_pitch = output_stride;
   thr->in_pitch = input_stride;
   thr->width = width;
   thr->height = height;

   if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
      packets[0].work = upscale_x192_320x240_work_cb_rgb565;
   }
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation upscale_x192_320x240_generic = {
   upscale_x192_320x240_generic_input_fmts,
   upscale_x192_320x240_generic_output_fmts,

   upscale_x192_320x240_generic_create,
   upscale_x192_320x240_generic_destroy,

   upscale_x192_320x240_generic_threads,
   upscale_x192_320x240_generic_output,
   upscale_x192_320x240_generic_packets,

   SOFTFILTER_API_VERSION,
   "Upscale_x192-320x240",
   "upscale_x192_320x240",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &upscale_x192_320x240_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
