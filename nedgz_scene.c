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
#include "nedgz_tile.h"
#include "nedgz_scene.h"

#define LOG_TAG "nedgz"
#include "nedgz_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

#define NEDGZ_SCENE_TL     0x01
#define NEDGZ_SCENE_TR     0x02
#define NEDGZ_SCENE_BL     0x04
#define NEDGZ_SCENE_BR     0x08
#define NEDGZ_SCENE_EXISTS 0x10

static int nedgz_scene_exportf(nedgz_scene_t* self, FILE* f)
{
	assert(self);
	assert(f);
	LOGD("debug");

	short mask = 0;
	mask |= self->tl     ? NEDGZ_SCENE_TL     : 0;
	mask |= self->tr     ? NEDGZ_SCENE_TR     : 0;
	mask |= self->bl     ? NEDGZ_SCENE_BL     : 0;
	mask |= self->br     ? NEDGZ_SCENE_BR     : 0;
	mask |= self->exists ? NEDGZ_SCENE_EXISTS : 0;

	if(fwrite((const void*) &self->min, sizeof(short), 1, f) != 1)
	{
		LOGE("fwrite failed");
		return 0;
	}

	if(fwrite((const void*) &self->max, sizeof(short), 1, f) != 1)
	{
		LOGE("fwrite failed");
		return 0;
	}

	if(fwrite((const void*) &mask, sizeof(short), 1, f) != 1)
	{
		LOGE("fwrite failed");
		return 0;
	}

	if(self->tl)
	{
		if(nedgz_scene_exportf(self->tl, f) == 0)
		{
			return 0;
		}
	}

	if(self->tr)
	{
		if(nedgz_scene_exportf(self->tr, f) == 0)
		{
			return 0;
		}
	}

	if(self->bl)
	{
		if(nedgz_scene_exportf(self->bl, f) == 0)
		{
			return 0;
		}
	}

	if(self->br)
	{
		if(nedgz_scene_exportf(self->br, f) == 0)
		{
			return 0;
		}
	}

	return 1;
}

static int nedgz_scene_importf(nedgz_scene_t** _self, FILE* f)
{
	// _self can be NULL for new leaf nodes
	assert(f);
	LOGD("debug");

	nedgz_scene_t* self = *_self;
	if(self == NULL)
	{
		self = nedgz_scene_new();
		if(self == NULL)
		{
			return 0;
		}
		*_self = self;
	}

	if(fread((void*) &self->min, sizeof(short), 1, f) != 1)
	{
		return 0;
	}

	if(fread((void*) &self->max, sizeof(short), 1, f) != 1)
	{
		return 0;
	}

	short mask;
	if(fread((void*) &mask, sizeof(short), 1, f) != 1)
	{
		return 0;
	}

	self->exists = (mask & NEDGZ_SCENE_EXISTS) ? 1 : 0;

	if(mask & NEDGZ_SCENE_TL)
	{
		if(nedgz_scene_importf(&self->tl, f) == 0)
		{
			return 0;
		}
	}

	if(mask & NEDGZ_SCENE_TR)
	{
		if(nedgz_scene_importf(&self->tr, f) == 0)
		{
			return 0;
		}
	}

	if(mask & NEDGZ_SCENE_BL)
	{
		if(nedgz_scene_importf(&self->bl, f) == 0)
		{
			return 0;
		}
	}

	if(mask & NEDGZ_SCENE_BR)
	{
		if(nedgz_scene_importf(&self->br, f) == 0)
		{
			return 0;
		}
	}

	return 1;
}

/***********************************************************
* public                                                   *
***********************************************************/

nedgz_scene_t* nedgz_scene_new(void)
{
	LOGD("debug");

	nedgz_scene_t* self = (nedgz_scene_t*) malloc(sizeof(nedgz_scene_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	self->tl     = NULL;
	self->tr     = NULL;
	self->bl     = NULL;
	self->br     = NULL;
	self->exists = 0;
	self->min    = NEDGZ_NODATA;
	self->max    = NEDGZ_NODATA;

	return self;
}

void nedgz_scene_delete(nedgz_scene_t** _self)
{
	assert(_self);

	nedgz_scene_t* self = *_self;
	if(self)
	{
		LOGD("debug");

		nedgz_scene_delete(&self->tl);
		nedgz_scene_delete(&self->tr);
		nedgz_scene_delete(&self->bl);
		nedgz_scene_delete(&self->br);
		free(self);
		*_self = NULL;
	}
}

nedgz_scene_t* nedgz_scene_import(const char* fname)
{
	assert(fname);
	LOGD("debug fname=%s", fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return NULL;
	}

	nedgz_scene_t* self = NULL;
	if(nedgz_scene_importf(&self, f) == 0)
	{
		goto fail_importf;
	}

	fclose(f);

	// success
	return self;

	// failure
	fail_importf:
		nedgz_scene_delete(&self);
	return NULL;
}

int nedgz_scene_export(nedgz_scene_t* self, const char* fname)
{
	FILE* f = fopen(fname, "w");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return 0;
	}

	int ret = nedgz_scene_exportf(self, f);
	fclose(f);
	return ret;
}
