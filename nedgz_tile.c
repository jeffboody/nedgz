/*
 * Copyright (c) 2013 Jeff Boody
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
#include <sys/stat.h>
#include <sys/types.h>
#include <zlib.h>
#include "nedgz_tile.h"
#include "nedgz_util.h"

#define LOG_TAG "nedgz"
#include "nedgz_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

static nedgz_subtile_t* nedgz_subtile_new(void)
{
	nedgz_subtile_t* self = (nedgz_subtile_t*) malloc(sizeof(nedgz_subtile_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	int i;
	for(i = 0; i < NEDGZ_SUBTILE_SIZE*NEDGZ_SUBTILE_SIZE; ++i)
	{
		self->data[i] = NEDGZ_NODATA;
	}
	return self;
}

static void nedgz_subtile_delete(nedgz_subtile_t** _self)
{
	assert(_self);

	nedgz_subtile_t* self = *_self;
	if(self)
	{
		LOGD("debug");
		free(self);
		*_self = NULL;
	}
}

static void nedgz_subtile_set(nedgz_subtile_t* self, int m, int n, short h)
{
	assert(self);
	LOGD("debug m=%i, n=%i, h=%i", m, n, (int) h);

	self->data[m*NEDGZ_SUBTILE_SIZE + n] = h;
}

/***********************************************************
* public                                                   *
***********************************************************/

nedgz_tile_t* nedgz_tile_new(int x, int y, int zoom)
{
	assert(x >= 0);
	assert(y >= 0);
	assert(zoom >= 0);
	LOGD("debug x=%i, y=%i, zoom=%i",
	     x, y, zoom);

	nedgz_tile_t* self = (nedgz_tile_t*) malloc(sizeof(nedgz_tile_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	self->x    = x;
	self->y    = y;
	self->zoom = zoom;

	nedgz_tile2coord((float) x, (float) y, zoom,
	               &self->latT, &self->lonL);
	nedgz_tile2coord((float) x + 1.0f, (float) y + 1.0f, zoom,
	               &self->latB, &self->lonR);
	int count = NEDGZ_SUBTILE_COUNT*NEDGZ_SUBTILE_COUNT;
	int size = count*sizeof(nedgz_subtile_t*);
	memset((void*) self->subtile, 0, size);

	return self;
}

void nedgz_tile_delete(nedgz_tile_t** _self)
{
	assert(_self);

	nedgz_tile_t* self = *_self;
	if(self)
	{
		LOGD("debug");

		int i;
		int j;
		for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				nedgz_subtile_t* subtile = nedgz_tile_getij(self, i, j);
				if(subtile)
				{
					nedgz_subtile_delete(&subtile);
				}
			}
		}
		free(self);
		*_self = NULL;
	}
}

nedgz_tile_t* nedgz_tile_import(const char* base,
                                int x, int y, int zoom)
{
	assert(base);
	assert(x >= 0);
	assert(y >= 0);
	assert(zoom >= 0);
	LOGD("debug base=%s, x=%i, y=%i, zoom=%i", base, x, y, zoom);

	nedgz_tile_t* self = nedgz_tile_new(x, y, zoom);
	if(self == NULL)
	{
		return NULL;
	}

	char fname[256];
	snprintf(fname, 256, "%s/%i/%i_%i.nedgz", base, zoom, x, y);
	gzFile f = gzopen(fname, "rb");
	if(f == NULL)
	{
		LOGE("failed %s", fname);
		goto fail_gzopen;
	}

	short count;
	if(gzread(f, &count, sizeof(short)) != sizeof(short))
	{
		LOGE("failed %s", fname);
		goto fail_count;
	}

	if((count < 0) ||
	   (count >= NEDGZ_SUBTILE_COUNT*NEDGZ_SUBTILE_COUNT))
	{
		LOGE("failed count=%i, %s", (int) count, fname);
		goto fail_count;
	}

	int idx;
	for(idx = 0; idx < count; ++idx)
	{
		unsigned char i;
		unsigned char j;
		if(gzread(f, &i, sizeof(i)) != sizeof(i))
		{
			LOGE("failed %s", fname);
			goto fail_ij;
		}
		if(gzread(f, &j, sizeof(j)) != sizeof(j))
		{
			LOGE("failed %s", fname);
			goto fail_ij;
		}

		if((i < 0) || (i >= NEDGZ_SUBTILE_COUNT) ||
		   (j < 0) || (j >= NEDGZ_SUBTILE_COUNT))
		{
			LOGE("failed %s", fname);
			goto fail_ij;
		}

		// make sure i,j doesn't already exist
		nedgz_subtile_t* subtile = nedgz_tile_getij(self, i, j);
		if(subtile)
		{
			LOGE("failed %s", fname);
			goto fail_subtile_exists;
		}

		subtile = nedgz_subtile_new();
		if(subtile == NULL)
		{
			goto fail_subtile_new;
		}
		self->subtile[i*NEDGZ_SUBTILE_COUNT + j] = subtile;

		int bytes = NEDGZ_SUBTILE_SIZE*NEDGZ_SUBTILE_SIZE*sizeof(short);
		unsigned char* data = (unsigned char*) subtile->data;
		while(bytes > 0)
		{
			int bytes_read = gzread(f, data, bytes);
			if(bytes_read == 0)
			{
				LOGE("failed to read data");
				goto fail_data;
			}
			data  += bytes_read;
			bytes -= bytes_read;
		}
	}

	gzclose(f);

	// success
	return self;

	// failure
	fail_data:
		// subtile added to self
	fail_subtile_new:
	fail_subtile_exists:
	fail_ij:
	fail_count:
		gzclose(f);
	fail_gzopen:
		nedgz_tile_delete(&self);
	return NULL;
}

