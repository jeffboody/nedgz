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
#include "nedgz/nedgz_util.h"
#include "nedgz/nedgz_tile.h"
#include "texgz/texgz_tex.h"
#include "libpak/pak_file.h"

#define LOG_TAG "hillshade"
#include "texgz/texgz_log.h"

#define SUBTILE_SIZE 256

// hillshading
static pak_file_t* dst = NULL;

// heightmap
static pak_file_t*  src_tl = NULL;
static pak_file_t*  src_tc = NULL;
static pak_file_t*  src_tr = NULL;
static pak_file_t*  src_cl = NULL;
static pak_file_t*  src_cc = NULL;
static pak_file_t*  src_cr = NULL;
static pak_file_t*  src_bl = NULL;
static pak_file_t*  src_bc = NULL;
static pak_file_t*  src_br = NULL;
static texgz_tex_t* tex_tl = NULL;
static texgz_tex_t* tex_tc = NULL;
static texgz_tex_t* tex_tr = NULL;
static texgz_tex_t* tex_cl = NULL;
static texgz_tex_t* tex_cc = NULL;
static texgz_tex_t* tex_cr = NULL;
static texgz_tex_t* tex_bl = NULL;
static texgz_tex_t* tex_bc = NULL;
static texgz_tex_t* tex_br = NULL;

static void subtile2coord(int x, int y, int zoom,
                          int i, int j, int m, int n,
                          double* lat, double* lon)
{
	assert(lat);
	assert(lon);
	LOGD("debug x=%i, y=%i, zoom=%i, i=%i, j=%i, m=%i, n=%i",
	     x, y, zoom, i, j, m, n);

	float s  = (float) SUBTILE_SIZE;
	float c  = (float) NEDGZ_SUBTILE_COUNT;
	float xx = (float) x;
	float yy = (float) y;
	float jj = (float) j;
	float ii = (float) i;
	float nn = (float) n/(s - 1.0f);
	float mm = (float) m/(s - 1.0f);

	nedgz_tile2coord(xx + (jj + nn)/c, yy + (ii + mm)/c,
	                 zoom, lat, lon);
}

static void tile_coord(int x, int y, int zoom,
                       int i, int j,
                       int m, int n,
                       double* lat, double* lon)
{
	assert(i >= 0);
	assert(i < NEDGZ_SUBTILE_COUNT);
	assert(j >= 0);
	assert(j < NEDGZ_SUBTILE_COUNT);
	assert(m >= 0);
	assert(m < 256);
	assert(n >= 0);
	assert(n < 256);
	LOGD("debug i=%i, j=%i, m=%i, n=%i", i, j, m, n);

	subtile2coord(x, y, zoom,
	              i, j, m, n, lat, lon);
}

static void world_coord2xy(double lat, double lon,
                           float* x, float* y)
{
	assert(x);
	assert(y);
	LOGD("debug lat=%lf, lon=%lf", lat, lon);

	// use home as the origin
	double lat2meter = 111072.12110934;
	double lon2meter = 85337.868965619;
	double home_lat  = 40.061295;
	double home_lon  =-105.214552;

	*x = (float) ((lon - home_lon)*lon2meter);
	*y = (float) ((lat - home_lat)*lat2meter);
}

