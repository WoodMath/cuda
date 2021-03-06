/*
 * Copyright in this software is owned by CSIRO.  CSIRO grants permission to
 * any individual or institution to use, copy, modify, and distribute this
 * software, provided that:
 * 
 * (a)     this copyright and permission notice appears in its entirety in or
 * on (as the case may be) all copies of the software and supporting
 * documentation; 
 * 
 * (b)     the authors of papers that describe software systems using this
 * software package acknowledge such use by citing the paper as follows: 
 * 
 *     "Quantitative Evaluation of Matching Methods and Validity Measures for
 *     Stereo Vision" by J. Banks and P. Corke, Int. J. Robotics Research,
 *     Vol 20(7), 2001; and
 * 
 * (c)     users of this software acknowledge and agree that:
 * 
 *   (i) CSIRO makes no representations about the suitability of this software
 *   for any purpose;
 * 
 *   (ii) that the software is provided "as is" without express or implied
 *   warranty; and
 *  
 *   (iii) users of this software use the software entirely at their own risk.
 *
 *  Author: Jasmine E. Banks (jbanks@ieee.org)
 */

#define STANDALONE
#ifndef STANDALONE 
#include "mex.h"
#define CALLOC mxCalloc
#else

#include <stdlib.h>
#include <stdio.h>
#define CALLOC calloc
#endif

#include <math.h>
#define COUNT_TABLE_BITS 256

static void calc_table (int N, long int start, long int end, unsigned char *table, unsigned char count) {

  /*printf("%d\t%d\t%d\t%d\n",N,start,end,count);*/
  if (N == 1) {
    *(table + start) = count;

  } else {
    calc_table (N/2, start, start + (end-start+1)/2 - 1, table, count);
    calc_table (N/2, start + (end-start+1)/2, end, table, count + 1);
  }
} /* calc_table */

static void print_table(unsigned char *count_table, int N) {
  int i;
  for (i=0; i<N; i++)
    printf("%d\t%d\n",i,*(count_table+i));
}

static void census_transform (unsigned char *image, int x_window_size, int y_window_size, int width, int height, int num_buffs, int size_buff, long int *census_tx) {
  int i, j, x_surround, y_surround, top, bottom, left, right, x, y, incr, index, k;
  unsigned char *image_row_ptr, *pix_ptr, *top_corner, centre_val;
  long int *census_ptr;

  printf ("CensusTransform [%d,%d], [%d,%d], %d, %d\n",x_window_size, y_window_size, width, height, num_buffs, size_buff);

  x_surround = (x_window_size - 1) / 2; 
  y_surround = (y_window_size - 1) / 2;
  top = y_surround;
  left = x_surround;
  right = width - x_surround;
  bottom = height - y_surround;
  incr = width - x_window_size;

  image_row_ptr = image;

  for (y = top; y < bottom; y++) {
    census_ptr = census_tx + (y * width + left) * num_buffs;
    top_corner = image_row_ptr;

    for (x = left; x < right; x++) {
      pix_ptr =  top_corner;
      centre_val = *(top_corner + width * y_surround + x_surround);
	      
      /* initialise census transform to 0 */
      for (i = 0; i < num_buffs; i++) 
	  *(census_ptr + i) = 0;

      k = 0;
      for (i = 0; i < y_window_size; i++) {
		for (j = 0; j < x_window_size; j++) {
		  index = k / size_buff;
		  *(census_ptr + index) <<= 1;
		  if (*pix_ptr < centre_val)
			*(census_ptr + index) |= 1;
		  pix_ptr++;
		  k++;
		} /* for j */
		pix_ptr += incr;
      } /* for i */	

      top_corner++;
      census_ptr += num_buffs;
    } /* for x */

    image_row_ptr += width;
  } /* for */
} /* census_transform */