int nedgz_tile_export(nedgz_tile_t* self, const char* base)
{
	assert(self);
	assert(base);

	char dname[256];
	char fname[256];
	snprintf(dname, 256, "%s/%i", base, self->zoom);
	snprintf(fname, 256, "%s/%i_%i.nedgz", dname, self->x, self->y);
	LOGD("debug %s", fname);

	// make sure there are subtiles to write
	short count = 0;
	unsigned char i;
	unsigned char j;
	for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
	{
		for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
		{
			if(nedgz_tile_getij(self, (int) i, (int) j))
			{
				++count;
			}
		}
	}

	if(count == 0)
	{
		// silently succeed
		return 1;
	}

	// create directories if necessary
	if(mkdir(base, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
	{
		if(errno == EEXIST)
		{
			// already exists
		}
		else
		{
			LOGE("mkdir %s failed", base);
			return 0;
		}
	}
	if(mkdir(dname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
	{
		if(errno == EEXIST)
		{
			// already exists
		}
		else
		{
			LOGE("mkdir %s failed", dname);
			return 0;
		}
	}

	gzFile f = gzopen(fname, "wb");
	if(f == NULL)
	{
		LOGE("gzopen failed");
		return 0;
	}

	if(gzwrite(f, (const void*) &count, sizeof(short)) != sizeof(short))
	{
		LOGE("failed to write count");
		goto fail_count;
	}

	// write subtiles
	for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
	{
		for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
		{
			nedgz_subtile_t* subtile = nedgz_tile_getij(self, (int) i, (int) j);
			if(subtile == NULL)
			{
				continue;
			}

			if(gzwrite(f, (const void*) &i, sizeof(unsigned char)) != sizeof(unsigned char))
			{
				LOGE("failed %s", fname);
				goto fail_i;
			}
			if(gzwrite(f, (const void*) &j, sizeof(unsigned char)) != sizeof(unsigned char))
			{
				LOGE("failed %s", fname);
				goto fail_j;
			}

			int bytes = NEDGZ_SUBTILE_SIZE*NEDGZ_SUBTILE_SIZE*sizeof(short);
			unsigned char* data = (unsigned char*) subtile->data;
			while(bytes > 0)
			{
				int bytes_written = gzwrite(f, (const void*) data, bytes);
				if(bytes_written == 0)
				{
					LOGE("failed %s", fname);
					goto fail_data;
				}
				data  += bytes_written;
				bytes -= bytes_written;
			}
		}
	}

	gzclose(f);

	// success
	return 1;

	// failure
	fail_data:
	fail_j:
	fail_i:
	fail_count:
		gzclose(f);
	return 0;
}

nedgz_subtile_t* nedgz_tile_getij(nedgz_tile_t* self, int i, int j)
{
	assert(self);
	assert(i >= 0);
	assert(i < NEDGZ_SUBTILE_COUNT);
	assert(j >= 0);
	assert(j < NEDGZ_SUBTILE_COUNT);
	LOGD("debug i=%i, j=%i", i, j);

	return self->subtile[i*NEDGZ_SUBTILE_COUNT + j];
}

int nedgz_tile_set(nedgz_tile_t* self,
                   int i, int j,
                   int m, int n,
                   short h)
{
	assert(self);
	assert(i >= 0);
	assert(i < NEDGZ_SUBTILE_COUNT);
	assert(j >= 0);
	assert(j < NEDGZ_SUBTILE_COUNT);
	assert(m >= 0);
	assert(m < NEDGZ_SUBTILE_SIZE);
	assert(n >= 0);
	assert(n < NEDGZ_SUBTILE_SIZE);
	LOGD("debug i=%i, j=%i, m=%i, n=%i, h=%i", i, j, m, n, (int) h);

	// create subtile if it does not yet exist
	nedgz_subtile_t* subtile = nedgz_tile_getij(self, i, j);
	if(subtile == NULL)
	{
		subtile = nedgz_subtile_new();
		if(subtile == NULL)
		{
			return 0;
		}
		self->subtile[i*NEDGZ_SUBTILE_COUNT + j] = subtile;
	}

	nedgz_subtile_set(subtile, m, n, h);

	return 1;
}

void nedgz_tile_coord(nedgz_tile_t* self,
                      int i, int j,
                      int m, int n,
                      double* lat, double* lon)
{
	assert(self);
	assert(i >= 0);
	assert(i < NEDGZ_SUBTILE_COUNT);
	assert(j >= 0);
	assert(j < NEDGZ_SUBTILE_COUNT);
	assert(m >= 0);
	assert(m < NEDGZ_SUBTILE_SIZE);
	assert(n >= 0);
	assert(n < NEDGZ_SUBTILE_SIZE);
	LOGD("debug i=%i, j=%i, m=%i, n=%i", i, j, m, n);

	// r allows following condition to hold true
	// (0, 0, 0, 15) == (0, 1, 0, 0)
	double count = (double) NEDGZ_SUBTILE_COUNT;
	double r     = (double) (NEDGZ_SUBTILE_SIZE - 1);
	double id    = (double) i;
	double jd    = (double) j;
	double md    = (double) m;
	double nd    = (double) n;
	double lats  = (jd + nd/r)/count;
	double lons  = (id + md/r)/count;
	double latT  = self->latT;
	double lonL  = self->lonL;
	double latB  = self->latB;
	double lonR  = self->lonR;
	*lat = latT + lats*(latB - latT);
	*lon = lonL + lons*(lonR - lonL);
}
