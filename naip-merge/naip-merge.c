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
#include "a3d/a3d_list.h"

#define LOG_TAG "naip-merge"
#include "a3d/a3d_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

typedef struct
{
	char   id[32];
	char   url[256];
	double t;
	double l;
	double b;
	double r;
} naip_item_t;

static naip_item_t* naip_item_new(const char* str_id,
                                  const char* str_url,
                                  const char* t, const char* l,
                                  const char* b, const char* r,
                                  int row, int col)
{
	assert(str_id);
	assert(str_url);
	assert(t);
	assert(l);
	assert(b);
	assert(r);

	naip_item_t* self = (naip_item_t*) malloc(sizeof(naip_item_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	strncpy(self->id, str_id, 32);
	strncpy(self->url, str_url, 256);
	self->id[31]   = '\0';
	self->url[255] = '\0';

	self->t = strtod(t, NULL);
	self->l = strtod(l, NULL);
	self->b = strtod(b, NULL);
	self->r = strtod(r, NULL);

	double bb_t = (double) row;
	double bb_l = (double) -col;
	double bb_b = bb_t - 1.0;
	double bb_r = bb_l + 1.0;

	// check that the item belongs to the file
	if((self->t < bb_b) || (self->b > bb_t) ||
	   (self->l > bb_r) || (self->r < bb_l))
	{
		LOGE("invalid row=%i, col=%i, t=%lf, l=%lf, b=%lf, r=%lf",
		     row, col, self->t, self->l, self->b, self->r);
		goto fail_bb;
	}

	// success
	return self;

	// failure
	fail_bb:
		free(self);
	return NULL;
}

static void naip_item_delete(naip_item_t** _self)
{
	assert(_self);

	naip_item_t* self = *_self;
	if(self)
	{
		free(self);
		*_self = NULL;
	}
}

typedef struct
{
	int init;
	int idx_id;
	int idx_bb;
	int idx_url;

	char table[181*181];

	a3d_list_t* list;
} naip_parser_t;

static void naip_trim(char* line)
{
	assert(line);

	int idx = 0;
	while(line[idx] != '\0')
	{
		if(line[idx] == '\r')
		{
			line[idx] = '\0';
		}
		else if(line[idx] == '\n')
		{
			line[idx] = '\0';
		}

		++idx;
	}
}

static int naip_cmp(const void* a, const void* b)
{
	assert(a);
	assert(b);

	naip_item_t* item = (naip_item_t*) a;
	char*        id   = (char*) b;

	return strcmp(item->id, id);
}

static void naip_parser_reset(naip_parser_t* self)
{
	assert(self);

	self->init    = 0;
	self->idx_id  = -1;
	self->idx_bb  = -1;
	self->idx_url = -1;
}

static naip_parser_t* naip_parser_new(void)
{
	naip_parser_t* self = (naip_parser_t*)
	                      malloc(sizeof(naip_parser_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	memset(self->table, 0, 181*181);

	self->list = a3d_list_new();
	if(self->list == NULL)
	{
		goto fail_list;
	}

	naip_parser_reset(self);

	// success
	return self;

	// failure
	fail_list:
		free(self);
	return NULL;
}

static void naip_parser_delete(naip_parser_t** _self)
{
	assert(_self);

	naip_parser_t* self = *_self;
	if(self)
	{
		FILE* f = fopen("naip.xml", "w");
		if(f == NULL)
		{
			LOGE("invalid naip.xml");
		}
		else
		{
			fprintf(f, "%s", "<?xml version='1.0' encoding='US-ASCII'?>\n");
			fprintf(f, "<naip>\n");
		}

		int idx = 0;
		a3d_listitem_t* item = a3d_list_head(self->list);
		while(item)
		{
			naip_item_t* node = (naip_item_t*)
			                    a3d_list_remove(self->list,
			                                    &item);
			LOGI("%i: id=%s, url=%s, t=%lf, l=%lf, b=%lf, r=%lf",
			     idx, node->id, node->url, node->t, node->l, node->b, node->r);
			if(f)
			{
				fprintf(f, "\t<node id=\"%s\" url=\"%s\" t=\"%lf\" l=\"%lf\" b=\"%lf\" r=\"%lf\" />\n",
				        node->id, node->url, node->t, node->l, node->b, node->r);
			}
			naip_item_delete(&node);
			++idx;
		}

		if(f)
		{
			fprintf(f, "</naip>\n");
			fclose(f);
		}

		/*
		 * print table
		 */
		int row;
		int col;
		LOGI("TABLE");
		for(row = 180; row >= 0; --row)
		{
			for(col = 180; col >= 0; --col)
			{
				char x = self->table[row*181 + col];
				printf("%s", x ? "*" : " ");
			}
			printf("\n");
		}

		a3d_list_delete(&self->list);
		free(self);
		*_self = NULL;
	}
}

static char* naip_parser_readcell(naip_parser_t* self, char* line)
{
	assert(self);
	assert(line);

	char* cell = line;
	if(line[0] == '"')
	{
		// find the end quote
		cell = strchr(&(line[1]), '"');
		if(cell == NULL)
		{
			LOGE("bad line=%s", line);
			return NULL;
		}
	}

	// find the next cell
	char* p = strchr(cell, ',');
	if(p == NULL)
	{
		return NULL;
	}

	// mark end of this cell
	p[0] = '\0';

	// return a pointer to the next cell
	return &p[1];
}

static int naip_parser_readhdr(naip_parser_t* self, char* line)
{
	assert(self);
	assert(line);

	int   idx  = 0;
	char* next = NULL;
	while(line)
	{
		next = naip_parser_readcell(self, line);
		if(strcmp(line, "sourceId") == 0)
		{
			self->idx_id = idx;
		}
		else if(strcmp(line, "boundingBox") == 0)
		{
			self->idx_bb = idx;
		}
		else if(strcmp(line, "downloadURL") == 0)
		{
			self->idx_url = idx;
		}

		line = next;
		++idx;
	}

	if((self->idx_id  == -1) ||
	   (self->idx_bb  == -1) ||
	   (self->idx_url == -1))
	{
		LOGE("invalid id=%i, bb=%i, url=%i",
		     self->idx_id, self->idx_bb, self->idx_url);
		return 0;
	}

	self->init = 1;
	return 1;
}

static int naip_readbb(char* s, char** t, char** l, char** b, char** r)
{
	assert(s);
	assert(t);
	assert(l);
	assert(b);
	assert(r);

	// example: {minY:38.9375,minX:-106,maxY:39,maxX:-105.9375}
	char* minY = strstr(s, "minY:");
	char* minX = strstr(s, "minX:");
	char* maxY = strstr(s, "maxY:");
	char* maxX = strstr(s, "maxX:");

	if((minY == NULL) || (minX == NULL) ||
	   (maxY == NULL) || (maxX == NULL))
	{
		LOGE("invalid %s", s);
		return 0;
	}

	// mark the end-of-string
	while(s[0] != '\0')
	{
		if(s[0] == ',')
		{
			s[0] = '\0';
		}
		else if(s[0] == '}')
		{
			s[0] = '\0';
		}

		s = &s[1];
	}

	// store the string pointers
	*t = &maxY[5];
	*l = &minX[5];
	*b = &minY[5];
	*r = &maxX[5];

	return 1;
}

static int naip_parser_readbody(naip_parser_t* self, char* line,
                                int row, int col)
{
	assert(self);
	assert(line);

	char* str_id  = NULL;
	char* str_bb  = NULL;
	char* str_url = NULL;

	int   idx  = 0;
	char* next = NULL;
	while(line)
	{
		next = naip_parser_readcell(self, line);
		if(idx == self->idx_id)
		{
			str_id = line;
		}
		else if(idx == self->idx_bb)
		{
			str_bb = line;
		}
		else if(idx == self->idx_url)
		{
			str_url = line;
		}

		line = next;
		++idx;
	}


	if(str_id && str_bb && str_url)
	{
		char* t = NULL;
		char* l = NULL;
		char* b = NULL;
		char* r = NULL;
		if(naip_readbb(str_bb, &t, &l, &b, &r) == 0)
		{
			return 0;
		}

		a3d_listitem_t* item = a3d_list_find(self->list,
		                                     (const void*) str_id,
		                                     naip_cmp);
		if(item)
		{
			// aready exists
			return 1;
		}

		// add the node to the list
		naip_item_t* node = naip_item_new(str_id, str_url,
		                                  t, l, b, r,
		                                  row, col);
		if(node == NULL)
		{
			return 0;
		}

		if(a3d_list_push(self->list, (const void*) node) == 0)
		{
			return 0;
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

static int naip_parser_readln(naip_parser_t* self, char* line,
                              int row, int col)
{
	assert(self);
	assert(line);

	if(strlen(line) == 0)
	{
		// ignore empty lines
		return 1;
	}

	if(self->init == 0)
	{
		return naip_parser_readhdr(self, line);
	}
	else
	{
		return naip_parser_readbody(self, line, row, col);
	}
}

static int naip_parser_readfile(naip_parser_t* self, const char* fname)
{
	assert(self);
	assert(fname);

	/*
	 * parse fname
	 */

	char copy[1024];
	strncpy(copy, fname, 1024);
	copy[1023] = '\0';

	char* p1 = strchr(copy, '_');
	if(p1 == NULL)
	{
		LOGE("invalid fname=%s", fname);
		return 0;
	}
	p1[0] = '\0';
	++p1;

	char* p2 = strchr(p1, '_');
	if(p2 == NULL)
	{
		LOGE("invalid fname=%s", fname);
		return 0;
	}
	p2[0] = '\0';
	++p2;

	char* p3 = strchr(p2, '.');
	if(p3 == NULL)
	{
		LOGE("invalid fname=%s", fname);
		return 0;
	}
	p3[0] = '\0';

	/*
	 * update table
	 */

	int col = (int) strtol(p2, NULL, 0);
	int row = (int) strtol(p1, NULL, 0);
	if((row >= 0) && (row <= 180) &&
	   (col >= 0) && (col <= 180))
	{
		if(self->table[row*181 + col])
		{
			LOGE("duplicate fname=%s", fname);
			return 0;
		}
		else
		{
			self->table[row*181 + col] = 1;
		}
	}
	else
	{
		LOGE("invalid fname=%s", fname);
		return 0;
	}

	/*
	 * parse file
	 */

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return 0;
	}

	// reset the state
	naip_parser_reset(self);

	char*  line = NULL;
	size_t n    = 0;
	int    ret  = 1;
	while(getline(&line, &n, f) > 0)
	{
		naip_trim(line);

		strncpy(copy, line, 1024);
		copy[1023] = '\0';

		if(naip_parser_readln(self, copy, row, col) == 0)
		{
			LOGE("invalid fname=%s, line=%s", fname, line);
			ret = 0;
			break;
		}
	}
	free(line);
	fclose(f);

	return ret;
}

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, const char** argv)
{
	if(argc != 2)
	{
		LOGE("usage: %s <csv.list>", argv[0]);
		return EXIT_FAILURE;
	}

	naip_parser_t* parser = naip_parser_new();
	if(parser == NULL)
	{
		return EXIT_FAILURE;
	}

	FILE* f = fopen(argv[1], "r");
	if(f == NULL)
	{
		LOGE("fopen failed for %s", argv[1]);
		return 0;
	}

	char*  line = NULL;
	size_t n    = 0;
	int    ret  = EXIT_SUCCESS;
	int    idx  = 0;
	while(getline(&line, &n, f) > 0)
	{
		naip_trim(line);

		LOGI("%i: %s", idx, line);
		if(naip_parser_readfile(parser, line) == 0)
		{
			ret = EXIT_FAILURE;
			break;
		}
		++idx;
	}
	free(line);
	fclose(f);
	naip_parser_delete(&parser);
	return ret;
}
