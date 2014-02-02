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
#include "nedgz/nedgz_scene.h"

static void getosm(nedgz_scene_t* scene, int x, int y, int zoom)
{
	assert(scene);

	if(scene->exists)
	{
		int i;
		int j;

		for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				int xj = NEDGZ_SUBTILE_COUNT*x + j;
				int yi = NEDGZ_SUBTILE_COUNT*y + i;
				printf("wget -x http://localhost/osm/%i/%i/%i.png\n",
				       zoom, xj, yi);
			}
		}
	}

	if(scene->tl)
	{
		getosm(scene->tl, 2*x, 2*y, zoom + 1);
	}
	if(scene->tr)
	{
		getosm(scene->tr, 2*x + 1, 2*y, zoom + 1);
	}
	if(scene->bl)
	{
		getosm(scene->bl, 2*x, 2*y + 1, zoom + 1);
	}
	if(scene->br)
	{
		getosm(scene->br, 2*x + 1, 2*y + 1, zoom + 1);
	}
}

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		printf("%s ned.sg", argv[0]);
		return EXIT_FAILURE;
	}

	nedgz_scene_t* scene = nedgz_scene_import(argv[1]);
	if(scene == NULL)
	{
		return EXIT_FAILURE;
	}

	getosm(scene, 0, 0, 0);

	nedgz_scene_delete(&scene);
	return EXIT_SUCCESS;
}
