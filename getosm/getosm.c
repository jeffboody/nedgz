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
#include "net/net_socket.h"
#include "net/net_socket_wget.h"

#define LOG_TAG "getosm"
#include "net/net_log.h"

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

	// connect to addr
	net_socket_t* sock = net_socket_connect("localhost",
	                                        "80",
	                                        NET_SOCKET_TCP);
	if(sock == NULL)
	{
		goto fail_socket;
	}

	// iteratively download osm images
	char*  line  = NULL;
	size_t n     = 0;
	int    index = 0;
	char*  data  = NULL;
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
		snprintf(dname, 256, "localhost/osm/%i/%i", zoom, x);
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

		// wget data
		int   size = 0;
		char request[256];
		snprintf(request, 256, "/osm/%i/%i/%i.png", zoom, x, y);
		if(net_socket_wget(sock, "getosm/1.0", request, 1,
		                   &size, (void**) &data) == 0)
		{
			LOGE("wget failed for zoom=%i, x=%i, y=%i", zoom, x, y);
			continue;
		}

		// save data
		char fname[256];
		snprintf(fname, 256, "localhost/osm/%i/%i/%i.png", zoom, x, y);
		FILE* fdata = fopen(fname, "w");
		if(fdata == NULL)
		{
			LOGE("fopen failed for zoom=%i, x=%i, y=%i", zoom, x, y);
			continue;
		}

		if(fwrite(data, size, 1, fdata) != 1)
		{
			LOGE("fwrite failed for zoom=%i, x=%i, y=%i", zoom, x, y);
			fclose(fdata);
			continue;
		}
		fclose(fdata);
	}
	free(line);
	free(data);
	net_socket_close(&sock);

	// success
	return EXIT_SUCCESS;

	// failure
	fail_socket:
		fclose(f);
	return EXIT_FAILURE;
}
