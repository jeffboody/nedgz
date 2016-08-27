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
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "a3d/a3d_list.h"
#include "texgz/texgz_tex.h"
#include "texgz/texgz_jpeg.h"
#include "texgz/texgz_jp2.h"
#include "terrain/terrain_tile.h"
#include "terrain/terrain_util.h"
#include "libexpat/expat/lib/expat.h"

#define LOG_TAG "naip-sample"
#include "a3d/a3d_log.h"

typedef struct naip_node_s
{
	char fname[256];
	double t;
	double l;
	double b;
	double r;

	// LRU cache
	texgz_tex_t*        tex;
	struct naip_node_s* next;
	struct naip_node_s* prev;
} naip_node_t;

#define NAIP_CACHE_SIZE 16

// global state
a3d_list_t*  glist = NULL;
naip_node_t* glru  = NULL;

naip_node_t* naip_cache(naip_node_t* node)
{
	assert(node);

	LOGI("import %s", node->fname);
	node->tex = texgz_jp2_import(node->fname);
	if(node->tex == NULL)
	{
		return NULL;
	}

	// force the jp2 to RGBA
	if(texgz_tex_convert(node->tex,
	                     TEXGZ_UNSIGNED_BYTE,
	                     TEXGZ_RGBA) == 0)
	{
		goto fail_convert;
	}

	// insert node into cache
	node->prev = NULL;
	node->next = glru;

	if(glru)
	{
		glru->prev = node;
	}
	glru = node;

	// evict excess nodes
	int count = 0;
	while(node)
	{
		++count;

		if(count > NAIP_CACHE_SIZE)
		{
			naip_node_t* prev = node->prev;
			naip_node_t* next = node->next;
			if(prev)
			{
				prev->next = next;
			}

			if(next)
			{
				next->prev = prev;
			}

			texgz_tex_delete(&node->tex);
			node->prev = NULL;
			node->next = NULL;
			node = next;
		}
		else
		{
			node = node->next;
		}
	}

	// success
	return glru;

	// failure
	fail_convert:
		texgz_tex_delete(&node->tex);
	return NULL;
}

naip_node_t* naip_find(double lat, double lon)
{
	// search the LRU
	naip_node_t* iter = glru;
	while(iter)
	{
		// if found update LRU
		if((iter->t >= lat) &&
		   (iter->b <= lat) &&
		   (iter->l <= lon) &&
		   (iter->r >= lon))
		{
			if(iter == glru)
			{
				return iter;
			}

			naip_node_t* prev = iter->prev;
			naip_node_t* next = iter->next;
			if(prev)
			{
				prev->next = next;
			}

			if(next)
			{
				next->prev = prev;
			}

			iter->prev = NULL;
			iter->next = glru;
			glru->prev = iter;
			glru       = iter;

			return iter;
		}

		iter = iter->next;
	}

	// search the list
	a3d_listitem_t* item = a3d_list_head(glist);
	while(item)
	{
		// if found insert in LRU
		iter = (naip_node_t*) a3d_list_peekitem(item);
		if((iter->t >= lat) &&
		   (iter->b <= lat) &&
		   (iter->l <= lon) &&
		   (iter->r >= lon))
		{
			return naip_cache(iter);
		}

		item = a3d_list_next(item);
	}

	return NULL;
}

