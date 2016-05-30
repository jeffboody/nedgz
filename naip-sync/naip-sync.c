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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "a3d/a3d_list.h"
#include "libexpat/expat/lib/expat.h"

#define LOG_TAG "naip-sync"
#include "a3d/a3d_log.h"

#define NAIP_THREAD_COUNT 8

// global state
int             gcancel = 0;
int             gcount  = 0;
a3d_list_t*     glist   = NULL;
a3d_listitem_t* giter   = NULL;
pthread_t       gthread[NAIP_THREAD_COUNT];
pthread_mutex_t gmutex;

typedef struct
{
	char fname[32];
	char url[256];
} naip_node_t;

static naip_node_t* naip_node_new(const char* id, const char* url)
{
	assert(id);
	assert(url);

	naip_node_t* self = (naip_node_t*) malloc(sizeof(naip_node_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	snprintf(self->fname, 32, "%s.jp2", id);
	self->fname[31] = '\0';

	strncpy(self->url, url, 256);
	self->url[255] = '\0';

	return self;
}

static void naip_node_delete(naip_node_t** _self)
{
	assert(_self);

	naip_node_t* self = *_self;
	if(self)
	{
		free(self);
		*_self = NULL;
	}
}

static void naip_parseStart(void* _null,
                            const XML_Char* name,
                            const XML_Char** atts)
{
	assert(name);
	assert(atts);

	// validate name
	if(strcmp(name, "naip") == 0)
	{
		// ignore
		return;
	}
	else if(strcmp(name, "node") != 0)
	{
		LOGW("invalid name=%s", name);
		return;
	}

	// validate atts
	if((atts[ 0] == NULL) ||
	   (atts[ 1] == NULL) ||
	   (atts[ 2] == NULL) ||
	   (atts[ 3] == NULL) ||
	   (atts[ 4] == NULL) ||
	   (atts[ 5] == NULL) ||
	   (atts[ 6] == NULL) ||
	   (atts[ 7] == NULL) ||
	   (atts[ 8] == NULL) ||
	   (atts[ 9] == NULL) ||
	   (atts[10] == NULL) ||
	   (atts[11] == NULL) ||
	   (atts[12] != NULL) ||
	   (strcmp(atts[ 0], "id")  != 0) ||
	   (strcmp(atts[ 2], "url") != 0) ||
	   (strcmp(atts[ 4], "t")   != 0) ||
	   (strcmp(atts[ 6], "l")   != 0) ||
	   (strcmp(atts[ 8], "b")   != 0) ||
	   (strcmp(atts[10], "r")   != 0))
	{
		int idx = 0;
		while(atts[idx])
		{
			LOGW("invalid atts=%s", atts[idx]);
			++idx;
		}
		return;
	}

	naip_node_t* node = naip_node_new(atts[1], atts[3]);
	if(node == NULL)
	{
		return;
	}

	if(a3d_list_enqueue(glist, (const void*) node) == 0)
	{
		naip_node_delete(&node);
	}
}

static void naip_parseEnd(void* _null,
                          const XML_Char* name)
{
	assert(name);

	// ignore
}

static void naip_deleteList(void)
{
	a3d_listitem_t* iter = a3d_list_head(glist);
	while(iter)
	{
		naip_node_t* node = (naip_node_t*)
		                    a3d_list_remove(glist, &iter);
		naip_node_delete(&node);
	}
	a3d_list_delete(&glist);
}

static int naip_readXml(const char* fname)
{
	assert(fname);

	LOGI("%s", fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("invalid %s", fname);
		return 0;
	}

	XML_Parser xml = XML_ParserCreate("US-ASCII");
	if(xml == NULL)
	{
		LOGE("XML_ParserCreate failed");
		goto fail_create;
	}
	XML_SetElementHandler(xml,
	                      naip_parseStart,
	                      naip_parseEnd);

	int done = 0;
	while(done == 0)
	{
		void* buf = XML_GetBuffer(xml, 4096);
		if(buf == NULL)
		{
			LOGE("XML_GetBuffer buf=NULL");
			goto fail_parse;
		}

		int bytes = fread(buf, 1, 4096, f);
		if(bytes < 0)
		{
			LOGE("read failed");
			goto fail_parse;
		}

		done = (bytes == 0) ? 1 : 0;
		if(XML_ParseBuffer(xml, bytes, done) == 0)
		{
			// make sure str is null terminated
			char* str = (char*) buf;
			str[(bytes > 0) ? (bytes - 1) : 0] = '\0';

			enum XML_Error e = XML_GetErrorCode(xml);
			LOGE("XML_ParseBuffer err=%s, bytes=%i, buf=%s",
			     XML_ErrorString(e), bytes, str);
			goto fail_parse;
		}
	}
	XML_ParserFree(xml);
	fclose(f);

	// success
	return 1;

	// failure
	fail_parse:
		XML_ParserFree(xml);
	fail_create:
		fclose(f);
	return 0;
}

static int naip_nextNode(naip_node_t** _node, int* _count)
{
	assert(_node);

	pthread_mutex_lock(&gmutex);

	if(gcancel || (giter == NULL))
	{
		pthread_mutex_unlock(&gmutex);
		return 0;
	}

	*_node = (naip_node_t*) a3d_list_peekitem(giter);
	giter  = a3d_list_next(giter);
	++gcount;
	*_count = gcount;

	pthread_mutex_unlock(&gmutex);
	return 1;
}

static void naip_signal(int signo)
{
	if(signo != SIGINT)
	{
		return;
	}

	pthread_mutex_lock(&gmutex);
	LOGI("[QUIT]");
	gcancel = 1;
	pthread_mutex_unlock(&gmutex);
}

static void* naip_thread(void* arg)
{
	int id = *((int*) arg);

	int count = 0;
	naip_node_t* node = NULL;
	while(naip_nextNode(&node, &count))
	{
		// check if file already exists
		if(access(node->fname, F_OK) == 0)
		{
			LOGI("[SKIP] id=%i, count=%i, fname=%s",
			     id, count, node->fname);
			continue;
		}

		char cmd[256];
		snprintf(cmd, 256, "wget %s -O %s.part", node->url, node->fname);
		cmd[255] = '\0';

		char pname[256];
		snprintf(pname, 256, "%s.part", node->fname);
		pname[255] = '\0';

		LOGI("[SYNC] id=%i, count=%i, fname=%s",
		     id, count, node->fname);

		while((system(cmd)                != 0) ||
		      (rename(pname, node->fname) != 0))
		{
			LOGI("[FAIL] id=%i, count=%i, fname=%s",
			     id, count, node->fname);

			// remove part file
			if(access(pname, F_OK) == 0)
			{
				unlink(pname);
			}

			pthread_mutex_lock(&gmutex);
			if(gcancel)
			{
				pthread_mutex_unlock(&gmutex);
				return NULL;
			}
			pthread_mutex_unlock(&gmutex);

			usleep(1000000);
			LOGI("[SYNC] id=%i, count=%i, fname=%s",
			     id, count, node->fname);
		}
	}

	LOGI("[DONE] id=%i", id);
	return NULL;
}

int main(int argc, const char** argv)
{
	// To track progress:
	// naip-sync naip.xml | tee log.txt
	// tail -f log.txt
	if(argc != 2)
	{
		LOGE("usage: %s <naip.xml>", argv[0]);
		return EXIT_FAILURE;
	}

	glist = a3d_list_new();
	if(glist == NULL)
	{
		return EXIT_FAILURE;
	}

	if(naip_readXml(argv[1]) == 0)
	{
		goto fail_xml;
	}
	giter = a3d_list_head(glist);

	// PTHREAD_MUTEX_DEFAULT is not re-entrant
	if(pthread_mutex_init(&gmutex, NULL) != 0)
	{
		LOGE("pthread_mutex_init failed");
		goto fail_mutex;
	}

	// create threads
	int i;
	int ids[NAIP_THREAD_COUNT];
	pthread_mutex_lock(&gmutex);
	for(i = 0; i < NAIP_THREAD_COUNT; ++i)
	{
		ids[i] = i;
		if(pthread_create(&gthread[i], NULL,
		                  naip_thread,
		                  (void*) &ids[i]) != 0)
		{
			LOGE("pthread_create failed");
			gcancel = 1;
			pthread_mutex_unlock(&gmutex);
			goto fail_thread;
		}
	}

	// install signal handler
	if(signal(SIGINT, naip_signal) == SIG_ERR)
	{
		LOGE("signal failed");
		gcancel = 1;
		pthread_mutex_unlock(&gmutex);
		goto fail_signal;
	}
	pthread_mutex_unlock(&gmutex);

	for(i = 0; i < NAIP_THREAD_COUNT; ++i)
	{
		pthread_join(gthread[i], NULL);
	}
	pthread_mutex_destroy(&gmutex);
	naip_deleteList();

	// success
	return EXIT_SUCCESS;

	// failure
	fail_signal:
		for(i = 0; i < NAIP_THREAD_COUNT; ++i)
		{
			pthread_join(gthread[i], NULL);
		}
	fail_thread:
		pthread_mutex_destroy(&gmutex);
	fail_mutex:
	fail_xml:
		naip_deleteList();
	return EXIT_FAILURE;
}
