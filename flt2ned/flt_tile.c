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

static float meters2feet(float m)
{
	return m*5280.0f/1609.344f;
}

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
		// skip silently
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
	self->latT      = yllcorner + (double) ncols*cellsize;
	self->lonR      = xllcorner + (double) nrows*cellsize;
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
		// skip silently
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
		// skip silently
		return 0;
	}

	size_t size = self->nrows*self->ncols*sizeof(short);
	self->height = (short*) malloc(size);
	if(self->height == NULL)
	{
		LOGE("malloc failed");
		goto fail_height;
	}

	size_t rsize = self->ncols*sizeof(float);
	float* rdata = (float*) malloc(rsize);
	if(rdata == NULL)
	{
		LOGE("malloc failed");
		goto fail_rdata;
	}

	int row;
	for(row = 0; row < self->nrows; ++row)
	{
		size_t left = rsize;
		unsigned char* data = (unsigned char*) rdata;
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
		data = (unsigned char*) rdata;
		if(self->byteorder == FLT_MSBFIRST)
		{
			int i;
			for(i = 0; i < self->ncols; ++i)
			{
				unsigned char d0 = data[4*i + 0];
				unsigned char d1 = data[4*i + 1];
				unsigned char d2 = data[4*i + 2];
				unsigned char d3 = data[4*i + 3];
				data[4*i + 0] = d3;
				data[4*i + 1] = d2;
				data[4*i + 2] = d1;
				data[4*i + 3] = d0;

				// convert data to feet
				float* height = (float*) &data[4*i];
				self->height[row*self->ncols + i] = (short) (meters2feet(*height) + 0.5f);
			}
		}
		else
		{
			int i;
			for(i = 0; i < self->ncols; ++i)
			{
				// convert data to feet
				float* height = (float*) &data[4*i];
				self->height[row*self->ncols + i] = (short) (meters2feet(*height) + 0.5f);
			}
		}
	}

	free(rdata);
	fclose(f);

	return 1;

	// failure
	fail_read:
		free(rdata);
		rdata = NULL;
	fail_rdata:
		free(self->height);
		self->height = NULL;
	fail_height:
		fclose(f);
	return 0;
}

/***********************************************************
* public                                                   *
***********************************************************/

flt_tile_t* flt_tile_import(int arcs, int lat, int lon)
{
	LOGD("debug arcs=%i, lat=%i, lon=%i", arcs, lat, lon);

	char flt_fbase[256];
	char flt_fname[256];
	char hdr_fname[256];
	char prj_fname[256];

	snprintf(flt_fbase, 256, "%s%i%s%i",
	         (lat >= 0) ? "n" : "s", abs(lat),
	         (lon >= 0) ? "e" : "w", abs(lon));
	snprintf(flt_fname, 256, "%s/float%s_%i", flt_fbase, flt_fbase, arcs);
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
	self->height    = NULL;

	if(flt_tile_importhdr(self, hdr_fname) == 0)
	{
		// silently fail if hdr does not exist
		// but if hdr exists then prj and flt
		// must also exist
		goto fail_hdr;
	}

	if(flt_tile_importprj(self, prj_fname) == 0)
	{
		LOGE("flt_tile_importprj %s failed", prj_fname);
		goto fail_prj;
	}

	if(flt_tile_importflt(self, flt_fname) == 0)
	{
		// filenames in source files are inconsistent
		snprintf(flt_fname, 256, "%s/float%s_%i.flt", flt_fbase, flt_fbase, arcs);
		if(flt_tile_importflt(self, flt_fname) == 0)
		{
			LOGE("flt_tile_importflt %s failed", flt_fname);
			goto fail_flt;
		}
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

		free(self->height);
		free(self);
		*_self = NULL;
	}
}

int flt_tile_sample(flt_tile_t* self,
                    double lat, double lon,
                    short* height)
{
	assert(self);
	assert(height);
	LOGD("debug lat=%lf, lon=%lf", lat, lon);

	double lonu = (lon - self->lonL) / (self->lonR - self->lonL);
	double latv = 1.0 - ((lat - self->latB) / (self->latT - self->latB));
	if((lonu >= 0.0) && (lonu <= 1.0) &&
	   (latv >= 0.0) && (latv <= 1.0))
	{
		// interpolate between height samples
		float lon   = (float) (lonu*self->ncols);
		float lat   = (float) (latv*self->nrows);
		int   lon0  = (int) (lonu*self->ncols);
		int   lat0  = (int) (latv*self->nrows);
		int   lon1  = (int) (lonu*self->ncols) + 1;
		int   lat1  = (int) (latv*self->nrows) + 1;
		int   lon0f = (int) lon0;
		int   lat0f = (int) lat0;
		int   lon1f = (int) lon1;
		int   lat1f = (int) lat1;
		float u     = (lon - lon0f)/(lon1f - lon0f);
		float v     = (lat - lat0f)/(lat1f - lat0f);

		// top-left
		// top-right
		// bottom-left
		// bottom-right
		float h00   = (float) self->height[lat0*self->ncols + lon0];
		float h01   = (float) self->height[lat0*self->ncols + lon1];
		float h10   = (float) self->height[lat1*self->ncols + lon0];
		float h11   = (float) self->height[lat1*self->ncols + lon1];

		// top-left    to top-right
		// bottom-left to bottom-right
		// top-center  to bottom-center
		float h0001 = h00 + u*(h01 - h00);
		float h1011 = h10 + u*(h11 - h10);
		*height = (short) (h0001 + v*(h1011 - h0001) + 0.5f);

		return 1;
	}

	return 0;
}
