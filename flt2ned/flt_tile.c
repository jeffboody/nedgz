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
#include <stdio.h>
#include <string.h>
#include "flt_tile.h"

#define LOG_TAG "flt"
#include "nedgz/nedgz_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

static int keyval(char* s, const char** k, const char** v)
{
	assert(s);
	assert(k);
	assert(v);
	LOGD("debug s=%s", s);

	// find key
	*k = s;
	while(*s != ' ')
	{
		if(*s == '\0')
		{
			// fail silently
			return 0;
		}
		++s;
	}
	*s = '\0';
	++s;

	// find value
	while(*s == ' ')
	{
		if(*s == '\0')
		{
			// skip silently
			return 0;
		}
		++s;
	}
	*v = s;
	++s;

	// find end
	while(*s != '\0')
	{
		if((*s == '\n') ||
		   (*s == '\r') ||
		   (*s == '\t') ||
		   (*s == ' '))
		{
			*s = '\0';
			break;
		}
		++s;
	}

	return 1;
}

static int flt_tile_importhdr(flt_tile_t* self, const char* fname)
{
	assert(self);
	assert(fname);
	LOGD("debug fname=%s", fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		// silently fail if hdr does not exist
		// but if hdr exists then prj and flt
		// must also exist
		return 0;
	}

	const char* key;
	const char* value;
	char        buffer[256];
	int         ncols     = 0;
	int         nrows     = 0;
	double      xllcorner = 0.0;
	double      yllcorner = 0.0;
	double      cellsize  = 0.0;
	float       nodata    = 0.0f;
	int         byteorder = 0;
	while(fgets(buffer, 256, f))
	{
		if(keyval(buffer, &key, &value) == 0)
		{
			// skip silently
			continue;
		}

		if(strcmp(key, "ncols") == 0)
		{
			ncols = (int) strtol(value, NULL, 0);
		}
		else if(strcmp(key, "nrows") == 0)
		{
			nrows = (int) strtol(value, NULL, 0);
		}
		else if(strcmp(key, "xllcorner") == 0)
		{
			xllcorner = strtod(value, NULL);
		}
		else if(strcmp(key, "yllcorner") == 0)
		{
			yllcorner = strtod(value, NULL);
		}
		else if(strcmp(key, "cellsize") == 0)
		{
			cellsize = strtod(value, NULL);
		}
		else if(strcmp(key, "NODATA_value") == 0)
		{
			nodata = strtof(value, NULL);
		}
		else if(strcmp(key, "byteorder") == 0)
		{
			if(strcmp(value, "MSBFIRST") == 0)
			{
				byteorder = FLT_MSBFIRST;
			}
			else if(strcmp(value, "LSBFIRST") == 0)
			{
				byteorder = FLT_LSBFIRST;
			}
		}
		else
		{
			LOGW("unknown key=%s, value=%s", key, value);
		}
	}

	fclose(f);

	// verfy required fields
	if((ncols == 0)       ||
	   (nrows == 0)       ||
	   (cellsize  == 0.0) ||
	   (byteorder == 0))
	{
		LOGE("invalid nrows=%i, ncols=%i, xllcorner=%0.3lf, yllcorner=%0.3lf, cellsize=%0.6lf, byteorder=%i",
		     nrows, ncols, xllcorner, yllcorner, cellsize, byteorder);
		return 0;
	}

	self->latB      = yllcorner;
	self->lonL      = xllcorner;
	self->latT      = yllcorner + (double) nrows*cellsize;
	self->lonR      = xllcorner + (double) ncols*cellsize;
	self->nodata    = nodata;
	self->byteorder = byteorder;
	self->nrows     = nrows;
	self->ncols     = ncols;

	return 1;
}

static int flt_tile_importprj(flt_tile_t* self, const char* fname)
{
	assert(self);
	assert(fname);
	LOGD("debug fname=%s", fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return 0;
	}

	const char* key;
	const char* value;
	char        buffer[256];
	while(fgets(buffer, 256, f))
	{
		if(keyval(buffer, &key, &value) == 0)
		{
			// skip silently
			continue;
		}

		if(strcmp(key, "Projection") == 0)
		{
			if(strcmp(value, "GEOGRAPHIC") != 0)
			{
				LOGW("%s=%s", key, value);
			}
		}
		else if(strcmp(key, "Datum") == 0)
		{
			if(strcmp(value, "NAD83") != 0)
			{
				LOGW("%s=%s", key, value);
			}
		}
		else if(strcmp(key, "Zunits") == 0)
		{
			if(strcmp(value, "METERS") != 0)
			{
				LOGW("%s=%s", key, value);
			}
		}
		else if(strcmp(key, "Units") == 0)
		{
			if(strcmp(value, "DD") != 0)
			{
				LOGW("%s=%s", key, value);
			}
		}
		else if(strcmp(key, "Spheroid") == 0)
		{
			if(strcmp(value, "GRS1980") != 0)
			{
				LOGW("%s=%s", key, value);
			}
		}
		else if(strcmp(key, "Xshift") == 0)
		{
			if(strtod(value, NULL) != 0.0)
			{
				LOGW("%s=%s", key, value);
			}
		}
		else if(strcmp(key, "Yshift") == 0)
		{
			if(strtod(value, NULL) != 0.0)
			{
				LOGW("%s=%s", key, value);
			}
		}
		else if(strcmp(key, "Parameters") == 0)
		{
			// skip
		}
		else
		{
			LOGW("unknown key=%s, value=%s", key, value);
		}
	}

	fclose(f);

	return 1;
}

