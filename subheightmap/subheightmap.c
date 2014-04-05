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

#define LOG_TAG "subheightmap"
#include "nedgz/nedgz_log.h"

#define SUBTILE_SIZE 256

static short interpolateh(texgz_tex_t* tex, float u, float v)
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
	short* pixels = (short*) tex->pixels;
	float h00   = (float) pixels[v0*SUBTILE_SIZE + u0];
	float h01   = (float) pixels[v1*SUBTILE_SIZE + u0];
	float h10   = (float) pixels[v0*SUBTILE_SIZE + u1];
	float h11   = (float) pixels[v1*SUBTILE_SIZE + u1];

	// interpolate u
	float h0010 = h00 + uf*(h10 - h00);
	float h0111 = h01 + uf*(h11 - h01);

	// interpolate v
	return (short) (h0010 + vf*(h0111 - h0010) + 0.5f);
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
                                     TEXGZ_SHORT,
	                                 TEXGZ_LUMINANCE,
                                     NULL);
	if(tex == NULL)
	{
		return;
	}

	// sample the 00-quadrant
	int          m;
	int          n;
	int          size;
	texgz_tex_t* subtex;
	int          half = SUBTILE_SIZE/2;
	int          iprime = (2*i)%NEDGZ_SUBTILE_COUNT;
	int          jprime = (2*j)%NEDGZ_SUBTILE_COUNT;
	char         fname[256];
	snprintf(fname, 256, "%i_%i", jprime, iprime);
	size = pak_file_seek(subpak, fname);
	subtex = (size > 0) ? texgz_tex_importf(subpak->f, size) : NULL;
	if(subtex)
	{
		for(m = 0; m < half; ++m)
		{
			for(n = 0; n < half; ++n)
			{
				// scale uv coords for 00-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f);
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f);

				short* pixels = (short*) tex->pixels;
				pixels[m*SUBTILE_SIZE + n] = interpolateh(subtex, u, v);
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
		for(m = half; m < SUBTILE_SIZE; ++m)
		{
			for(n = 0; n < half; ++n)
			{
				// scale uv coords for 01-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f);
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;

				short* pixels = (short*) tex->pixels;
				pixels[m*SUBTILE_SIZE + n] = interpolateh(subtex, u, v);
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
		for(m = 0; m < half; ++m)
		{
			for(n = half; n < SUBTILE_SIZE; ++n)
			{
				// scale uv coords for 10-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f);

				short* pixels = (short*) tex->pixels;
				pixels[m*SUBTILE_SIZE + n] = interpolateh(subtex, u, v);
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
		for(m = half; m < SUBTILE_SIZE; ++m)
		{
			for(n = half; n < SUBTILE_SIZE; ++n)
			{
				// scale uv coords for 11-quadrant
				float u = 2.0f*((float) n)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;
				float v = 2.0f*((float) m)/((float) SUBTILE_SIZE - 1.0f) - 1.0f;

				short* pixels = (short*) tex->pixels;
				pixels[m*SUBTILE_SIZE + n] = interpolateh(subtex, u, v);
			}
		}
		texgz_tex_delete(&subtex);
	}

	// j=dx, i=dy
	snprintf(fname, 256, "%i_%i", j, i);
	pak_file_writek(pak, fname);
	texgz_tex_exportf(tex, pak->f);
	texgz_tex_delete(&tex);
}

static void sample_tile(int x, int y, int zoom)
{
	LOGD("debug x=%i, y=%i, zoom=%i", x, y, zoom);

	// create directories if necessary
	char dname[256];
	snprintf(dname, 256, "heightmap/%i", zoom);
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
	snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom + 1, 2*x, 2*y);
	pak_file_t* subpak00 = pak_file_open(fname, PAK_FLAG_READ);
	snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom + 1, 2*x, 2*y + 1);
	pak_file_t* subpak01 = pak_file_open(fname, PAK_FLAG_READ);
	snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom + 1, 2*x + 1, 2*y);
	pak_file_t* subpak10 = pak_file_open(fname, PAK_FLAG_READ);
	snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom + 1, 2*x + 1, 2*y + 1);
	pak_file_t* subpak11 = pak_file_open(fname, PAK_FLAG_READ);

	if((subpak00 == NULL) &&
	   (subpak01 == NULL) &&
	   (subpak10 == NULL) &&
	   (subpak11 == NULL))
	{
		goto fail_src;
	}

	// open the dst tile
	snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x, y);
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

static void sample_tile_range(int x0, int y0, int x1, int y1, int zoom)
{
	LOGD("debug x0=%i, y0=%i, x1=%i, y1=%i, zoom=%i", x0, y0, x1, y1, zoom);

	int x;
	int y;
	int idx   = 0;
	int count = (x1 - x0 + 1)*(y1 - y0 + 1);
	for(y = y0; y <= y1; ++y)
	{
		for(x = x0; x <= x1; ++x)
		{
			LOGI("%i/%i: x=%i, y=%i", idx++, count, x, y);

			sample_tile(x, y, zoom);
		}
	}
}

int main(int argc, char** argv)
{
	if(argc != 6)
	{
		LOGE("usage: %s [zoom] [latT] [lonL] [latB] [lonR]", argv[0]);
		return EXIT_FAILURE;
	}

	// create directories if necessary
	char dname[256];
	snprintf(dname, 256, "%s", "heightmap");
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

	int zoom = (int) strtol(argv[1], NULL, 0);
	int latT = (int) strtol(argv[2], NULL, 0);
	int lonL = (int) strtol(argv[3], NULL, 0);
	int latB = (int) strtol(argv[4], NULL, 0) - 1;
	int lonR = (int) strtol(argv[5], NULL, 0) + 1;

	// determine tile range
	float x0f;
	float y0f;
	float x1f;
	float y1f;
	nedgz_coord2tile(latT,
	                 lonL,
	                 zoom,
	                 &x0f,
	                 &y0f);
	nedgz_coord2tile(latB,
	                 lonR,
	                 zoom,
	                 &x1f,
	                 &y1f);

	// determine range of candidate tiles
	int x0 = (int) x0f;
	int y0 = (int) y0f;
	int x1 = (int) (x1f + 1.0f);
	int y1 = (int) (y1f + 1.0f);

	// sample the set of tiles whose origin should cover range
	// again, due to overlap with other flt tiles the sampling
	// actually occurs over the entire flt_xx set
	sample_tile_range(x0, y0, x1, y1, zoom);

	// success
	return EXIT_SUCCESS;
}
