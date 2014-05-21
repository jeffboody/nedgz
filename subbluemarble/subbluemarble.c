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
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "nedgz/nedgz_tile.h"
#include "nedgz/nedgz_util.h"
#include "texgz/texgz_tex.h"
#include "libpak/pak_file.h"

#define LOG_TAG "subbluemarble"
#include "nedgz/nedgz_log.h"

#define SUBTILE_SIZE 256

static void interpolatec(texgz_tex_t* tex, float u, float v,
                         unsigned char* r,
                         unsigned char* g,
                         unsigned char* b)
{
	assert(tex);
	LOGD("debug u=%f, v=%f", u, v);

	// "float indices"
	float pu = u*(SUBTILE_SIZE - 1);
	float pv = v*(SUBTILE_SIZE - 1);

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
	if(u1 >= SUBTILE_SIZE)
	{
		u1 = SUBTILE_SIZE - 1;
	}
	if(v0 < 0)
	{
		v0 = 0;
	}
	if(v1 >= SUBTILE_SIZE)
	{
		v1 = SUBTILE_SIZE - 1;
	}

	// compute interpolation coordinates
	float u0f = (float) u0;
	float v0f = (float) v0;
	float uf    = pu - u0f;
	float vf    = pv - v0f;

	// sample interpolation values
	unsigned char* pixels = tex->pixels;
	int            bpp    = texgz_tex_bpp(tex);
	float r00   = (float) pixels[bpp*(v0*SUBTILE_SIZE + u0)];
	float r01   = (float) pixels[bpp*(v1*SUBTILE_SIZE + u0)];
	float r10   = (float) pixels[bpp*(v0*SUBTILE_SIZE + u1)];
	float r11   = (float) pixels[bpp*(v1*SUBTILE_SIZE + u1)];
	float g00   = (float) pixels[bpp*(v0*SUBTILE_SIZE + u0) + 1];
	float g01   = (float) pixels[bpp*(v1*SUBTILE_SIZE + u0) + 1];
	float g10   = (float) pixels[bpp*(v0*SUBTILE_SIZE + u1) + 1];
	float g11   = (float) pixels[bpp*(v1*SUBTILE_SIZE + u1) + 1];
	float b00   = (float) pixels[bpp*(v0*SUBTILE_SIZE + u0) + 2];
	float b01   = (float) pixels[bpp*(v1*SUBTILE_SIZE + u0) + 2];
	float b10   = (float) pixels[bpp*(v0*SUBTILE_SIZE + u1) + 2];
	float b11   = (float) pixels[bpp*(v1*SUBTILE_SIZE + u1) + 2];

	// interpolate u
	float r0010 = r00 + uf*(r10 - r00);
	float r0111 = r01 + uf*(r11 - r01);
	float g0010 = g00 + uf*(g10 - g00);
	float g0111 = g01 + uf*(g11 - g01);
	float b0010 = b00 + uf*(b10 - b00);
	float b0111 = b01 + uf*(b11 - b01);

	// interpolate v
	*r = (r0010 + vf*(r0111 - r0010) + 0.5f);
	*g = (g0010 + vf*(g0111 - g0010) + 0.5f);
	*b = (b0010 + vf*(b0111 - b0010) + 0.5f);
}

