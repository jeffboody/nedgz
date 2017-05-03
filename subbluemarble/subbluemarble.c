/*
 * Copyright (c) 2014 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "texgz/texgz_tex.h"
#include "texgz/texgz_png.h"

#define LOG_TAG "subbluemarble"
#include "nedgz/nedgz_log.h"

#define SUBTILE_SIZE 256

static int mymkdir(const char* fname)
{
	assert(fname);

	int  len = strnlen(fname, 255);
	char dir[256];
	int  i;
	for(i = 0; i < len; ++i)
	{
		dir[i]     = fname[i];
		dir[i + 1] = '\0';

		if(dir[i] == '/')
		{
			if(access(dir, R_OK) == 0)
			{
				// dir already exists
				continue;
			}

			// try to mkdir
			if(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
			{
				if(errno == EEXIST)
				{
					// already exists
				}
				else
				{
					LOGE("mkdir %s failed", dir);
					return 0;
				}
			}
		}
	}

	return 1;
}

static void sample_subtile(texgz_tex_t* dst, texgz_tex_t* src,
                           int m, int n, int m0, int n0)
{
	assert(dst);
	assert(src);

	int r = 0;
	int g = 0;
	int b = 0;

	unsigned char* pixels = (unsigned char*) src->pixels;
	int            bpp    = texgz_tex_bpp(src);

	// sample the 00-quadrant
	int mm = m0;
	int nn = n0;
	r += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 0];
	g += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 1];
	b += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 2];

	// sample the 01-quadrant
	mm = m0;
	nn = n0 + 1;
	r += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 0];
	g += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 1];
	b += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 2];

	// sample the 10-quadrant
	mm = m0 + 1;
	nn = n0;
	r += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 0];
	g += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 1];
	b += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 2];

	// sample the 11-quadrant
	mm = m0 + 1;
	nn = n0 + 1;
	r += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 0];
	g += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 1];
	b += (int) pixels[bpp*(mm*SUBTILE_SIZE + nn) + 2];

	// average samples (e.g. box filter)
	pixels = (unsigned char*) dst->pixels;
	bpp    = texgz_tex_bpp(dst);
	pixels[bpp*(m*SUBTILE_SIZE + n) + 0] = (unsigned char) (r/4);
	pixels[bpp*(m*SUBTILE_SIZE + n) + 1] = (unsigned char) (g/4);
	pixels[bpp*(m*SUBTILE_SIZE + n) + 2] = (unsigned char) (b/4);
}

static void sample_tile(texgz_tex_t* dst,
                        int month, int zoom, int x, int y)
{
	assert(dst);

	// open the src
	char fname[256];
	snprintf(fname, 256, "png256/%i/%i/%i/%i.png",
	         month, zoom + 1, 2*x, 2*y);
	texgz_tex_t* src00 = texgz_png_import(fname);
	if(src00 == NULL)
	{
		return;
	}

	snprintf(fname, 256, "png256/%i/%i/%i/%i.png",
	         month, zoom + 1, 2*x, 2*y + 1);
	texgz_tex_t* src01 = texgz_png_import(fname);
	if(src01 == NULL)
	{
		goto fail_src01;
	}

	snprintf(fname, 256, "png256/%i/%i/%i/%i.png",
	         month, zoom + 1, 2*x + 1, 2*y);
	texgz_tex_t* src10 = texgz_png_import(fname);
	if(src10 == NULL)
	{
		goto fail_src10;
	}

	snprintf(fname, 256, "png256/%i/%i/%i/%i.png",
	         month, zoom + 1, 2*x + 1, 2*y + 1);
	texgz_tex_t* src11 = texgz_png_import(fname);
	if(src11 == NULL)
	{
		goto fail_src11;
	}

	// sample the 00-quadrant
	int m;
	int n;
	int half = SUBTILE_SIZE/2;
	for(m = 0; m < half; ++m)
	{
		for(n = 0; n < half; ++n)
		{
			sample_subtile(dst, src00, m, n,
			               2*m, 2*n);
		}
	}

	// sample the 01-quadrant
	for(m = half; m < SUBTILE_SIZE; ++m)
	{
		for(n = 0; n < half; ++n)
		{
			sample_subtile(dst, src01, m, n,
			               2*(m - half), 2*n);
		}
	}

	// sample the 10-quadrant
	for(m = 0; m < half; ++m)
	{
		for(n = half; n < SUBTILE_SIZE; ++n)
		{
			sample_subtile(dst, src10, m, n,
			               2*m, 2*(n - half));
		}
	}

	// sample the 11-quadrant
	for(m = half; m < SUBTILE_SIZE; ++m)
	{
		for(n = half; n < SUBTILE_SIZE; ++n)
		{
			sample_subtile(dst, src11, m, n,
			               2*(m - half), 2*(n - half));
		}
	}

	// export the tile
	snprintf(fname, 256, "png256/%i/%i/%i/%i.png",
	         month, zoom, x, y);
	fname[255] = '\0';
	mymkdir(fname);
	texgz_png_export(dst, fname);

	texgz_tex_delete(&src11);
	texgz_tex_delete(&src10);
	texgz_tex_delete(&src01);
	texgz_tex_delete(&src00);

	// success
	return;

	// failure
	fail_src11:
		texgz_tex_delete(&src10);
	fail_src10:
		texgz_tex_delete(&src01);
	fail_src01:
		texgz_tex_delete(&src00);
}

static void sample_tile_range(texgz_tex_t* dst,
                              int month, int zoom,
                              int x0, int y0, int x1, int y1)
{
	assert(dst);

	int x;
	int y;
	int idx   = 0;
	int count = (x1 - x0)*(y1 - y0);
	for(y = y0; y < y1; ++y)
	{
		for(x = x0; x < x1; ++x)
		{
			++idx;
			if((x%8 == 0) && (y%8 == 0))
			{
				LOGI("%i: %i/%i: zoom=%i, x=%i, y=%i",
				     month, idx, count, zoom, x, y);
			}

			sample_tile(dst, month, zoom, x, y);
		}
	}
}

int main(int argc, char** argv)
{
	if(argc != 1)
	{
		LOGE("usage: %s", argv[0]);
		return EXIT_FAILURE;
	}

	// create temporary working image
	texgz_tex_t* dst = texgz_tex_new(SUBTILE_SIZE, SUBTILE_SIZE,
	                                 SUBTILE_SIZE, SUBTILE_SIZE,
	                                 TEXGZ_UNSIGNED_BYTE,
	                                 TEXGZ_RGB, NULL);
	if(dst == NULL)
	{
		return EXIT_FAILURE;
	}

	// sample remaining zoom levels
	int zoom;
	for(zoom = 8; zoom >=0; --zoom)
	{
		int x0   = 0;
		int y0   = 0;
		int x1   = (int) powl(2, zoom);
		int y1   = (int) powl(2, zoom);

		int month;
		for(month = 1; month <= 12; ++month)
		{
			sample_tile_range(dst, month, zoom,
			                  x0, y0, x1, y1);
		}
	}

	texgz_tex_delete(&dst);

	// success
	return EXIT_SUCCESS;
}
