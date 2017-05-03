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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "terrain/terrain_util.h"
#include "texgz/texgz_tex.h"
#include "texgz/texgz_png.h"

#define LOG_TAG "bluemarble"
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

static void interpolatec(texgz_tex_t* tex,
                         float u, float v,
                         unsigned char* r,
                         unsigned char* g,
                         unsigned char* b)
{
	assert(tex);

	// "float indices"
	float pu = u*(tex->width - 1);
	float pv = v*(tex->height - 1);

	// determine indices to sample
	int u0 = (int) pu;
	int v0 = (int) pv;
	int u1 = u0 + 1;
	int v1 = v0 + 1;

	// double check the indices
	if(u0 < 0)
	{
		u0 = 0;
	}
	if(u1 >= tex->width)
	{
		u1 = tex->width - 1;
	}
	if(v0 < 0)
	{
		v0 = 0;
	}
	if(v1 >= tex->height)
	{
		v1 = tex->height - 1;
	}

	// compute interpolation coordinates
	float u0f = (float) u0;
	float v0f = (float) v0;
	float uf    = pu - u0f;
	float vf    = pv - v0f;

	// compute index
	int bpp    = texgz_tex_bpp(tex);
	int stride = tex->stride;
	int idx00  = v0*stride*bpp + u0*bpp;
	int idx01  = v1*stride*bpp + u0*bpp;
	int idx10  = v0*stride*bpp + u1*bpp;
	int idx11  = v1*stride*bpp + u1*bpp;

	// sample interpolation values
	unsigned char* pixels = tex->pixels;
	float          r00    = (float) pixels[idx00];
	float          r01    = (float) pixels[idx01];
	float          r10    = (float) pixels[idx10];
	float          r11    = (float) pixels[idx11];
	float          g00    = (float) pixels[idx00 + 1];
	float          g01    = (float) pixels[idx01 + 1];
	float          g10    = (float) pixels[idx10 + 1];
	float          g11    = (float) pixels[idx11 + 1];
	float          b00    = (float) pixels[idx00 + 2];
	float          b01    = (float) pixels[idx01 + 2];
	float          b10    = (float) pixels[idx10 + 2];
	float          b11    = (float) pixels[idx11 + 2];

	// interpolate u
	float r0010 = r00 + uf*(r10 - r00);
	float r0111 = r01 + uf*(r11 - r01);
	float g0010 = g00 + uf*(g10 - g00);
	float g0111 = g01 + uf*(g11 - g01);
	float b0010 = b00 + uf*(b10 - b00);
	float b0111 = b01 + uf*(b11 - b01);

	// interpolate v
	*r = (unsigned char) (r0010 + vf*(r0111 - r0010) + 0.5f);
	*g = (unsigned char) (g0010 + vf*(g0111 - g0010) + 0.5f);
	*b = (unsigned char) (b0010 + vf*(b0111 - b0010) + 0.5f);
}

static void sample_data(texgz_tex_t* src,
                        texgz_tex_t* dst,
                        int x, int y,
                        int m, int n,
                        double latT, double lonL,
                        double latB, double lonR)
{
	assert(src);
	assert(dst);

	// compute sample coords
	double lat;
	double lon;
	float s  = (float) (SUBTILE_SIZE - 1);
	float xx = (float) x;
	float yy = (float) y;
	float nn = (float) n/s;
	float mm = (float) m/s;
	terrain_tile2coord(xx + nn, yy + mm, 9, &lat, &lon);

	// compute u, v
	float u = (float) ((lon - lonL)/(lonR - lonL));
	float v = (float) ((lat - latT)/(latB - latT));

	// interpolate color
	unsigned char r;
	unsigned char g;
	unsigned char b;
	interpolatec(src, u, v, &r, &g, &b);

	// store color
	unsigned char* pixels = dst->pixels;
	int            stride = dst->stride;
	int            bpp    = texgz_tex_bpp(dst);
	int            idx    = m*stride*bpp + n*bpp;
	pixels[idx + 0] = r;
	pixels[idx + 1] = g;
	pixels[idx + 2] = b;
}

static void sample_tile(texgz_tex_t* src,
                        texgz_tex_t* dst,
                        int month, int x, int y,
                        double latT, double lonL,
                        double latB, double lonR)
{
	assert(src);
	assert(dst);

	int m;
	int n;
	for(m = 0; m < SUBTILE_SIZE; ++m)
	{
		for(n = 0; n < SUBTILE_SIZE; ++n)
		{
			sample_data(src, dst, x, y, m, n,
			            latT, lonL, latB, lonR);
		}
	}

	// export the tile
	char fname[256];
	snprintf(fname, 256, "png256/%i/9/%i/%i.png",
	         month, x, y);
	fname[255] = '\0';
	mymkdir(fname);
	texgz_png_export(dst, fname);
}

static void sample_sector(texgz_tex_t* dst,
                          int month, int r, int c,
                          double latT, double lonL,
                          double latB, double lonR,
                          const char* fname)
{
	assert(dst);
	assert(fname);

	LOGI("%s", fname);

	texgz_tex_t* src = texgz_png_import(fname);
	if(src == NULL)
	{
		return;
	}

	// convert src to 888 (should be already)
	texgz_tex_convert(src, TEXGZ_UNSIGNED_BYTE, TEXGZ_RGB);

	// 2^9 gives 512 tiles at zoom=9
	int t = 512;
	int x;
	int y;
	int xmin = (t/4)*c;
	int xmax = (t/4)*(c + 1);
	int ymin = (t/2)*r;
	int ymax = (t/2)*(r + 1);
	for(y = ymin; y < ymax; ++y)
	{
		for(x = xmin; x < xmax; ++x)
		{
			sample_tile(src, dst, month, x, y,
			            latT, lonL, latB, lonR);
		}
	}

	texgz_tex_delete(&src);
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

	int         month;
	char        fname[256];
	const char* base = "png/world.topo.bathy.2004";
	const char* size = "3x21600x21600";
	for(month = 1; month <= 12; ++month)
	{
		snprintf(fname, 256, "%s%02i.%s.A1.png", base, month, size);
		sample_sector(dst, month, 0, 0, 90.0, -180.0, 0.0, -90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.B1.png", base, month, size);
		sample_sector(dst, month, 0, 1, 90.0, -90.0, 0.0, 0.0, fname);

		snprintf(fname, 256, "%s%02i.%s.C1.png", base, month, size);
		sample_sector(dst, month, 0, 2, 90.0, 0.0, 0.0, 90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.D1.png", base, month, size);
		sample_sector(dst, month, 0, 3, 90.0, 90.0, 0.0, 180.0, fname);

		snprintf(fname, 256, "%s%02i.%s.A2.png", base, month, size);
		sample_sector(dst, month, 1, 0, 0.0, -180.0, -90.0, -90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.B2.png", base, month, size);
		sample_sector(dst, month, 1, 1, 0.0, -90.0, -90.0, 0.0, fname);

		snprintf(fname, 256, "%s%02i.%s.C2.png", base, month, size);
		sample_sector(dst, month, 1, 2, 0.0, 0.0, -90.0, 90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.D2.png", base, month, size);
		sample_sector(dst, month, 1, 3, 0.0, 90.0, -90.0, 180.0, fname);
	}

	texgz_tex_delete(&dst);

	return EXIT_SUCCESS;
}