void CENSUS_RIGHT (unsigned char *left_image, unsigned char *right_image, signed char *disparity, double *min_array,
		           int width, int height, int x_census_win_size, int y_census_win_size, int x_window_size, int y_window_size, int min_disparity, int max_disparity) {
  unsigned int right_x;
  int right_lim, left_lim, y, i, top, bottom, left, right, x_surround, y_surround, diff, num_buffs, extra_bits, size_buff, div_buffs, u, v, incr, x_surr1, y_surr1;
  long int *census_left, *census_right, *ptr_censusl, *ptr_censusr, census_l, census_r, *buff_r, *buff_l, *lptr, *rptr, xor_res;
  int disp;

  unsigned char *count_table;

  count_table = (unsigned char*) CALLOC(256, sizeof(unsigned char));
  calc_table (COUNT_TABLE_BITS, 0, COUNT_TABLE_BITS-1, count_table, 0);
  /* print_table(count_table,COUNT_TABLE_BITS); */

  size_buff = sizeof(long int) * 8; // 32
  div_buffs = (x_census_win_size * y_census_win_size) / size_buff;
  extra_bits = (x_census_win_size * y_census_win_size) % size_buff;
  num_buffs = div_buffs + ((extra_bits > 0)?1:0);

  buff_l = (long int*) CALLOC(num_buffs, sizeof(long int));
  buff_r = (long int*) CALLOC(num_buffs, sizeof(long int));

  census_left = (long int*) CALLOC(width * height * num_buffs, sizeof(long int));
  census_transform (left_image, x_census_win_size, y_census_win_size, width, height, num_buffs, size_buff, census_left);
  census_right = (long int*) CALLOC(width * height * num_buffs, sizeof(long int));
  census_transform (right_image, x_census_win_size, y_census_win_size, width, height, num_buffs, size_buff, census_right);

  x_surround = (x_window_size - 1) / 2; 
  y_surround = (y_window_size - 1) / 2;
  x_surr1 = x_surround + x_census_win_size/2; 
  y_surr1 = y_surround + y_census_win_size/2;
  top = y_surr1;
  left = x_surr1;
  right = width - x_surr1;
  bottom = height - y_surr1;
  incr = (width - x_window_size) * num_buffs;

  /* Set minimum array to a really large number */
  for (i = 0; i < width * height; i++)
    min_array[i] = 1E10;

  for (disp = min_disparity; disp < max_disparity; disp++) {
#ifndef STANDALONE
    fprintf (stderr, "%d ",disp);
#else
    printf ("%d\n",disp);
#endif
 
    for (y = top; y < bottom; y++) {

      if (disp < 0) {
	ptr_censusl =  census_left + ((y - y_surround) * width + x_surr1 - x_surround) * num_buffs;
	ptr_censusr = census_right + ((y - y_surround) * width - disp + x_surr1 - x_surround) * num_buffs;

      } else { 
	ptr_censusl =  census_left + ((y - y_surround) * width + disp + x_surr1 - x_surround) * num_buffs;  
	ptr_censusr = census_right + ((y - y_surround) * width + x_surr1 - x_surround) * num_buffs;
      }

      right_lim = (disp < 0)? right : right - disp;
      left_lim = (disp < 0)? left - disp : left;
      /*printf("%d\n",y);*/

      for (right_x = left_lim; right_x < right_lim; right_x++) { 

	lptr = ptr_censusl;
	rptr = ptr_censusr;

	diff = 0;
	for (u = 0; u < y_window_size; u++) {
	  for (v = 0; v < x_window_size * num_buffs; v++) {
	
	    census_l = *lptr;
	    census_r = *rptr;

	    xor_res = census_l ^ census_r;
	    for (i = 0; i < sizeof(long int); i++) {
	      diff += *(count_table + (xor_res & 0x00ff));
	    }
		
	    lptr ++;
	    rptr ++;
	  } /* for v */
	  
	  lptr += incr;
	  rptr += incr;
	} /* for u */
	
	if (diff < *(min_array + width * y + right_x)) {
	  *(disparity + width * y + right_x) = (unsigned char) disp; /* - min_disparity; */
	  *(min_array + width * y + right_x) = diff;
	} /* if */

	ptr_censusl += num_buffs;
	ptr_censusr += num_buffs;
      } /* for right_x */
    } /* for y*/
  } /* for disparity */

#ifdef STANDALONE
  free (count_table);
  free (buff_l); free(buff_r);
  free (census_left); free(census_right);
#endif

  printf("\n");
} /* CENSUS_RIGHT */