static int flt_tile_importflt(flt_tile_t* self, const char* fname)
{
	assert(self);
	assert(fname);
	LOGD("debug fname=%s", fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return 0;
	}

	size_t size = self->nrows*self->ncols*sizeof(float);
	self->data = (float*) malloc(size);
	if(self->data == NULL)
	{
		LOGE("malloc failed");
		goto fail_data;
	}

	size_t left = size;
	unsigned char* data = (unsigned char*) self->data;
	while(left > 0)
	{
		size_t bytes = fread(data, sizeof(unsigned char), left, f);
		if(bytes == 0)
		{
			LOGE("fread failed");
			goto fail_read;
		}
		left -= bytes;
		data += bytes;
	}

	// need to swap byte order for big endian
	if(self->byteorder == FLT_MSBFIRST)
	{
		data = (unsigned char*) self->data;

		int i;
		for(i = 0; i < size; i+=4)
		{
			unsigned char d0 = data[i + 0];
			unsigned char d1 = data[i + 1];
			unsigned char d2 = data[i + 2];
			unsigned char d3 = data[i + 3];
			data[i + 0] = d3;
			data[i + 1] = d2;
			data[i + 2] = d1;
			data[i + 3] = d0;
		}
	}

	fclose(f);

	return 1;

	// failure
	fail_read:
		free(self->data);
		self->data = NULL;
	fail_data:
		fclose(f);
	return 0;
}

/***********************************************************
* public                                                   *
***********************************************************/

flt_tile_t* flt_tile_import(int lat, int lon)
{
	LOGD("debug lat=%i, lon=%i", lat, lon);

	char flt_fbase[256];
	char flt_fname[256];
	char hdr_fname[256];
	char prj_fname[256];

	snprintf(flt_fbase, 256, "%s%i%s%i",
	         (lat >= 0) ? "n" : "s", abs(lat),
	         (lon >= 0) ? "e" : "w", abs(lon));
	snprintf(flt_fname, 256, "%s/float%s_1", flt_fbase, flt_fbase);
	snprintf(hdr_fname, 256, "%s.hdr", flt_fname);
	snprintf(prj_fname, 256, "%s.prj", flt_fname);

	flt_tile_t* self = (flt_tile_t*) malloc(sizeof(flt_tile_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return 0;
	}

	// init defaults
	self->lat       = lat;
	self->lon       = lon;
	self->lonL      = 0.0;
	self->latB      = 0.0;
	self->lonR      = 0.0;
	self->latT      = 0.0;
	self->nodata    = -9999.0;
	self->byteorder = FLT_LSBFIRST;
	self->nrows     = 0;
	self->ncols     = 0;
	self->data      = NULL;

	if(flt_tile_importhdr(self, hdr_fname) == 0)
	{
		goto fail_hdr;
	}

	if(flt_tile_importprj(self, prj_fname) == 0)
	{
		goto fail_prj;
	}

	if(flt_tile_importflt(self, flt_fname) == 0)
	{
		goto fail_flt;
	}

	// success
	return self;

	// failure
	fail_flt:
	fail_prj:
	fail_hdr:
		free(self);
	return NULL;
}

void flt_tile_delete(flt_tile_t** _self)
{
	assert(_self);

	flt_tile_t* self = *_self;
	if(self)
	{
		LOGD("debug");

		free(self->data);
		free(self);
		*_self = NULL;
	}
}

int flt_tile_sample(flt_tile_t* self,
                    double lat, double lon,
                    float* height)
{
	assert(self);
	assert(height);
	LOGD("debug lat=%lf, lon=%lf", lat, lon);

	double lonv = (lon - self->lonL) / (self->lonR - self->lonL);
	double latu = (lat - self->latB) / (self->latT - self->latB);
	if((lonv >= 0.0) && (lonv <= 1.0) &&
	   (latu >= 0.0) && (latu <= 1.0))
	{
		// TODO - interpolate tile
		int lat = (int) (latu*self->ncols);
		int lon = (int) (lonv*self->ncols);
		*height = self->data[lon*self->ncols + lat];
		return 1;
	}

	return 0;
}
