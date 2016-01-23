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

#define LOG_TAG "cloud-sync"
#include "nedgz/nedgz_log.h"

#define CLOUD_THREADS 32
typedef struct
{
	FILE*           fsync;
	FILE*           frestart;
	int             synced;
	int             count;
	int             quit;
	pthread_mutex_t mutex;
	pthread_t       thread[CLOUD_THREADS];
} cloud_state_t;

cloud_state_t* gstate = NULL;

static void cloud_state_quit(int signo)
{
	assert(gstate);

	if(signo != SIGINT)
	{
		return;
	}

	pthread_mutex_lock(&gstate->mutex);
	LOGI("[ QUIT]");
	gstate->quit = 1;
	pthread_mutex_unlock(&gstate->mutex);
}

static int cloud_state_isQuit(int id, const char* line)
{
	LOGD("debug id=%i", id);

	pthread_mutex_lock(&gstate->mutex);
	int quit = gstate->quit;
	if(quit)
	{
		LOGI("[ QUIT] id=%02i", id);

		// this file was not synced
		fprintf(gstate->frestart, "%s", line);
	}
	pthread_mutex_unlock(&gstate->mutex);
	return quit;
}

static int cloud_state_next(int id, char** line, size_t* n)
{
	assert(line);
	assert(n);
	LOGD("debug id=%i", id);

	pthread_mutex_lock(&gstate->mutex);

	// quit thread
	if(gstate->quit)
	{
		LOGI("[ QUIT] id=%02i", id);
		pthread_mutex_unlock(&gstate->mutex);
		return 0;
	}

	// get next line
	if(getline(line, n, gstate->fsync) < 0)
	{
		LOGI("[  EOF] id=%02i", id);
		pthread_mutex_unlock(&gstate->mutex);
		return 0;
	}

	// ignore carrage return and line feed
	int   i = 0;
	char* s = *line;
	while(s[i] != '\0')
	{
		if((s[i] == '\r') ||
		   (s[i] == '\n'))
		{
			s[i] = '\0';
		}
		++i;
	}

	pthread_mutex_unlock(&gstate->mutex);
	return 1;
}

static int cloud_sync(int id, const char* line)
{
	assert(line);

	LOGI("[START] id=%02i: %s", id, line);

	// gsutil occasionally hangs
	char timeout[256];
	snprintf(timeout, 256, "%s", "timeout -s KILL 60");

	char cmd[256];
	snprintf(cmd, 256,
	         "%s gsutil cp -a public-read %s gs://goto/%s",
	         timeout, line, line);
	if(system(cmd) != 0)
	{
		return 0;
	}

	pthread_mutex_lock(&gstate->mutex);
	++gstate->synced;
	LOGI("[ SYNC] id=%02i: %i %i %f %s",
	     id, gstate->synced, gstate->count,
	     100.0f*((float) gstate->synced)/((float) gstate->count),
	     line);
	pthread_mutex_unlock(&gstate->mutex);

	return 1;
}

static void* cloud_thread(void* arg)
{
	assert(arg);
	LOGD("debug");

	int    id   = *((int*) arg);
	char*  line = NULL;
	size_t n    = 0;
	while(cloud_state_next(id, &line, &n))
	{
		while(cloud_sync(id, line) == 0)
		{
			if(cloud_state_isQuit(id, line))
			{
				free(line);
				return NULL;
			}

			LOGI("[RETRY] id=%02i: %s", id, line);
			usleep(100000);
		}
	}
	free(line);
	return NULL;
}

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		LOGE("usage: %s [sync.list] [restart.list]", argv[0]);
		return EXIT_FAILURE;
	}

	const char* fname_sync    = argv[1];
	const char* fname_restart = argv[2];

	// warn if restart list already exists
	if(access(fname_restart, F_OK) == 0)
	{
		LOGE("%s already exists", fname_restart);
		return EXIT_FAILURE;
	}

	gstate = (cloud_state_t*) malloc(sizeof(cloud_state_t));
	if(gstate == NULL)
	{
		LOGE("malloc failed");
		return EXIT_FAILURE;
	}

	gstate->fsync = fopen(fname_sync, "r");
	if(gstate->fsync == NULL)
	{
		LOGE("failed to open %s", fname_sync);
		goto fail_fsync;
	}

	char*  line = NULL;
	size_t n    = 0;
	gstate->count  = 0;
	gstate->synced = 0;
	while(getline(&line, &n, gstate->fsync) > 0)
	{
		++gstate->count;
	}
	rewind(gstate->fsync);
	free(line);
	line = NULL;
	n    = 0;

	gstate->frestart = fopen(fname_restart, "w");
	if(gstate->frestart == NULL)
	{
		LOGE("failed to open %s", fname_restart);
		goto fail_frestart;
	}

	gstate->quit = 0;

	// PTHREAD_MUTEX_DEFAULT is not re-entrant
	if(pthread_mutex_init(&gstate->mutex, NULL) != 0)
	{
		LOGE("pthread_mutex_init failed");
		goto fail_mutex;
	}

	// init thread ids
	int i;
	int ids[CLOUD_THREADS];
	for(i = 0; i < CLOUD_THREADS; ++i)
	{
		ids[i] = i;
	}

	// start threads
	pthread_mutex_lock(&gstate->mutex);
	for(i = 0; i < CLOUD_THREADS; ++i)
	{
		if(pthread_create(&gstate->thread[i], NULL,
		                  cloud_thread,
		                  (void*) &ids[i]) != 0)
		{
			LOGE("pthread_create failed");
			goto fail_thread;
		}
	}

	// install signal handler
	if(signal(SIGINT, cloud_state_quit) == SIG_ERR)
	{
		goto fail_signal;
	}
	pthread_mutex_unlock(&gstate->mutex);

	// process threads
	for(i = 0; i < CLOUD_THREADS; ++i)
	{
		pthread_join(gstate->thread[i], NULL);
	}

	// write any files that need to be restarted
	while(getline(&line, &n, gstate->fsync) > 0)
	{
		fprintf(gstate->frestart, "%s", line);
	}
	free(line);
	fclose(gstate->frestart);
	fclose(gstate->fsync);

	LOGI("[ DONE]");

	// cleanup
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
		fclose(gstate->frestart);
	fail_frestart:
		fclose(gstate->fsync);
	fail_fsync:
		free(gstate);
		gstate = NULL;
	return EXIT_FAILURE;
}
