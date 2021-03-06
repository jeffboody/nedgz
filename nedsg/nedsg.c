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
#include <math.h>
#include <string.h>
#include "nedgz/nedgz_scene.h"
#include "nedgz/nedgz_tile.h"
#include "nedgz/nedgz_util.h"

#define LOG_TAG "nedsg"
#include "nedgz/nedgz_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

static nedgz_scene_t* make_scene(nedgz_scene_t** _node,
                                 int fsize,
                                 int x,  int y,  int zoom,
                                 nedgz_tile_t* ned)
{
	// *_node may be NULL for new leaf nodes
	assert(_node);
	assert(ned);
	LOGD("debug x=%i, y=%i, zoom=%i", x, y, zoom);

	// when traversing to a new leaf create the node
	nedgz_scene_t* node = *_node;
	if(node == NULL)
	{
		node = nedgz_scene_new();
		if(node == NULL)
		{
			return NULL;
		}
		*_node = node;
	}

	// when traversing the scene the only way to reach
	// the same zoom is when the node is the one we want
	if(zoom == ned->zoom)
	{
		// compute the min/max height
		int i;
		int j;
		int m;
		int n;
		short height;
		for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				for(m = 0; m < NEDGZ_SUBTILE_SIZE; ++m)
				{
					for(n = 0; n < NEDGZ_SUBTILE_SIZE; ++n)
					{
						nedgz_tile_height(ned, i, j, m, n, &height);

						if(height == NEDGZ_NODATA)
						{
							// ignore
							continue;
						}

						if((node->min == NEDGZ_NODATA) ||
						   (node->min > height))
						{
							node->min = height;
						}

						if((node->max == NEDGZ_NODATA) ||
						   (node->max < height))
						{
							node->max = height;
						}
					}
				}
			}
		}

		node->exists = 1;
		node->fsize  = fsize;

		return node;
	}

	// which way to traverse to ned
	int next_x    = 2*x;
	int next_y    = 2*y;
	int next_zoom = zoom + 1;
	int s         = (int) powl(2, ned->zoom - next_zoom);
	x             = ned->x / s;
	y             = ned->y / s;
	zoom          = next_zoom;

	nedgz_scene_t** next;
	if((next_x == x) && (next_y == y))
	{
		next  = &node->tl;
	}
	else if(((next_x + 1) == x) && (next_y == y))
	{
		next  = &node->tr;
	}
	else if((next_x == x) && ((next_y + 1) == y))
	{
		next  = &node->bl;
	}
	else if(((next_x + 1) == x) && ((next_y + 1) == y))
	{
		next  = &node->br;
	}
	else
	{
		LOGE("failed to traverse from %i,%i,%i to %i,%i,%i",
		     x, y, zoom, ned->x, ned->y, ned->zoom);
		return NULL;
	}

	return make_scene(next, fsize, x, y, zoom, ned);
}

static void nedgz_scene_fixheight(nedgz_scene_t* self, short* min, short* max)
{
	// allow self to be NULL
	assert(min);
	assert(max);
	LOGD("debug");

	if(self == NULL)
	{
		return;
	}

	nedgz_scene_fixheight(self->tl, &self->min, &self->max);
	nedgz_scene_fixheight(self->tr, &self->min, &self->max);
	nedgz_scene_fixheight(self->bl, &self->min, &self->max);
	nedgz_scene_fixheight(self->br, &self->min, &self->max);

	if((self->min == NEDGZ_NODATA) ||
	   (self->max == NEDGZ_NODATA))
	{
		// ignore
		return;
	}

	if((*min == NEDGZ_NODATA) || (self->min < *min))
	{
		*min = self->min;
	}

	if((*max == NEDGZ_NODATA) || (self->max > *max))
	{
		*max = self->max;
	}
}

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, char** argv)
{
	// 1. to create the list
	//     cd ned
	//     find . -name "*.nedgz" > ned.list
	// 2. trim list elements to "x y zoom"
	// 3. to create scene graph for ned
	//     cd ned
	//     <path>/nedsg -ned ned.list ned.sg
	// 4. to create scene graph for osm/blue/etc.
	//     cd osm
	//     <path>/nedsg osm.list osm.sg
	if((argc < 3) || (argc > 4))
	{
		LOGE("usage: %s [-ned] in.list out.sg", argv[0]);
		LOGE("-ned: import nedgz file for min/max height");
		return EXIT_FAILURE;
	}

	int   usened = 0;
	char* lname  = argv[1];
	char* sname  = argv[2];
	if(argc == 4)
	{
		usened = 1;
		lname  = argv[2];
		sname  = argv[3];

		if(strcmp(argv[1], "-ned") != 0)
		{
			LOGE("usage: %s [-ned] in.list out.sg", argv[0]);
			return EXIT_FAILURE;
		}
	}

	// open the list
	FILE* f = fopen(lname, "r");
	if(f == NULL)
	{
		LOGE("failed to open %s", lname);
		return EXIT_FAILURE;
	}

	// iteratively add nodes to the scene graph
	char*          line  = NULL;
	size_t         n     = 0;
	int            index = 0;
	nedgz_scene_t* scene = NULL;
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

		LOGI("%i: %i %i %i", ++index, zoom, x, y);

		char          fname[256];
		nedgz_tile_t* ned;
		if(usened)
		{
			ned = nedgz_tile_import(".", x, y, zoom);
			snprintf(fname, 256, "%i/%i_%i.nedgz", zoom, x, y);
		}
		else
		{
			ned = nedgz_tile_new(x, y, zoom);
			snprintf(fname, 256, "%i/%i_%i.pak", zoom, x, y);
		}

		if(ned == NULL)
		{
			LOGE("invalid line=%s", line);
			continue;
		}

		int   fsize = 0;
		FILE* f     = fopen(fname, "r");
		if(f)
		{
			fseek(f, 0, SEEK_END);
			fsize = (int) ftell(f);
			if(fsize < 0)
			{
				LOGE("failed line=%s", line);
				fsize = 0;
			}
			fclose(f);
		}

		make_scene(&scene, fsize, 0, 0, 0, ned);
		nedgz_tile_delete(&ned);
	}
	free(line);

	// fix min/max heights across LOD
	short min = NEDGZ_NODATA;
	short max = NEDGZ_NODATA;
	nedgz_scene_fixheight(scene, &min, &max);

	nedgz_scene_export(scene, sname);
	nedgz_scene_delete(&scene);

	return EXIT_SUCCESS;
}
