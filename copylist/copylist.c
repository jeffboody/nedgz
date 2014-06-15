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
#include <string.h>
#include "nedgz/nedgz_util.h"

#define LOG_TAG "copylist"
#include "nedgz/nedgz_log.h"

#define MODE_NED        0
#define MODE_OSM        1
#define MODE_BLUEMARBLE 2
#define MODE_HILLSHADE  3

int main(int argc, char** argv)
{
	if(argc != 8)
	{
		LOGE("usage: %s [mode] [zmin] [zmax] [latT] [lonL] [latB] [lonR]", argv[0]);
		LOGE("mode: -ned, -osm, -bluemarble, -hillshade");
		return EXIT_FAILURE;
	}

	int    zmin = (int) strtol(argv[2], NULL, 0);
	int    zmax = (int) strtol(argv[3], NULL, 0);
	double latT = strtod(argv[4], NULL);
	double lonL = strtod(argv[5], NULL);
	double latB = strtod(argv[6], NULL);
	double lonR = strtod(argv[7], NULL);

	int mode;
	int month;
	if(strcmp(argv[1], "-ned") == 0)
	{
		mode = MODE_NED;

		printf("mkdir drive/ned\n");
	}
	else if(strcmp(argv[1], "-osm") == 0)
	{
		mode = MODE_OSM;

		printf("mkdir drive/osm\n");
	}
	else if(strcmp(argv[1], "-bluemarble") == 0)
	{
		mode = MODE_BLUEMARBLE;

		printf("mkdir drive/bluemarble\n");
		for(month = 1; month <= 12; ++month)
		{
			printf("mkdir drive/bluemarble/%i\n", month);
		}
	}
	else if(strcmp(argv[1], "-hillshade") == 0)
	{
		mode = MODE_HILLSHADE;

		printf("mkdir drive/hillshade\n");
	}
	else
	{
		LOGE("usage: %s [mode] [zmin] [zmax] [latT] [lonL] [latB] [lonR]", argv[0]);
		LOGE("mode: -ned, -osm, -bluemarble, -hillshade");
		return EXIT_FAILURE;
	}

	int zoom;
	for(zoom = zmin; zoom <= zmax; ++zoom)
	{
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

		int x0 = (int) x0f;
		int y0 = (int) y0f;
		int x1 = (int) x1f;
		int y1 = (int) y1f;

		if(mode == MODE_BLUEMARBLE)
		{
			for(month = 1; month <= 12; ++month)
			{
				printf("mkdir drive/bluemarble/%i/%i\n", month, zoom);

				int x;
				int y;
				for(y = y0; y <= y1; ++y)
				{
					for(x = x0; x <= x1; ++x)
					{
						printf("cp bluemarble/%i/%i/%i_%i.pak drive/bluemarble/%i/%i/\n", month, zoom, x, y, month, zoom);
					}
				}
			}
		}
		else if(mode == MODE_OSM)
		{
			printf("mkdir drive/osm/%i\n", zoom);

			int x;
			int y;
			for(y = y0; y <= y1; ++y)
			{
				for(x = x0; x <= x1; ++x)
				{
					printf("cp osm/%i/%i_%i.pak drive/osm/%i/\n", zoom, x, y, zoom);
				}
			}
		}
		else if(mode == MODE_NED)
		{
			printf("mkdir drive/ned/%i\n", zoom);

			int x;
			int y;
			for(y = y0; y <= y1; ++y)
			{
				for(x = x0; x <= x1; ++x)
				{
					printf("cp ned/%i/%i_%i.nedgz drive/ned/%i/\n", zoom, x, y, zoom);
				}
			}
		}
		else if(mode == MODE_HILLSHADE)
		{
			printf("mkdir drive/hillshade/%i\n", zoom);

			int x;
			int y;
			for(y = y0; y <= y1; ++y)
			{
				for(x = x0; x <= x1; ++x)
				{
					printf("cp hillshade/%i/%i_%i.pak drive/hillshade/%i/\n", zoom, x, y, zoom);
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