static texgz_tex_t* opentex(int i, int j)
{
	LOGD("debug i=%i, j=%i", i, j);

	pak_file_t* pak = src_cc;
	if((i < 0) && (j < 0))
	{
		i   += NEDGZ_SUBTILE_COUNT;
		j   += NEDGZ_SUBTILE_COUNT;
		pak  = src_tl;
	}
	else if((i >= NEDGZ_SUBTILE_COUNT) && (j < 0))
	{
		i   -= NEDGZ_SUBTILE_COUNT;
		j   += NEDGZ_SUBTILE_COUNT;
		pak  = src_bl;
	}
	else if((i < 0) && (j >= NEDGZ_SUBTILE_COUNT))
	{
		i   += NEDGZ_SUBTILE_COUNT;
		j   -= NEDGZ_SUBTILE_COUNT;
		pak  = src_tr;
	}
	else if((i >= NEDGZ_SUBTILE_COUNT) && (j >= NEDGZ_SUBTILE_COUNT))
	{
		i   -= NEDGZ_SUBTILE_COUNT;
		j   -= NEDGZ_SUBTILE_COUNT;
		pak  = src_br;
	}
	else if(i < 0)
	{
		i   += NEDGZ_SUBTILE_COUNT;
		pak  = src_tc;
	}
	else if(i >= NEDGZ_SUBTILE_COUNT)
	{
		i   -= NEDGZ_SUBTILE_COUNT;
		pak  = src_bc;
	}
	else if(j < 0)
	{
		j   += NEDGZ_SUBTILE_COUNT;
		pak  = src_cl;
	}
	else if(j >= NEDGZ_SUBTILE_COUNT)
	{
		j   -= NEDGZ_SUBTILE_COUNT;
		pak  = src_cr;
	}

	// heightmap may be sparse
	if(pak == NULL)
	{
		return NULL;
	}

	char key[256];
	snprintf(key, 256, "%i_%i", j, i);

	// heightmap may be sparse
	int size = pak_file_seek(pak, key);
	if(size == 0)
	{
		return NULL;
	}

	return texgz_tex_importf(pak->f, size);
}

static float get_height(int c, int r)
{
	LOGD("debug c=%i, r=%i", c, r);

	// sample the neighboring heighmap but take into
	// account the 1-pixel shared edge
	int offset = SUBTILE_SIZE - 1;

	texgz_tex_t* tex = tex_cc;
	if((r < 0) && (c < 0))
	{
		r   += offset;
		c   += offset;
		tex  = tex_tl;
	}
	else if((r >= SUBTILE_SIZE) && (c < 0))
	{
		r   -= offset;
		c   += offset;
		tex  = tex_bl;
	}
	else if((r < 0) && (c >= SUBTILE_SIZE))
	{
		r   += offset;
		c   -= offset;
		tex  = tex_tr;
	}
	else if((r >= SUBTILE_SIZE) && (c >= SUBTILE_SIZE))
	{
		r   -= offset;
		c   -= offset;
		tex  = tex_br;
	}
	else if(r < 0)
	{
		r   += offset;
		tex  = tex_tc;
	}
	else if(r >= SUBTILE_SIZE)
	{
		r   -= offset;
		tex  = tex_bc;
	}
	else if(c < 0)
	{
		c   += offset;
		tex  = tex_cl;
	}
	else if(c >= SUBTILE_SIZE)
	{
		c   -= offset;
		tex  = tex_cr;
	}

	if(tex == NULL)
	{
		return (float) NEDGZ_NODATA;
	}

	short* pixels = (short*) tex->pixels;
	short height = pixels[SUBTILE_SIZE*r + c];
	return nedgz_feet2meters((float) height);
}

