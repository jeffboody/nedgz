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

#define LOG_TAG "subned"
#include "nedgz/nedgz_log.h"

static short interpolateh(nedgz_subtile_t* subtile, float u, float v)
{
	assert(subtile);
	LOGD("debug u=%f, v=%f", u, v);

	// "float indices"
	float pu = u*(NEDGZ_SUBTILE_SIZE - 1);
	float pv = v*(NEDGZ_SUBTILE_SIZE - 1);

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
	if(u1 >= NEDGZ_SUBTILE_SIZE)
	{
		u1 = NEDGZ_SUBTILE_SIZE - 1;
	}
	if(v0 < 0)
	{
		v0 = 0;
	}
	if(v1 >= NEDGZ_SUBTILE_SIZE)
	{
		v1 = NEDGZ_SUBTILE_SIZE - 1;
	}

	// compute interpolation coordinates
	float u0f = (float) u0;
	float v0f = (float) v0;
	float uf    = pu - u0f;
	float vf    = pv - v0f;

	// sample interpolation values
	short* data = (short*) subtile->data;
	float h00   = (float) data[v0*NEDGZ_SUBTILE_SIZE + u0];
	float h01   = (float) data[v1*NEDGZ_SUBTILE_SIZE + u0];
	float h10   = (float) data[v0*NEDGZ_SUBTILE_SIZE + u1];
	float h11   = (float) data[v1*NEDGZ_SUBTILE_SIZE + u1];

	// interpolate u
	float h0010 = h00 + uf*(h10 - h00);
	float h0111 = h01 + uf*(h11 - h01);

	// interpolate v
	return (short) (h0010 + vf*(h0111 - h0010) + 0.5f);
}

static void sample_subtile(nedgz_tile_t* ned, nedgz_tile_t* subned, int i, int j)
{
	assert(ned);
	assert(subned);
	LOGD("debug i=%i, j=%i", i, j);

	// sample the 00-quadrant
	int m;
	int n;
	int half = NEDGZ_SUBTILE_SIZE/2;
	int iprime = (2*i)%NEDGZ_SUBTILE_COUNT;
	int jprime = (2*j)%NEDGZ_SUBTILE_COUNT;
	nedgz_subtile_t* subtile = nedgz_tile_getij(subned, iprime, jprime);
	if(subtile)
	{
		for(m = 0; m < half; ++m)
		{
			for(n = 0; n < half; ++n)
			{
				// scale uv coords for 00-quadrant
				float u = 2.0f*((float) n)/((float) NEDGZ_SUBTILE_SIZE - 1.0f);
				float v = 2.0f*((float) m)/((float) NEDGZ_SUBTILE_SIZE - 1.0f);
				short h = interpolateh(subtile, u, v);
				nedgz_tile_set(ned, i, j, m, n, h);
			}
		}
	}

	// sample the 01-quadrant
	subtile = nedgz_tile_getij(subned, iprime + 1, jprime);
	if(subtile)
	{
		for(m = half; m < NEDGZ_SUBTILE_SIZE; ++m)
		{
			for(n = 0; n < half; ++n)
			{
				// scale uv coords for 01-quadrant
				float u = 2.0f*((float) n)/((float) NEDGZ_SUBTILE_SIZE - 1.0f);
				float v = 2.0f*((float) m)/((float) NEDGZ_SUBTILE_SIZE - 1.0f) - 1.0f;
				short h = interpolateh(subtile, u, v);
				nedgz_tile_set(ned, i, j, m, n, h);
			}
		}
	}

	// sample the 10-quadrant
	subtile = nedgz_tile_getij(subned, iprime, jprime + 1);
	if(subtile)
	{
		for(m = 0; m < half; ++m)
		{
			for(n = half; n < NEDGZ_SUBTILE_SIZE; ++n)
			{
				// scale uv coords for 10-quadrant
				float u = 2.0f*((float) n)/((float) NEDGZ_SUBTILE_SIZE - 1.0f) - 1.0f;
				float v = 2.0f*((float) m)/((float) NEDGZ_SUBTILE_SIZE - 1.0f);
				short h = interpolateh(subtile, u, v);
				nedgz_tile_set(ned, i, j, m, n, h);
			}
		}
	}

	// sample the 11-quadrant
	subtile = nedgz_tile_getij(subned, iprime + 1, jprime + 1);
	if(subtile)
	{
		for(m = half; m < NEDGZ_SUBTILE_SIZE; ++m)
		{
			for(n = half; n < NEDGZ_SUBTILE_SIZE; ++n)
			{
				// scale uv coords for 11-quadrant
				float u = 2.0f*((float) n)/((float) NEDGZ_SUBTILE_SIZE - 1.0f) - 1.0f;
				float v = 2.0f*((float) m)/((float) NEDGZ_SUBTILE_SIZE - 1.0f) - 1.0f;
				short h = interpolateh(subtile, u, v);
				nedgz_tile_set(ned, i, j, m, n, h);
			}
		}
	}
}

static void sample_tile(int x, int y, int zoom)
{
	LOGD("debug x=%i, y=%i, zoom=%i", x, y, zoom);

	// create directories if necessary
	char dname[256];
	snprintf(dname, 256, "ned/%i", zoom);
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
	nedgz_tile_t* subned00 = nedgz_tile_import("ned", 2*x, 2*y, zoom + 1);
	nedgz_tile_t* subned01 = nedgz_tile_import("ned", 2*x, 2*y + 1, zoom + 1);
	nedgz_tile_t* subned10 = nedgz_tile_import("ned", 2*x + 1, 2*y, zoom + 1);
	nedgz_tile_t* subned11 = nedgz_tile_import("ned", 2*x + 1, 2*y + 1, zoom + 1);
	if((subned00 == NULL) &&
	   (subned01 == NULL) &&
	   (subned10 == NULL) &&
	   (subned11 == NULL))
	{
		goto fail_src;
	}

	// open the dst tile
	nedgz_tile_t* ned = nedgz_tile_new(x, y, zoom);
	if(ned == NULL)
	{
		goto fail_dst;
	}

	// sample the 00-quadrant
	int i;
	int j;
	int half = NEDGZ_SUBTILE_COUNT/2;
	if(subned00)
	{
		for(i = 0; i < half; ++i)
		{
			for(j = 0; j < half; ++j)
			{
				sample_subtile(ned, subned00, i, j);
			}
		}
		nedgz_tile_delete(&subned00);
	}

	// sample the 01-quadrant
	if(subned01)
	{
		for(i = half; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = 0; j < half; ++j)
			{
				sample_subtile(ned, subned01, i, j);
			}
		}
		nedgz_tile_delete(&subned01);
	}

	// sample the 10-quadrant
	if(subned10)
	{
		for(i = 0; i < half; ++i)
		{
			for(j = half; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				sample_subtile(ned, subned10, i, j);
			}
		}
		nedgz_tile_delete(&subned10);
	}

	// sample the 11-quadrant
	if(subned11)
	{
		for(i = half; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = half; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				sample_subtile(ned, subned11, i, j);
			}
		}
		nedgz_tile_delete(&subned11);
	}

	nedgz_tile_export(ned, "ned");
	nedgz_tile_delete(&ned);

	// success
	return;

	// failure
	fail_dst:
	fail_src:
		nedgz_tile_delete(&subned00);
		nedgz_tile_delete(&subned01);
		nedgz_tile_delete(&subned10);
		nedgz_tile_delete(&subned11);
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
	snprintf(dname, 256, "%s", "ned");
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
