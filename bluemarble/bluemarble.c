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
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "nedgz/nedgz_tile.h"
#include "nedgz/nedgz_util.h"
#include "texgz/texgz_tex.h"
#include "texgz/texgz_png.h"
#include "libpak/pak_file.h"

#define LOG_TAG "bluemarble"
#include "nedgz/nedgz_log.h"

#define SUBTILE_SIZE 256

// TODO - replace nedgz hack
static void tile_coord(nedgz_tile_t* self, int i, int j, int m, int n,
                       double* lat, double* lon)
{
	assert(self);
	assert(i >= 0);
	assert(i < NEDGZ_SUBTILE_COUNT);
	assert(j >= 0);
	assert(j < NEDGZ_SUBTILE_COUNT);
	assert(m >= 0);
	assert(m < SUBTILE_SIZE);
	assert(n >= 0);
	assert(n < SUBTILE_SIZE);
	LOGD("debug i=%i, j=%i, m=%i, n=%i", i, j, m, n);

	// r allows following condition to hold true
	// (0, 0, 0, 15) == (0, 1, 0, 0)
	double count = (double) NEDGZ_SUBTILE_COUNT;
	double r     = (double) (SUBTILE_SIZE - 1);
	double id    = (double) i;
	double jd    = (double) j;
	double md    = (double) m;
	double nd    = (double) n;
	double lats  = (id + md/r)/count;
	double lons  = (jd + nd/r)/count;
	double latT  = self->latT;
	double lonL  = self->lonL;
	double latB  = self->latB;
	double lonR  = self->lonR;
	*lat = latT + lats*(latB - latT);
	*lon = lonL + lons*(lonR - lonL);
}

static void interpolatec(texgz_tex_t* tex,
                         float u, float v,
                         unsigned char* r,
                         unsigned char* g,
                         unsigned char* b)
{
	assert(tex);
	LOGD("debug u=%f, v=%f", u, v);

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
                        int i, int j,
                        int m, int n,
                        double latT, double lonL,
                        double latB, double lonR)
{
	assert(src);
	assert(dst);
	LOGD("debug x=%i, y=%i, i=%i, j=%i, m=%i, n=%i, latT=%lf, lonL=%lf, latB=%lf, lonR=%lf",
	     x, y, i, j, m, n, latT, lonL, latB, lonR);

	// make a dummy tile
	nedgz_tile_t* tile = nedgz_tile_new(x, y, 9);
	if(tile == NULL)
	{
		return;
	}

	// compute sample coords
	double lat;
	double lon;
	tile_coord(tile, i, j, m, n, &lat, &lon);

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

	nedgz_tile_delete(&tile);
}

static void sample_subtile(texgz_tex_t* src,
                           pak_file_t* pak,
                           int x, int y,
                           int i, int j,
                           double latT, double lonL,
                           double latB, double lonR)
{
	assert(src);
	assert(pak);
	LOGD("debug x=%i, y=%i, i=%i, j=%i, latT=%lf, lonL=%lf, latB=%lf, lonR=%lf",
	     x, y, i, j, latT, lonL, latB, lonR);

	// create dst
	texgz_tex_t* dst = texgz_tex_new(SUBTILE_SIZE, SUBTILE_SIZE,
	                                 SUBTILE_SIZE, SUBTILE_SIZE,
	                                 TEXGZ_UNSIGNED_BYTE,
	                                 TEXGZ_RGB, NULL);
	if(dst == NULL)
	{
		return;
	}

	int m;
	int n;
	for(m = 0; m < SUBTILE_SIZE; ++m)
	{
		for(n = 0; n < SUBTILE_SIZE; ++n)
		{
			sample_data(src, dst, x, y, i, j, m, n, latT, lonL, latB, lonR);
		}
	}

	// convert dst to 565
	texgz_tex_convert(dst, TEXGZ_UNSIGNED_SHORT_5_6_5, TEXGZ_RGB);

	char key[256];
	snprintf(key, 256, "%i_%i", j, i);
	pak_file_writek(pak, key);
	texgz_tex_exportf(dst, pak->f);
	texgz_tex_delete(&dst);
}