void compute_dz(texgz_tex_t* tex, int m, int n, float dx, float dy)
{
	assert(tex);
	LOGD("debug m=%i, n=%i, dx=%f, dy=%f", m, n, dx, dy);

#ifdef GOTO_USE_3X3
	// initialize edge masks
	float mask_x[] =
	{
		-1.0f/4.0f, 0.0f, 1.0f/4.0f,
		-2.0f/4.0f, 0.0f, 2.0f/4.0f,
		-1.0f/4.0f, 0.0f, 1.0f/4.0f,
	};
	float mask_y[] =
	{
		-1.0f/4.0f, -2.0f/4.0f, -1.0f/4.0f,
		      0.0f,       0.0f,       0.0f,
		 1.0f/4.0f,  2.0f/4.0f,  1.0f/4.0f,
	};

	// initialize map
	float map[] =
	{
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
	};
	int r  = m;
	int c  = n;
	int cm = c - 1;
	int cp = c + 1;
	int rm = r - 1;
	int rp = r + 1;
	map[0] = get_height(cm, rm);
	map[1] = get_height(c,  rm);
	map[2] = get_height(cp, rm);
	map[3] = get_height(cm, r);
	map[5] = get_height(cp, r);
	map[6] = get_height(cm, rp);
	map[7] = get_height(c,  rp);
	map[8] = get_height(cp, rp);

	// compute dzx, dzy
	float dzxf;
	float dzyf;
	dzxf  = map[0]*mask_x[0];
	dzyf  = map[0]*mask_y[0];
	dzyf += map[1]*mask_y[1];
	dzxf += map[2]*mask_x[2];
	dzyf += map[2]*mask_y[2];
	dzxf += map[3]*mask_x[3];
	dzxf += map[5]*mask_x[5];
	dzxf += map[6]*mask_x[6];
	dzyf += map[6]*mask_y[6];
	dzyf += map[7]*mask_y[7];
	dzxf += map[8]*mask_x[8];
	dzyf += map[8]*mask_y[8];
#else
	float mask_x[] =
	{
		 -5.0f/84.0f,  -4.0f/84.0f, 0.0f,  4.0f/84.0f,  5.0f/84.0f,
		 -8.0f/84.0f, -10.0f/84.0f, 0.0f, 10.0f/84.0f,  8.0f/84.0f,
		-10.0f/84.0f, -20.0f/84.0f, 0.0f, 20.0f/84.0f, 10.0f/84.0f,
		 -8.0f/84.0f, -10.0f/84.0f, 0.0f, 10.0f/84.0f,  8.0f/84.0f,
		 -5.0f/84.0f,  -4.0f/84.0f, 0.0f,  4.0f/84.0f,  5.0f/84.0f,
	};
	float mask_y[] =
	{
		-5.0f/84.0f,  -8.0f/84.0f, -10.0f/84.0f,  -8.0f/84.0f, -5.0f/84.0f,
		-4.0f/84.0f, -10.0f/84.0f, -20.0f/84.0f, -10.0f/84.0f, -4.0f/84.0f,
		       0.0f,         0.0f,         0.0f,         0.0f,        0.0f,
		 4.0f/84.0f,  10.0f/84.0f,  20.0f/84.0f,  10.0f/84.0f,  4.0f/84.0f,
		 5.0f/84.0f,   8.0f/84.0f,  10.0f/84.0f,   8.0f/84.0f,  5.0f/84.0f,
	};

	// initialize map
	float map[] =
	{
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	};
	int r  = m;
	int c  = n;
	int cm2 = c - 2;
	int cm1 = c - 1;
	int cp2 = c + 2;
	int cp1 = c + 1;
	int rm2 = r - 2;
	int rm1 = r - 1;
	int rp2 = r + 2;
	int rp1 = r + 1;
	map[ 0] = get_height(cm2, rm2);
	map[ 1] = get_height(cm1, rm2);
	map[ 2] = get_height(c,   rm2);
	map[ 3] = get_height(cp1, rm2);
	map[ 4] = get_height(cp2, rm2);
	map[ 5] = get_height(cm2, rm1);
	map[ 6] = get_height(cm1, rm1);
	map[ 7] = get_height(c,   rm1);
	map[ 8] = get_height(cp1, rm1);
	map[ 9] = get_height(cp2, rm1);
	map[10] = get_height(cm2, r);
	map[11] = get_height(cm1, r);
	map[12] = get_height(c,   r);
	map[13] = get_height(cp1, r);
	map[14] = get_height(cp2, r);
	map[15] = get_height(cm2, rp1);
	map[16] = get_height(cm1, rp1);
	map[17] = get_height(c,   rp1);
	map[18] = get_height(cp1, rp1);
	map[19] = get_height(cp2, rp1);
	map[20] = get_height(cm2, rp2);
	map[21] = get_height(cm1, rp2);
	map[22] = get_height(c,   rp2);
	map[23] = get_height(cp1, rp2);
	map[24] = get_height(cp2, rp2);

	// compute dzx, dzy
	float dzxf;
	float dzyf;
	dzxf  = map[ 0]*mask_x[ 0];
	dzyf  = map[ 0]*mask_y[ 0];
	dzxf += map[ 1]*mask_x[ 1];
	dzyf += map[ 1]*mask_y[ 1];
	dzyf += map[ 2]*mask_y[ 2];
	dzxf += map[ 3]*mask_x[ 3];
	dzyf += map[ 3]*mask_y[ 3];
	dzxf += map[ 4]*mask_x[ 4];
	dzyf += map[ 4]*mask_y[ 4];
	dzxf += map[ 5]*mask_x[ 5];
	dzyf += map[ 5]*mask_y[ 5];
	dzxf += map[ 6]*mask_x[ 6];
	dzyf += map[ 6]*mask_y[ 6];
	dzyf += map[ 7]*mask_y[ 7];
	dzxf += map[ 8]*mask_x[ 8];
	dzyf += map[ 8]*mask_y[ 8];
	dzxf += map[ 9]*mask_x[ 9];
	dzyf += map[ 9]*mask_y[ 9];
	dzxf += map[10]*mask_x[10];
	dzxf += map[11]*mask_x[11];
	dzxf += map[13]*mask_x[13];
	dzxf += map[14]*mask_x[14];
	dzxf += map[15]*mask_x[15];
	dzyf += map[15]*mask_y[15];
	dzxf += map[16]*mask_x[16];
	dzyf += map[16]*mask_y[16];
	dzyf += map[17]*mask_y[17];
	dzxf += map[18]*mask_x[18];
	dzyf += map[18]*mask_y[18];
	dzxf += map[19]*mask_x[19];
	dzyf += map[19]*mask_y[19];
	dzxf += map[20]*mask_x[20];
	dzyf += map[20]*mask_y[20];
	dzxf += map[21]*mask_x[21];
	dzyf += map[21]*mask_y[21];
	dzyf += map[22]*mask_y[22];
	dzxf += map[23]*mask_x[23];
	dzyf += map[23]*mask_y[23];
	dzxf += map[24]*mask_x[24];
	dzyf += map[24]*mask_y[24];
#endif

	// scale dz so that dx and dy are 1.0
	dzxf /= dx;
	dzyf /= dy;

	// clamp dzx and dzy to (-2.0, 2.0)
	if(dzxf < -2.0f) dzxf = -2.0f;
	if(dzxf >  2.0f) dzxf =  2.0f;
	if(dzyf < -2.0f) dzyf = -2.0f;
	if(dzyf >  2.0f) dzyf =  2.0f;

	// scale dzx and dzy to (0.0, 1.0)
	dzxf = (dzxf/4.0f) + 0.5f;
	dzyf = (dzyf/4.0f) + 0.5f;

	// scale dzx and dzy to (0, 255)
	unsigned char dzx = (unsigned char) (dzxf*255.0f);
	unsigned char dzy = (unsigned char) (dzyf*255.0f);

	// store dzx and dzy
	tex->pixels[2*(SUBTILE_SIZE*r + c)]     = dzx;
	tex->pixels[2*(SUBTILE_SIZE*r + c) + 1] = dzy;
}