static void sample_subtile(pak_file_t* pak, pak_file_t* subpak, int i, int j)
{
	assert(pak);
	assert(subpak);
	LOGD("debug i=%i, j=%i", i, j);

	// create the dst subtile
	texgz_tex_t* tex = texgz_tex_new(SUBTILE_SIZE,
	                                 SUBTILE_SIZE,
	                                 SUBTILE_SIZE,
	                                 SUBTILE_SIZE,
	                                 TEXGZ_UNSIGNED_BYTE,
	                                 TEXGZ_RGB,
	                                 NULL);
	if(tex == NULL)
	{
		return;
	}

	// sample the 00-quadrant
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	int           m;
	int           n;
	int           size;
	texgz_tex_t*  subtex;
	int           half = SUBTILE_SIZE/2;
	int           iprime = (2*i)%NEDGZ_SUBTILE_COUNT;
	int           jprime = (2*j)%NEDGZ_SUBTILE_COUNT;
	char          fname[256];
	snprintf(fname, 256, "%i_%i", jprime, iprime);
	size = pak_file_seek(subpak, fname);
	subtex = (size > 0) ? texgz_tex_importf(subpak->f, size) : NULL;
	if(subtex)
	{
		texgz_tex_convert(subtex, TEXGZ_UNSIGNED_BYTE, TEXGZ_RGB);

		for(m = 0; m < half; ++m)
		{
			for(n = 0; n < half; ++n)
			{
				// scale uv coords for 00-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f);
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f);

				interpolatec(subtex, u, v, &r, &g, &b);

				unsigned char* pixels = (unsigned char*) tex->pixels;
				int            bpp    = texgz_tex_bpp(tex);
				pixels[bpp*(m*SUBTILE_SIZE + n) + 0] = r;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 1] = g;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 2] = b;
			}
		}
		texgz_tex_delete(&subtex);
	}

	// sample the 01-quadrant
	snprintf(fname, 256, "%i_%i", jprime, iprime + 1);
	size = pak_file_seek(subpak, fname);
	subtex = (size > 0) ? texgz_tex_importf(subpak->f, size) : NULL;
	if(subtex)
	{
		texgz_tex_convert(subtex, TEXGZ_UNSIGNED_BYTE, TEXGZ_RGB);

		for(m = half; m < SUBTILE_SIZE; ++m)
		{
			for(n = 0; n < half; ++n)
			{
				// scale uv coords for 01-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f);
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;

				interpolatec(subtex, u, v, &r, &g, &b);

				unsigned char* pixels = (unsigned char*) tex->pixels;
				int            bpp    = texgz_tex_bpp(tex);
				pixels[bpp*(m*SUBTILE_SIZE + n) + 0] = r;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 1] = g;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 2] = b;
			}
		}
		texgz_tex_delete(&subtex);
	}

	// sample the 10-quadrant
	snprintf(fname, 256, "%i_%i", jprime + 1, iprime);
	size = pak_file_seek(subpak, fname);
	subtex = (size > 0) ? texgz_tex_importf(subpak->f, size) : NULL;
	if(subtex)
	{
		texgz_tex_convert(subtex, TEXGZ_UNSIGNED_BYTE, TEXGZ_RGB);

		for(m = 0; m < half; ++m)
		{
			for(n = half; n < SUBTILE_SIZE; ++n)
			{
				// scale uv coords for 10-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f);

				interpolatec(subtex, u, v, &r, &g, &b);

				unsigned char* pixels = (unsigned char*) tex->pixels;
				int            bpp    = texgz_tex_bpp(tex);
				pixels[bpp*(m*SUBTILE_SIZE + n) + 0] = r;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 1] = g;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 2] = b;
			}
		}
		texgz_tex_delete(&subtex);
	}

	// sample the 11-quadrant
	snprintf(fname, 256, "%i_%i", jprime + 1, iprime + 1);
	size = pak_file_seek(subpak, fname);
	subtex = (size > 0) ? texgz_tex_importf(subpak->f, size) : NULL;
	if(subtex)
	{
		texgz_tex_convert(subtex, TEXGZ_UNSIGNED_BYTE, TEXGZ_RGB);

		for(m = half; m < SUBTILE_SIZE; ++m)
		{
			for(n = half; n < SUBTILE_SIZE; ++n)
			{
				// scale uv coords for 11-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;

				interpolatec(subtex, u, v, &r, &g, &b);

				unsigned char* pixels = (unsigned char*) tex->pixels;
				int            bpp    = texgz_tex_bpp(tex);
				pixels[bpp*(m*SUBTILE_SIZE + n) + 0] = r;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 1] = g;
				pixels[bpp*(m*SUBTILE_SIZE + n) + 2] = b;
			}
		}
		texgz_tex_delete(&subtex);
	}

	texgz_tex_convert(tex, TEXGZ_UNSIGNED_SHORT_5_6_5, TEXGZ_RGB);

	// j=dx, i=dy
	snprintf(fname, 256, "%i_%i", j, i);
	pak_file_writek(pak, fname);
	texgz_tex_exportf(tex, pak->f);
	texgz_tex_delete(&tex);
}

