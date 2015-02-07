/*
 * Copyright (c) 2015 Jeff Boody
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
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include "nedgz/nedgz_tile.h"
#include "nedgz/nedgz_util.h"
#include "net/net_socket.h"
#include "net/net_socket_wget.h"

#define LOG_TAG "mq-sync"
#include "nedgz/nedgz_log.h"

#define MQ_STATE_THREADS 16
typedef struct
{
	int             idx;
	int             cnt;
	int             x;
	int             y;
	int             x0;
	int             y0;
	int             x1;
	int             y1;
	int             zoom;
	int             quit;
	pthread_mutex_t mutex;
	pthread_t       thread[MQ_STATE_THREADS];
} mq_state_t;

mq_state_t* gstate = NULL;

static void mq_state_quit(int signo)
{
	assert(gstate);

	if(signo != SIGINT)
	{
		return;
	}

	pthread_mutex_lock(&gstate->mutex);
	LOGI("QUIT");
	gstate->quit = 1;
	pthread_mutex_unlock(&gstate->mutex);
}

static int mq_state_isQuit(int id)
{
	LOGD("debug id=%i", id);

	pthread_mutex_lock(&gstate->mutex);
	int quit = gstate->quit;
	if(quit)
	{
		LOGI("id=%i: QUIT", id);
	}
	pthread_mutex_unlock(&gstate->mutex);
	return quit;
}

static int mq_state_next(int id, int* x, int* y)
{
	assert(x);
	assert(y);
	LOGD("debug id=%i", id);

	pthread_mutex_lock(&gstate->mutex);

	// quit thread
	if(gstate->quit)
	{
		LOGI("id=%i: QUIT", id);
		pthread_mutex_unlock(&gstate->mutex);
		return 0;
	}

	// increment idx
	if(gstate->idx >= gstate->cnt)
	{
		pthread_mutex_unlock(&gstate->mutex);
		return 0;
	}
	++gstate->idx;

	// increment x, y except for the first step
	if(gstate->idx > 1)
	{
		++gstate->x;
		if(gstate->x > gstate->x1)
		{
			gstate->x = gstate->x0;
			++gstate->y;
		}
	}

	// update state
	*x = gstate->x;
	*y = gstate->y;

	pthread_mutex_unlock(&gstate->mutex);
	return 1;
}

static int mq_sync(int id, int xx, int yy)
{
	LOGD("debug id=%i, xx=%i, yy=%i", id, xx, yy);

	// skip files if they already exist
	char fname[256];
	snprintf(fname, 256, "mq/%i/%i/%i.jpg", gstate->zoom, xx, yy); 
	if(access(fname, R_OK) == 0)
	{
		LOGI("id=%i: %i/%i, %s (SKIP)",
		     id, gstate->idx, gstate->cnt, fname);
		return 1;
	}

	/*
	 * create directories
	 */

	char dir[256];
	snprintf(dir, 256, "%s", "mq");
	if(access(dir, R_OK) != 0)
	{
		if(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if(errno == EEXIST)
			{
				// already exists
			}
			else
			{
				LOGE("mkdir %s failed", dir);
				return 0;
			}
		}
	}

	snprintf(dir, 256, "%s/%i", "mq", gstate->zoom);
	if(access(dir, R_OK) != 0)
	{
		if(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if(errno == EEXIST)
			{
				// already exists
			}
			else
			{
				LOGE("mkdir %s failed", dir);
				return 0;
			}
		}
	}

	snprintf(dir, 256, "%s/%i/%i", "mq", gstate->zoom, xx);
	if(access(dir, R_OK) != 0)
	{
		if(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
		{
			if(errno == EEXIST)
			{
				// already exists
			}
			else
			{
				LOGE("mkdir %s failed", dir);
				return 0;
			}
		}
	}

	// connect to MapQuest storage
	// e.g. http://otile1.mqcdn.com/tiles/1.0.0/sat/15/5240/12661.jpg
	char server[256];
	snprintf(server, 256, "otile%i.mqcdn.com", (id%4) + 1);
	net_socket_t* s = net_socket_connect(server, "80", NET_SOCKET_TCP);
	if(s == NULL)
	{
		return 0;
	}

	// wget file
	int   size = 0;
	void* data = NULL;
	char request[256];
	snprintf(request, 256, "/tiles/1.0.0/sat/%i/%i/%i.jpg",
	         gstate->zoom, xx, yy);
	if(net_socket_wget(s, "goto/1.0", request,
	                   1, &size, &data) == 0)
	{
		goto fail_wget;
	}

	// open file.part
	char pname[256];
	snprintf(pname, 256, "%s%s", fname, ".part");
	FILE* f = fopen(pname, "w");
	if(f == NULL)
	{
		LOGE("fopen %s failed", pname);
		goto fail_part_open;
	}

	// write file.part
	if(fwrite(data, size, 1, f) != 1)
	{
		LOGE("fwrite %s failed", pname);
		goto fail_part_write;
	}

	// move file.part
	if(rename(pname, fname) != 0)
	{
		LOGE("rename %s failed", pname);
		goto fail_part_rename;
	}

	fclose(f);
	free(data);
	net_socket_close(&s);

	LOGI("id=%i: %i/%i, %s", id,
	     gstate->idx, gstate->cnt, fname);

	// success
	return 1;

	// failure
	fail_part_rename:
		unlink(pname);
	fail_part_write:
		fclose(f);
	fail_part_open:
		free(data);
	fail_wget:
		net_socket_close(&s);
	return 0;
}

