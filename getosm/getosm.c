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
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "net/net_socket.h"
#include "net/net_socket_wget.h"

#define LOG_TAG "getosm"
#include "net/net_log.h"

#define SUBTILE_COUNT 8
#define RETRY_DELAY   100000   // 100ms

int main(int argc, char** argv)
{
	// osm.list
	// zoom x y
	if(argc != 2)
	{
		LOGE("usage: %s [osm.list]", argv[0]);
		return EXIT_FAILURE;
	}

	// create directories if necessary
	char dname[256];
	snprintf(dname, 256, "%s", "localhost");
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
	snprintf(dname, 256, "%s", "localhost/osm");
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

	// open the list
	FILE* f = fopen(argv[1], "r");
	if(f == NULL)
	{
		LOGE("failed to open %s", argv[1]);
		return EXIT_FAILURE;
	}

	// iteratively download osm images
	char*         line  = NULL;
	size_t        n     = 0;
	int           index = 0;
	char*         data  = NULL;
	int           size  = 0;
	net_socket_t* sock  = NULL;
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

		LOGI("%i: zoom=%i, x=%i, y=%i", index++, zoom, x, y);

		// create directories if necessary
		char dname[256];
		snprintf(dname, 256, "localhost/osm/%i", zoom);
		if(mkdir(dname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if(errno == EEXIST)
			{
				// already exists
			}
			else
			{
				LOGE("mkdir %s failed", dname);
				continue;
			}
		}

		int i;
		int j;
		for(i = 0; i < SUBTILE_COUNT; ++i)
		{
			int yi = SUBTILE_COUNT*y + i;
			for(j = 0; j < SUBTILE_COUNT; ++j)
			{
				int xj = SUBTILE_COUNT*x + j;
				snprintf(dname, 256, "localhost/osm/%i/%i", zoom, xj);
				if(mkdir(dname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
				{
					if(errno == EEXIST)
					{
						// already exists
					}
					else
					{
						LOGE("mkdir %s failed", dname);
						continue;
					}
				}

				// osm server fails sometimes even if tile exists but since
				// all tiles in US should exist just retry until succcess
				while(1)
				{
					// connect to addr
					if(sock == NULL)
					{
						sock = net_socket_connect("localhost",
						                          "80",
						                          NET_SOCKET_TCP);
						if(sock == NULL)
						{
							LOGE("FAILURE zoom=%i, x=%i, y=%i, xj=%i, yi=%i",
							     zoom, x, y, xj, yi);
							usleep(RETRY_DELAY);
							continue;
						}
					}

					// wget data
					// request to keep socket open but server may close
					// when we have read "too much" data so just retry
					char request[256];
					snprintf(request, 256, "/osm/%i/%i/%i.png", zoom, xj, yi);
					if(net_socket_wget(sock, "getosm/1.0", request, 0,
					                   &size, (void**) &data) == 0)
					{
						LOGE("FAILURE request=%s", request);
						net_socket_close(&sock);
						usleep(RETRY_DELAY);
						continue;
					}
					LOGI("SUCCESS request=%s", request);
					break;
				}

				// save data
				char fname[256];
				snprintf(fname, 256, "localhost/osm/%i/%i/%i.png", zoom, xj, yi);
				FILE* fdata = fopen(fname, "w");
				if(fdata == NULL)
				{
					LOGE("fopen failed fname=%s", fname);
					continue;
				}

				if(fwrite(data, size, 1, fdata) != 1)
				{
					LOGE("fwrite failed fname=%s", fname);
					fclose(fdata);
					continue;
				}
				fclose(fdata);
			}
		}
	}
	net_socket_close(&sock);
	free(line);
	free(data);
	fclose(f);

	return EXIT_SUCCESS;
}