static void sample_tile(int month, int x, int y, int zoom)
{
	LOGD("debug month=%i, x=%i, y=%i, zoom=%i", month, x, y, zoom);

	// create directories if necessary
	char dname[256];
	snprintf(dname, 256, "bluemarble%i/%i", month, zoom);
	if(mkdir(dname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
	{
		if(errno == EEXIST)
		{
			// already exists
		}
		else
		{
			LOGE("mkdir %s failed", dname);
			return;
		}
	}

	// open the src
	char fname[256];
	snprintf(fname, 256, "bluemarble%i/%i/%i_%i.pak", month, zoom + 1, 2*x, 2*y);
	pak_file_t* subpak00 = pak_file_open(fname, PAK_FLAG_READ);
	snprintf(fname, 256, "bluemarble%i/%i/%i_%i.pak", month, zoom + 1, 2*x, 2*y + 1);
	pak_file_t* subpak01 = pak_file_open(fname, PAK_FLAG_READ);
	snprintf(fname, 256, "bluemarble%i/%i/%i_%i.pak", month, zoom + 1, 2*x + 1, 2*y);
	pak_file_t* subpak10 = pak_file_open(fname, PAK_FLAG_READ);
	snprintf(fname, 256, "bluemarble%i/%i/%i_%i.pak", month, zoom + 1, 2*x + 1, 2*y + 1);
	pak_file_t* subpak11 = pak_file_open(fname, PAK_FLAG_READ);

	if((subpak00 == NULL) &&
	   (subpak01 == NULL) &&
	   (subpak10 == NULL) &&
	   (subpak11 == NULL))
	{
		goto fail_src;
	}

	// open the dst tile
	snprintf(fname, 256, "bluemarble%i/%i/%i_%i.pak", month, zoom, x, y);
	pak_file_t* pak = pak_file_open(fname, PAK_FLAG_WRITE);
	if(pak == NULL)
	{
		goto fail_dst;
	}

	// sample the 00-quadrant
	int i;
	int j;
	int half = NEDGZ_SUBTILE_COUNT/2;
	if(subpak00)
	{
		for(i = 0; i < half; ++i)
		{
			for(j = 0; j < half; ++j)
			{
				sample_subtile(pak, subpak00, i, j);
			}
		}
		pak_file_close(&subpak00);
	}

	// sample the 01-quadrant
	if(subpak01)
	{
		for(i = half; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = 0; j < half; ++j)
			{
				sample_subtile(pak, subpak01, i, j);
			}
		}
		pak_file_close(&subpak01);
	}

	// sample the 10-quadrant
	if(subpak10)
	{
		for(i = 0; i < half; ++i)
		{
			for(j = half; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				sample_subtile(pak, subpak10, i, j);
			}
		}
		pak_file_close(&subpak10);
	}

	// sample the 11-quadrant
	if(subpak11)
	{
		for(i = half; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = half; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				sample_subtile(pak, subpak11, i, j);
			}
		}
		pak_file_close(&subpak11);
	}

	pak_file_close(&pak);

	// success
	return;

	// failure
	fail_dst:
	fail_src:
		pak_file_close(&subpak00);
		pak_file_close(&subpak01);
		pak_file_close(&subpak10);
		pak_file_close(&subpak11);
}

static void sample_tile_range(int month, int x0, int y0, int x1, int y1, int zoom)
{
	LOGD("debug month=%i, x0=%i, y0=%i, x1=%i, y1=%i, zoom=%i", month, x0, y0, x1, y1, zoom);

	int x;
	int y;
	int idx   = 1;
	int count = (x1 - x0)*(y1 - y0);
	for(y = y0; y < y1; ++y)
	{
		for(x = x0; x < x1; ++x)
		{
			LOGI("%i: %i/%i: x=%i, y=%i", month, idx++, count, x, y);

			sample_tile(month, x, y, zoom);
		}
	}
}

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		LOGE("usage: %s [zoom]", argv[0]);
		return EXIT_FAILURE;
	}

	// determine range of candidate tiles
	int zoom = (int) strtol(argv[1], NULL, 0);
	int x0   = 0;
	int y0   = 0;
	int x1   = (int) powl(2, zoom)/NEDGZ_SUBTILE_COUNT;
	int y1   = (int) powl(2, zoom)/NEDGZ_SUBTILE_COUNT;

	// sample the set of tiles whose origin should cover range
	// again, due to overlap with other flt tiles the sampling
	// actually occurs over the entire flt_xx set
	int month;
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

		sample_tile_range(month, x0, y0, x1, y1, zoom);
	}

	// success
	return EXIT_SUCCESS;
}