static void sample_tile(texgz_tex_t* src,
                        int month, int x, int y,
                        double latT, int lonL,
                        double latB, int lonR)
{
	assert(src);
	LOGD("debug month=%i, x=%i, y=%i, latT=%lf, lonL=%lf, latB=%lf, lonR=%lf",
	     month, x, y, latT, lonL, latB, lonR);

	char fname[256];
	snprintf(fname, 256, "bluemarble%i/9/%i_%i.pak", month, x, y);
	pak_file_t* pak = pak_file_open(fname, PAK_FLAG_WRITE);
	if(pak == NULL)
	{
		return;
	}

	int i;
	int j;
	for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
	{
		for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
		{
			sample_subtile(src, pak, x, y, i, j, latT, lonL, latB, lonR);
		}
	}

	pak_file_close(&pak);
}

static void sample_sector(int month, int r, int c,
                          double latT, double lonL,
                          double latB, double lonR,
                          const char* fname)
{
	assert(fname);
	LOGD("debug month=%i, r=%i, c=%i, latT=%lf, lonL=%ls, latB=%lf, lonR=%lf, fname=%s",
         month, r, c, latT, lonL, latB, lonR, fname);

	LOGI("%s", fname);

	texgz_tex_t* src = texgz_png_import(fname);
	if(src == NULL)
	{
		return;
	}

	// convert src to 888 (should be already)
	texgz_tex_convert(src, TEXGZ_UNSIGNED_BYTE, TEXGZ_RGB);

	// 2^9*256/2048 gives 64 tiles at zoom=9
	int t = 64;
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
			sample_tile(src, month, x, y, latT, lonL, latB, lonR);
		}
	}

	texgz_tex_delete(&src);
}

int main(int argc, char** argv)
{
	int         month;
	char        fname[256];
	const char* base = "nasa-blue-marble/world.topo.bathy.2004";
	const char* size = "3x21600x21600";
	for(month = 1; month <= 12; ++month)
	{
		// create directories if necessary
		char dname[256];
		snprintf(dname, 256, "bluemarble%i", month);
		if(mkdir(dname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if(errno == EEXIST)
			{
				// already exists
			}
			else
			{
				LOGE("mkdir %s failed", dname);
				return EXIT_FAILURE;
			}
		}
		snprintf(dname, 256, "bluemarble%i/9", month);
		if(mkdir(dname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if(errno == EEXIST)
			{
				// already exists
			}
			else
			{
				LOGE("mkdir %s failed", dname);
				return EXIT_FAILURE;
			}
		}

		snprintf(fname, 256, "%s%02i.%s.A1.png", base, month, size);
		sample_sector(month, 0, 0, 90.0, -180.0, 0.0, -90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.B1.png", base, month, size);
		sample_sector(month, 0, 1, 90.0, -90.0, 0.0, 0.0, fname);

		snprintf(fname, 256, "%s%02i.%s.C1.png", base, month, size);
		sample_sector(month, 0, 2, 90.0, 0.0, 0.0, 90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.D1.png", base, month, size);
		sample_sector(month, 0, 3, 90.0, 90.0, 0.0, 180.0, fname);

		snprintf(fname, 256, "%s%02i.%s.A2.png", base, month, size);
		sample_sector(month, 1, 0, 0.0, -180.0, -90.0, -90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.B2.png", base, month, size);
		sample_sector(month, 1, 1, 0.0, -90.0, -90.0, 0.0, fname);

		snprintf(fname, 256, "%s%02i.%s.C2.png", base, month, size);
		sample_sector(month, 1, 2, 0.0, 0.0, -90.0, 90.0, fname);

		snprintf(fname, 256, "%s%02i.%s.D2.png", base, month, size);
		sample_sector(month, 1, 3, 0.0, 90.0, -90.0, 180.0, fname);
	}
	return EXIT_SUCCESS;
}
