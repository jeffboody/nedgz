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
#include <unistd.h>
#include <pthread.h>

#define LOG_TAG "setacl"
#include "nedgz_log.h"

#define MODE_NED        0
#define MODE_OSM        1
#define MODE_HEIGHTMAP  2
#define MODE_BLUEMARBLE 3

#define NTHREAD         8

pthread_t       g_thread[NTHREAD];
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
int             g_mode  = -1;
FILE*           g_f     = NULL;
char*           g_line  = NULL;
int             g_index = 0;

static void runcmd(char* cmd)
{
	while(system(cmd) != 0)
	{
		LOGE("%s failed", cmd);
		usleep(1000000);
	}
}

static void setacl(int index, int zoom, int x, int y)
{
	LOGI("%i: zoom=%i, x=%i, y=%i", index, zoom, x, y);

	char cmd[256];
	if(g_mode == MODE_NED)
	{
		snprintf(cmd, 256, "gsutil acl set -a public-read gs://goto/ned/%i/%i_%i.nedgz", zoom, x, y);
		runcmd(cmd);
	}
	else if(g_mode == MODE_OSM)
	{
		snprintf(cmd, 256, "gsutil acl set -a public-read gs://goto/osm/%i/%i_%i.pak", zoom, x, y);
		runcmd(cmd);
	}
	else if(g_mode == MODE_HEIGHTMAP)
	{
		snprintf(cmd, 256, "gsutil acl set -a public-read gs://goto/hillshade/%i/%i_%i.pak", zoom, x, y);
		runcmd(cmd);
	}
	else if(g_mode == MODE_BLUEMARBLE)
	{
		int month;
		for(month = 1; month <= 12; ++month)
		{
			snprintf(cmd, 256, "gsutil acl set -a public-read gs://goto/bluemarble/%i/%i/%i_%i.pak", month, zoom, x, y);
			runcmd(cmd);
		}
	}
}

static int getnode(int* index, int* zoom, int* x, int* y)
{
	pthread_mutex_lock(&g_mutex);

	int    ret = 0;
	size_t n   = 0;
	ret = getline(&g_line, &n, g_f);
	if(ret <= 0)
	{
		goto fail_getline;
	}

	if(sscanf(g_line, "%i %i %i", zoom, x, y) != 3)
	{
		LOGE("invalid line=%s", g_line);
		ret = 0;
		goto fail_sscanf;
	}

	*index = g_index;
	++g_index;

	// success
	pthread_mutex_unlock(&g_mutex);
	return 1;

	// failure
	fail_sscanf:
	fail_getline:
		pthread_mutex_unlock(&g_mutex);
	return 0;
}

static void* run_thread(void* arg)
{
	int index;
	int zoom;
	int x;
	int y;
	while(getnode(&index, &zoom, &x, &y))
	{
		setacl(index, zoom, x, y);
	}
	return NULL;
}

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		LOGE("usage: %s [data]", argv[0]);
		return EXIT_FAILURE;
	}

	if(strncmp(argv[1], "ned", 256) == 0)
	{
		g_mode = MODE_NED;
	}
	else if(strncmp(argv[1], "osm", 256) == 0)
	{
		g_mode = MODE_OSM;
	}
	else if(strncmp(argv[1], "hillshade", 256) == 0)
	{
		g_mode = MODE_HEIGHTMAP;
	}
	else if(strncmp(argv[1], "bluemarble", 256) == 0)
	{
		g_mode = MODE_BLUEMARBLE;
	}
	else
	{
		LOGE("invalid data=%s", argv[1]);
		return EXIT_FAILURE;
	}

	char fname[256];
	snprintf(fname, 256, "%s/%s.list", argv[1], argv[1]);

	// open the list
	g_f = fopen(fname, "r");
	if(g_f == NULL)
	{
		LOGE("failed to open %s", fname);
		return EXIT_FAILURE;
	}

	int i;
	for(i = 0; i < NTHREAD; ++i)
	{
		if(pthread_create(&g_thread[i], NULL, run_thread, (void*) NULL) != 0)
		{
			LOGW("pthread_create failed");
		}
	}

	for(i = 0; i < NTHREAD; ++i)
	{
		if(g_thread[i])
		{
			pthread_join(g_thread[i], NULL);
		}
	}
	free(g_line);
	fclose(g_f);

	return EXIT_SUCCESS;
}
