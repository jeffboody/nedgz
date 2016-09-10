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
#include <math.h>
#include "terrain/terrain_util.h"
#include "texgz/texgz_jp2.h"
#include "naip_node.h"

#define LOG_TAG "naip"
#include "a3d/a3d_log.h"

/***********************************************************
* public                                                   *
***********************************************************/

naip_node_t* naip_node_new(const char* id,
                           const char* url,
                           const char* t,
                           const char* l,
                           const char* b,
                           const char* r)
{
	assert(id);
	assert(url);
	assert(t);
	assert(l);
	assert(b);
	assert(r);

	naip_node_t* self = (naip_node_t*)
	                    malloc(sizeof(naip_node_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	snprintf(self->fname, 256, "naip/%s.jp2", id);
	self->fname[255] = '\0';

	snprintf(self->id, 256, "%s", id);
	self->id[255] = '\0';

	snprintf(self->url, 256, "%s", url);
	self->url[255] = '\0';

	self->t    = strtod(t, NULL);
	self->l    = strtod(l, NULL);
	self->b    = strtod(b, NULL);
	self->r    = strtod(r, NULL);
	self->tex  = NULL;

	return self;
}

void naip_node_delete(naip_node_t** _self)
{
	assert(_self);

	naip_node_t* self = *_self;
	if(self)
	{
		texgz_tex_delete(&self->tex);
		free(self);
		*_self = NULL;
	}
}

int naip_node_import(naip_node_t* self)
{
	assert(self);

	LOGI("import %s", self->fname);

	if(self->tex)
	{
		return 1;
	}

	self->tex = texgz_jp2_import(self->fname);
	return self->tex ? 1 : 0;
}

void naip_node_tilesCovered(naip_node_t* self,
                            int zoom,
                            int* _x0, int* _y0,
                            int* _x1, int* _y1)
{
	assert(self);
	assert(_x0);
	assert(_y0);
	assert(_x1);
	assert(_y1);

	// find the candidate tiles at zoom
	float x0f;
	float y0f;
	float x1f;
	float y1f;
	terrain_coord2tile(self->t, self->l, zoom,
	                   &x0f, &y0f);
	terrain_coord2tile(self->b, self->r, zoom,
	                   &x1f, &y1f);

	// determine range of candidate tiles
	int x0 = (int) x0f;
	int y0 = (int) y0f;
	int x1 = (int) (x1f + 1.0f);
	int y1 = (int) (y1f + 1.0f);

	// check the tile range
	int range = (int) pow(2.0, (double) zoom);
	if(x0 < 0)
	{
		x0 = 0;
	}
	if(y0 < 0)
	{
		y0 = 0;
	}
	if(x1 >= range)
	{
		x1 = range - 1;
	}
	if(y1 >= range)
	{
		y1 = range - 1;
	}

	*_x0 = x0;
	*_y0 = y0;
	*_x1 = x1;
	*_y1 = y1;
}
