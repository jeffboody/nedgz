/*
 * Copyright (c) 2016 Jeff Boody
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
#include <unistd.h>
#include "naip_tile.h"
#include "naip_util.h"

#define LOG_TAG "naip"
#include "a3d/a3d_log.h"

/***********************************************************
* public                                                   *
***********************************************************/

naip_tile_t* naip_tile_new(int x, int y)
{
	naip_tile_t* self = (naip_tile_t*)
	                    malloc(sizeof(naip_tile_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	self->x = x;
	self->y = y;

	self->tex = texgz_tex_new(256, 256, 256, 256,
	                          TEXGZ_UNSIGNED_BYTE,
	                          TEXGZ_RGBA, NULL);
	if(self->tex == NULL)
	{
		goto fail_tex;
	}

	// success
	return self;

	// failure
	fail_tex:
		free(self);
	return NULL;
}

void naip_tile_delete(naip_tile_t** _self)
{
	assert(_self);

	naip_tile_t* self = *_self;
	if(self)
	{
		// remove cached tex
		char fname[256];
		snprintf(fname, 256, "cache/17/%i/%i/texz",
		         self->x, self->y);
		fname[255] = '\0';
		unlink(fname);

		texgz_tex_delete(&self->tex);
		free(self);
		*_self = NULL;
	}
}

int naip_tile_compare(const void* _a, const void* _b)
{
	assert(_a);
	assert(_b);

	naip_tile_t* a = (naip_tile_t*) _a;
	naip_tile_t* b = (naip_tile_t*) _b;

	if((a->x == b->x) &&
	   (a->y == b->y))
	{
		return 0;
	}

	return 1;
}

texgz_tex_t* naip_tile_cacheRestore(naip_tile_t* self)
{
	assert(self);

	// check if tex was cached
	if(self->tex)
	{
		return self->tex;
	}

	char fname[256];
	snprintf(fname, 256, "cache/17/%i/%i.texz",
	         self->x, self->y);
	fname[255] = '\0';

	self->tex = texgz_tex_importz(fname);
	unlink(fname);

	return self->tex;;
}

int naip_tile_cacheSave(naip_tile_t* self)
{
	assert(self);

	// check if tex was cached
	if(self->tex == NULL)
	{
		return 1;
	}

	char fname[256];
	snprintf(fname, 256, "cache/%i/%i/%i.texz",
	         17, self->x, self->y);
	fname[255] = '\0';

	char pname[256];
	snprintf(pname, 256, "%s.part", fname);
	pname[255] = '\0';

	if(naip_mkdir(pname) == 0)
	{
		return 0;
	}

	if(texgz_tex_exportz(self->tex, pname) == 0)
	{
		return 0;
	}

	if(rename(pname, fname) != 0)
	{
		LOGE("rename failed %s", fname);
		goto fail_rename;
	}

	texgz_tex_delete(&self->tex);

	// success
	return 1;

	// failure
	fail_rename:
		unlink(pname);
	return 0;
}

int naip_tile_cacheSize(naip_tile_t* self)
{
	assert(self);

	// check if tex was cached
	if(self->tex == NULL)
	{
		return 0;
	}

	return texgz_tex_size(self->tex);
}