static void sample_subtile(int zoom, int x, int y, int i, int j)
{
	LOGD("debug zoom=%i, x=%i, y=%i, i=%i, j=%i",
	     zoom, x, y, i, j);

	// compute dx and dy of mask
	#ifdef HILLSHADE_USE_3X3
		int mask_size = 3;
	#else
		int mask_size = 5;
	#endif
	float mask_dx;
	float mask_dy;
	{
		double lat0;
		double lon0;
		double lat1;
		double lon1;
		float  x0;
		float  y0;
		float  x1;
		float  y1;
		tile_coord(x, y, zoom, i, j,
		           0, 0, &lat0, &lon0);
		tile_coord(x, y, zoom, i, j,
		           mask_size, mask_size,
		           &lat1, &lon1);
		world_coord2xy(lat0, lon0, &x0, &y0);
		world_coord2xy(lat1, lon1, &x1, &y1);
		mask_dx = x1 - x0;
		mask_dy = y1 - y0;
	}

	// open hillshade dst
	texgz_tex_t* tex = texgz_tex_new(SUBTILE_SIZE, SUBTILE_SIZE,
	                                 SUBTILE_SIZE, SUBTILE_SIZE,
	                                 TEXGZ_UNSIGNED_BYTE,
	                                 TEXGZ_LUMINANCE_ALPHA,
	                                 NULL);
	if(tex == NULL)
	{
		return;
	}

	// open heightmap src
	tex_cc = opentex(i, j);
	if(tex_cc == NULL)
	{
		goto fail_tex_cc;
	}
	tex_tl = opentex(i - 1, j - 1);
	tex_tc = opentex(i - 1, j);
	tex_tr = opentex(i - 1, j + 1);
	tex_cl = opentex(i, j - 1);
	tex_cr = opentex(i, j + 1);
	tex_bl = opentex(i + 1, j - 1);
	tex_bc = opentex(i + 1, j);
	tex_br = opentex(i + 1, j + 1);

	// compute hillshading
	int m;
	int n;
	for(m = 0; m < SUBTILE_SIZE; ++m)
	{
		for(n = 0; n < SUBTILE_SIZE; ++n)
		{
			compute_dz(tex, m, n, mask_dx, mask_dy);
		}
	}

	// export hillshading
	char key[256];
	snprintf(key, 256, "%i_%i", j, i);
	pak_file_writek(dst, key);
	texgz_tex_exportf(tex, dst->f);

	texgz_tex_delete(&tex_tl);
	texgz_tex_delete(&tex_tc);
	texgz_tex_delete(&tex_tr);
	texgz_tex_delete(&tex_cl);
	texgz_tex_delete(&tex_cc);
	texgz_tex_delete(&tex_cr);
	texgz_tex_delete(&tex_bl);
	texgz_tex_delete(&tex_bc);
	texgz_tex_delete(&tex_br);
fail_tex_cc:
	texgz_tex_delete(&tex);
}