int naip_mkdir(const char* fname)
{
	assert(fname);

	int  len = strnlen(fname, 255);
	char dir[256];
	int  i;
	for(i = 0; i < len; ++i)
	{
		dir[i]     = fname[i];
		dir[i + 1] = '\0';

		if(dir[i] == '/')
		{
			if(access(dir, R_OK) == 0)
			{
				// dir already exists
				continue;
			}

			// try to mkdir
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
	}

	return 1;
}

static naip_node_t* naip_node_new(const char* id,
                                  const char* t,
                                  const char* l,
                                  const char* b,
                                  const char* r)
{
	assert(id);
	assert(t);
	assert(l);
	assert(b);
	assert(r);

	naip_node_t* self = (naip_node_t*) malloc(sizeof(naip_node_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	snprintf(self->fname, 256, "naip/%s.jp2", id);
	self->fname[255] = '\0';

	self->t    = strtod(t, NULL);
	self->l    = strtod(l, NULL);
	self->b    = strtod(b, NULL);
	self->r    = strtod(r, NULL);
	self->tex  = NULL;
	self->next = NULL;
	self->prev = NULL;

	return self;
}

static void naip_node_delete(naip_node_t** _self)
{
	assert(_self);

	naip_node_t* self = *_self;
	if(self)
	{
		texgz_tex_delete(&self->tex);
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

	naip_node_t* node = naip_node_new(atts[1], atts[5],
                                      atts[7], atts[9],
	                                  atts[11]);
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

static float stochastic(void)
{
	// compute a random number [-1.0f to 1.0f]
	return 0.01*((float) ((rand() % 201) - 100));
}

static void naip_sampleEnd(texgz_tex_t* tex,
                           int zoom, int x, int y)
{
	assert(tex);

	// tex and node->tex are expected to have RGBA-8888

	int i;
	int j;
	double lat;
	double lon;
	float  xf  = (float) x;
	float  yf  = (float) y;
	int    bpp = texgz_tex_bpp(tex);
	unsigned char pixel00[4];
	unsigned char pixel01[4];
	unsigned char pixel10[4];
	unsigned char pixel11[4];
	float         pixelf[4];
	for(i = 0; i < tex->height; ++i)
	{
		int offset = bpp*i*tex->stride;
		for(j = 0; j < tex->width; ++j)
		{
			// compute pixel center
			float ii = (float) i;
			float jj = (float) j;
			float xx = xf + jj/255.0f;
			float yy = yf + ii/255.0f;
			terrain_tile2coord(xx, yy, zoom,
			                   &lat, &lon);

			// find/cache the naip image
			naip_node_t* node = naip_find(lat, lon);
			if(node == NULL)
			{
				offset += bpp;
				continue;
			}

			// stochastic/random sampling pattern
			float w    = (float) node->tex->width;
			float h    = (float) node->tex->height;
			float ssu0 = (0.5f + 0.25*stochastic())/w;
			float ssu1 = (0.5f + 0.25*stochastic())/w;
			float ssu2 = (0.5f + 0.25*stochastic())/w;
			float ssu3 = (0.5f + 0.25*stochastic())/w;
			float ssv0 = (0.5f + 0.25*stochastic())/h;
			float ssv1 = (0.5f + 0.25*stochastic())/h;
			float ssv2 = (0.5f + 0.25*stochastic())/h;
			float ssv3 = (0.5f + 0.25*stochastic())/h;

			// sample the pixel (MSAA)
			double u0 = fabs(lon - node->l);
			double v0 = fabs(lat - node->t);
			double du = fabs(node->l - node->r);
			double dv = fabs(node->t - node->b);
			float  u  = (float) (u0/du);
			float  v  = (float) (v0/dv);
			texgz_tex_sample(node->tex, u - ssu0, v - ssv0,
			                 bpp, pixel00);
			texgz_tex_sample(node->tex, u + ssu1, v - ssv1,
			                 bpp, pixel01);
			texgz_tex_sample(node->tex, u - ssu2, v + ssv2,
			                 bpp, pixel10);
			texgz_tex_sample(node->tex, u + ssu3, v + ssv3,
			                 bpp, pixel11);

			// compute pixel average
			pixelf[0] = ((float) pixel00[0] + (float) pixel01[0] +
			             (float) pixel10[0] + (float) pixel11[0])/4.0f;
			pixelf[1] = ((float) pixel00[1] + (float) pixel01[1] +
			             (float) pixel10[1] + (float) pixel11[1])/4.0f;
			pixelf[2] = ((float) pixel00[2] + (float) pixel01[2] +
			             (float) pixel10[2] + (float) pixel11[2])/4.0f;
			pixelf[3] = ((float) pixel00[3] + (float) pixel01[3] +
			             (float) pixel10[3] + (float) pixel11[3])/4.0f;

			// set the pixel
			tex->pixels[offset + 0] = (unsigned char) pixelf[0];
			tex->pixels[offset + 1] = (unsigned char) pixelf[1];
			tex->pixels[offset + 2] = (unsigned char) pixelf[2];
			tex->pixels[offset + 3] = (unsigned char) pixelf[3];
			offset += bpp;
		}
	}
}

static texgz_tex_t* naip_sampleNode(int zoom, int x, int y)
{
	// restart the saved node if it exists
	char sname[256];
	if(zoom == 12)
	{
		snprintf(sname, 256, "naipsave/%i/%i/%i.texz", zoom, x, y);
		sname[255] = '\0';

		if(access(sname, F_OK) == 0)
		{
			// assume this LOD already completed
			texgz_tex_t* down = texgz_tex_importz(sname);
			if(down)
			{
				LOGI("restart %s", sname);
				return down;
			}
		}
	}

	// terrain tiles only exist up to zoom 15
	// and zoom > 15 is covered by zoom 15
	short min;
	short max;
	int   flags = TERRAIN_NEXT_ALL;
	if(zoom < 15)
	{
		if(terrain_tile_header(".", x, y, zoom,
		                       &min, &max, &flags) == 0)
		{
			return NULL;
		}
	}

	// create a working texture
	texgz_tex_t* tex = texgz_tex_new(256, 256, 256, 256,
	                                 TEXGZ_UNSIGNED_BYTE,
	                                 TEXGZ_RGBA, NULL);
	if(tex == NULL)
	{
		return NULL;
	}

	// sample the next/end LOD
	// naip data roughly corresponds to zoom 17
	if(zoom == 17)
	{
		naip_sampleEnd(tex, zoom, x, y);
	}
	else
	{
		texgz_tex_t* tmp;
		if(flags & TERRAIN_NEXT_TL)
		{
			tmp = naip_sampleNode(zoom + 1, 2*x, 2*y);
			if(tmp)
			{
				texgz_tex_blit(tmp, tex, 128, 128,
				               0, 0, 0, 0);
				texgz_tex_delete(&tmp);
			}
		}

		if(flags & TERRAIN_NEXT_BL)
		{
			tmp = naip_sampleNode(zoom + 1, 2*x, 2*y + 1);
			if(tmp)
			{
				texgz_tex_blit(tmp, tex, 128, 128,
				               0, 0, 0, 128);
				texgz_tex_delete(&tmp);
			}
		}

		if(flags & TERRAIN_NEXT_TR)
		{
			tmp = naip_sampleNode(zoom + 1, 2*x + 1, 2*y);
			if(tmp)
			{
				texgz_tex_blit(tmp, tex, 128, 128,
				               0, 0, 128, 0);
				texgz_tex_delete(&tmp);
			}
		}

		if(flags & TERRAIN_NEXT_BR)
		{
			tmp = naip_sampleNode(zoom + 1, 2*x + 1, 2*y + 1);
			if(tmp)
			{
				texgz_tex_blit(tmp, tex, 128, 128,
				               0, 0, 128, 128);
				texgz_tex_delete(&tmp);
			}
		}
	}

	// export the JPEG texture
	char fname[256];
	snprintf(fname, 256, "naipjpg/%i/%i/%i.jpg", zoom, x, y);
	fname[255] = '\0';
	{
		char pname[256];
		snprintf(pname, 256, "naipjpg/%i/%i/%i.jpg.part", zoom, x, y);
		pname[255] = '\0';

		if(naip_mkdir(fname) == 0)
		{
			goto fail_export;
		}

		if(texgz_jpeg_export(tex, pname) == 0)
		{
			goto fail_export;
		}

		if(rename(pname, fname) != 0)
		{
			LOGE("rename failed %s", fname);
			unlink(pname);
			goto fail_export;
		}

		LOGI("export %s", fname);
	}

	// compute the downscaled texture for the next LOD
	texgz_tex_t* down = texgz_tex_downscale(tex);
	if(down == NULL)
	{
		goto fail_down;
	}

	// save the output for restart
	if(zoom == 12)
	{
		char pname[256];
		snprintf(pname, 256, "naipsave/%i/%i/%i.texz.part", zoom, x, y);
		pname[255] = '\0';

		if(naip_mkdir(sname) == 0)
		{
			goto fail_save;
		}

		if(texgz_tex_exportz(down, pname) == 0)
		{
			goto fail_save;
		}

		if(rename(pname, sname) != 0)
		{
			LOGE("rename failed %s", sname);
			unlink(pname);
			goto fail_save;
		}

		LOGI("export %s", sname);
	}

	texgz_tex_delete(&tex);

	// success
	return down;

	// failure
	fail_save:
		texgz_tex_delete(&down);
	fail_down:
		unlink(fname);
	fail_export:
		texgz_tex_delete(&tex);
	return NULL;
}

static void naip_sampleStart(void)
{
	texgz_tex_t* tex = naip_sampleNode(0, 0, 0);
	texgz_tex_delete(&tex);
}

int main(int argc, const char** argv)
{
	// To track progress:
	// naip-sample naip.xml | tee log.txt
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

	naip_sampleStart();

	naip_deleteList();

	// success
	return EXIT_SUCCESS;

	// failure
	fail_xml:
		naip_deleteList();
	return EXIT_FAILURE;
}