static void* mq_thread(void* arg)
{
	assert(arg);
	LOGD("debug");

	int x;
	int y;
	int id = *((int*) arg);
	while(mq_state_next(id, &x, &y))
	{
		int i;
		int j;
		for(i = 0; i < NEDGZ_SUBTILE_COUNT; ++i)
		{
			for(j = 0; j < NEDGZ_SUBTILE_COUNT; ++j)
			{
				// try to sync xx, yy
				int xx = NEDGZ_SUBTILE_COUNT*x + j;
				int yy = NEDGZ_SUBTILE_COUNT*y + i;
				while(mq_sync(id, xx, yy) == 0)
				{
					if(mq_state_isQuit(id))
					{
						return NULL;
					}

					// try again
					usleep(100000);
				}

				if(mq_state_isQuit(id))
				{
					return NULL;
				}
			}
		}
	}

	return NULL;
}

int main(int argc, char** argv)
{
	if(argc != 6)
	{
		LOGE("usage: %s [zoom] [latT] [lonL] [latB] [lonR]", argv[0]);
		return EXIT_FAILURE;
	}

	int    zoom = (int) strtol(argv[1], NULL, 0);
	double latT = strtod(argv[2], NULL);
	double lonL = strtod(argv[3], NULL);
	double latB = strtod(argv[4], NULL);
	double lonR = strtod(argv[5], NULL);

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

	gstate = (mq_state_t*) malloc(sizeof(mq_state_t));
	if(gstate == NULL)
	{
		LOGE("malloc failed");
		return EXIT_FAILURE;
	}

	gstate->idx  = 0;
	gstate->x0   = (int) x0f;
	gstate->y0   = (int) y0f;
	gstate->x1   = (int) x1f;
	gstate->y1   = (int) y1f;
	gstate->zoom = zoom;
	gstate->cnt  = (gstate->x1 - gstate->x0 + 1)*
	               (gstate->y1 - gstate->y0 + 1);
	gstate->x    = gstate->x0;
	gstate->y    = gstate->y0;
	gstate->quit = 0;

	// PTHREAD_MUTEX_DEFAULT is not re-entrant
	if(pthread_mutex_init(&gstate->mutex, NULL) != 0)
	{
		LOGE("pthread_mutex_init failed");
		goto fail_mutex;
	}

	// init thread ids
	int i;
	int ids[MQ_STATE_THREADS];
	for(i = 0; i < MQ_STATE_THREADS; ++i)
	{
		ids[i] = i;
	}

	// start threads
	pthread_mutex_lock(&gstate->mutex);
	for(i = 0; i < MQ_STATE_THREADS; ++i)
	{
		if(pthread_create(&gstate->thread[i], NULL, mq_thread, (void*) &ids[i]) != 0)
		{
			LOGE("pthread_create failed");
			goto fail_thread;
		}
	}

	// install signal handler
	if(signal(SIGINT, mq_state_quit) == SIG_ERR)
	{
		goto fail_signal;
	}
	pthread_mutex_unlock(&gstate->mutex);

	// cleanup
	for(i = 0; i < MQ_STATE_THREADS; ++i)
	{
		pthread_join(gstate->thread[i], NULL);
	}
	signal(SIGINT, SIG_DFL);
	pthread_mutex_destroy(&gstate->mutex);
	free(gstate);
	gstate = NULL;

	// success
	return EXIT_SUCCESS;

	// failure
	fail_signal:
	fail_thread:
		gstate->quit = 1;
		pthread_mutex_unlock(&gstate->mutex);

		int j;
		for(j = 0; j < i; ++j)
		{
			pthread_join(gstate->thread[j], NULL);
		}

		pthread_mutex_destroy(&gstate->mutex);
	fail_mutex:
		free(gstate);
		gstate = NULL;
	return EXIT_FAILURE;
}