void CENSUS_LEFT (unsigned char *left_image, unsigned char *right_image, signed char *disparity, double *min_array, int width, int height, int x_census_win_size, int y_census_win_size, int x_window_size, int y_window_size, int min_disparity, int max_disparity) {
  unsigned int left_x, xor_res;
  int right_lim, left_lim, y, i, j, top, bottom, left, right, x_surround, y_surround, diff, num_buffs, extra_bits, size_buff, div_buffs, k, index, u, v, incr, x_surr1, y_surr1;
  long int *census_left, *census_right, *ptr_censusl, *ptr_censusr, census_l, census_r, bit_left, bit_right, *buff_r, *buff_l, *lptr, *rptr;  
  unsigned char *count_table;
  int disp;

  count_table = (unsigned char*) CALLOC(COUNT_TABLE_BITS, sizeof(unsigned char));
  calc_table (COUNT_TABLE_BITS, 0, COUNT_TABLE_BITS-1, count_table, 0);

  size_buff = sizeof(long int) * 8;
  div_buffs = (x_census_win_size * y_census_win_size) / size_buff;
  extra_bits = (x_census_win_size * y_census_win_size) % size_buff;
  num_buffs = div_buffs + ((extra_bits > 0)?1:0);

  buff_l = (long int*) CALLOC(num_buffs, sizeof(long int));
  buff_r = (long int*) CALLOC(num_buffs, sizeof(long int));

  census_left = (long int*) CALLOC(width*height*num_buffs, sizeof(long int));
  census_transform (left_image, x_census_win_size, y_census_win_size, width, height, num_buffs, size_buff, census_left);
  census_right = (long int*) CALLOC(width*height*num_buffs, sizeof(long int));
  census_transform (right_image, x_census_win_size, y_census_win_size, width, height, num_buffs, size_buff, census_right);
 
  x_surround = (x_window_size - 1) / 2; 
  y_surround = (y_window_size - 1) / 2;
  x_surr1 = x_surround + x_census_win_size/2; 
  y_surr1 = y_surround + y_census_win_size/2;
  top = y_surr1;
  left = x_surr1;
  right = width - x_surr1;
  bottom = height - y_surr1;
  incr = (width - x_window_size) * num_buffs;

  for (i = 0; i < width * height; i++)
    min_array[i] = 1E10;

  for (disp = min_disparity; disp < max_disparity; disp++) {
#ifndef STANDALONE
    printf ("%d ",disp);
#else
    printf ("%d\n",disp);
#endif
    
    for (y = top; y < bottom; y++) {

      if (disp < 0) {
	ptr_censusl =  census_left + ((y - y_surround) * width + x_surr1 - x_surround) * num_buffs;
	ptr_censusr = census_right + ((y - y_surround) * width - disp + x_surr1 - x_surround) * num_buffs;
      } else { 
	ptr_censusl =  census_left + ((y - y_surround) * width + disp + x_surr1 - x_surround) * num_buffs;  
	ptr_censusr = census_right + ((y - y_surround) * width + x_surr1 - x_surround) * num_buffs;
      }

      right_lim = (disp < 0)? right + disp : right;
      left_lim = (disp < 0)? left : left + disp;

      for (left_x = left_lim; left_x < right_lim; left_x++) { 

	lptr = ptr_censusl;
	rptr = ptr_censusr;

	diff = 0;
	for (u = 0; u < y_window_size; u++) {
	  for (v = 0; v < x_window_size * num_buffs; v++) {
	
	    census_l = *lptr;
	    census_r = *rptr;

	    xor_res = census_l ^ census_r;
	    for (i = 0; i < sizeof(long int); i++) {
		diff += *(count_table + (xor_res & 0x00ff));
		xor_res >> 8;
	    }

	    lptr++;
	    rptr++;
	  } /* for v */
	  
	  lptr += incr;
	  rptr += incr;
	} /* for u */

	if (diff < *(min_array + width * y + left_x)) {
	  *(disparity + width * y + left_x) = (unsigned char) disp; /* - min_disparity;*/
	  *(min_array + width * y + left_x) = diff;
	} /* if */

	ptr_censusl += num_buffs;
	ptr_censusr += num_buffs;
      } /* for left_x */
    } /* for y*/
  } /* for disparity */

#ifdef STANDALONE
  free (count_table);
  free (buff_l); free(buff_r);
  free (census_left); free(census_right);
#endif

  printf("\n");
} /* match_CENSUS_left */