int main(int argc, char** argv)
{
	// heightmap.list
	// zoom x y
	if(argc != 2)
	{
		LOGE("usage: %s [heightmap.list]", argv[0]);
		return EXIT_FAILURE;
	}

	// create directories if necessary
	char dname[256];
	snprintf(dname, 256, "%s", "hillshade");
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

	// open the list
	FILE* f = fopen(argv[1], "r");
	if(f == NULL)
	{
		LOGE("failed to open %s", argv[1]);
		return EXIT_FAILURE;
	}

	// iteratively pak hillshade images
	char* line = NULL;
	size_t n   = 0;
	int index = 0;
	while(getline(&line, &n, f) > 0)
	{
		int x;
		int y;
		int zoom;
		if(sscanf(line, "%i %i %i", &zoom, &x, &y) != 3)
		{
			LOGE("invalid line=%s", line);
			continue;
		}

		LOGI("%i: zoom=%i, x=%i, y=%i", index++, zoom, x, y);

		// create directories if necessary
		char dname[256];
		snprintf(dname, 256, "hillshade/%i", zoom);
		if(mkdir(dname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if(errno == EEXIST)
			{
				// already exists
			}
			else
			{
				LOGE("mkdir %s failed", dname);
				continue;
			}
		}

		// open hillshade dst
		char fname[256];
		snprintf(fname, 256, "hillshade/%i/%i_%i.pak", zoom, x, y);
		dst = pak_file_open(fname, PAK_FLAG_WRITE);
		if(dst == NULL)
		{
			continue;
		}

		// open heightmaps src
		// only src_cc must exist
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x, y);
		src_cc = pak_file_open(fname, PAK_FLAG_READ);
		if(src_cc == NULL)
		{
			pak_file_close(&dst);
			continue;
		}
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x - 1, y - 1);
		src_tl = pak_file_open(fname, PAK_FLAG_READ);
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x, y - 1);
		src_tc = pak_file_open(fname, PAK_FLAG_READ);
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x + 1, y - 1);
		src_tr = pak_file_open(fname, PAK_FLAG_READ);
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x - 1, y);
		src_cl = pak_file_open(fname, PAK_FLAG_READ);
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x + 1, y);
		src_cr = pak_file_open(fname, PAK_FLAG_READ);
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x - 1, y + 1);
		src_bl = pak_file_open(fname, PAK_FLAG_READ);
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x, y + 1);
		src_bc = pak_file_open(fname, PAK_FLAG_READ);
		snprintf(fname, 256, "heightmap/%i/%i_%i.pak", zoom, x + 1, y + 1);
		src_br = pak_file_open(fname, PAK_FLAG_READ);

		int i;
		int j;
		for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				sample_subtile(zoom, x, y, i, j);
			}
		}

		pak_file_close(&src_tl);
		pak_file_close(&src_tc);
		pak_file_close(&src_tr);
		pak_file_close(&src_cl);
		pak_file_close(&src_cc);
		pak_file_close(&src_cr);
		pak_file_close(&src_bl);
		pak_file_close(&src_bc);
		pak_file_close(&src_br);
		pak_file_close(&dst);
	}
	free(line);

	return EXIT_SUCCESS;
}
