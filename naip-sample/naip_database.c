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
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "a3d/a3d_list.h"
#include "terrain/terrain_tile.h"
#include "terrain/terrain_util.h"
#include "texgz/texgz_jpeg.h"
#include "libexpat/expat/lib/expat.h"
#include "naip_database.h"
#include "naip_node.h"
#include "naip_tile.h"
#include "naip_util.h"

#define LOG_TAG "naip"
#include "a3d/a3d_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

static void naip_database_parseStart(void* _self,
                                     const XML_Char* name,
                                     const XML_Char** atts)
{
	assert(_self);
	assert(name);
	assert(atts);

	naip_database_t* self = (naip_database_t*) _self;

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

	naip_node_t* node = naip_node_new(atts[1], atts[3],
	                                  atts[5], atts[7],
	                                  atts[9], atts[11]);
	if(node == NULL)
	{
		return;
	}

	if(a3d_list_enqueue(self->nodes, (const void*) node) == 0)
	{
		naip_node_delete(&node);
	}
}

static void naip_database_parseEnd(void* _self,
                                   const XML_Char* name)
{
	assert(_self);
	assert(name);

	// ignore
}

static int naip_database_readXml(naip_database_t* self,
                                 const char* fname)
{
	assert(fname);

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
	XML_SetUserData(xml, (void*) self);
	XML_SetElementHandler(xml,
	                      naip_database_parseStart,
	                      naip_database_parseEnd);

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

static a3d_listitem_t*
naip_database_tileFind(naip_database_t* self, int x, int y)
{
	assert(self);

	naip_tile_t tile =
	{
		.x    = x,
		.y    = y,
	};

	a3d_listitem_t* item;
	item = a3d_list_find(self->tiles,
	                     (const void*) &tile,
	                     naip_tile_compare);
	if(item == NULL)
	{
		return NULL;
	}

	return item;
}

static int
naip_database_nodeSampleTile(naip_database_t* self,
                             naip_node_t* node,
                             naip_tile_t* tile)
{
	assert(self);
	assert(node);
	assert(tile);

	// tile and node are expected to have RGBA-8888
	// because nodes are RGB + IR

	// compute the tile coordinates
	double tile_lat0;
	double tile_lon0;
	double tile_lat1;
	double tile_lon1;
	terrain_sample2coord(tile->x, tile->y, 17, 0, 0,
	                     &tile_lat0, &tile_lon0);
	terrain_sample2coord(tile->x, tile->y, 17,
	                     TERRAIN_SAMPLES_TILE - 1,
	                     TERRAIN_SAMPLES_TILE - 1,
	                     &tile_lat1, &tile_lon1);

	// node coordinates
	double node_lat0 = node->t;
	double node_lon0 = node->l;
	double node_lat1 = node->b;
	double node_lon1 = node->r;

	// intersect the tile with the node
	double lat0 = tile_lat0;
	double lon0 = tile_lon0;
	double lat1 = tile_lat1;
	double lon1 = tile_lon1;
	if((node_lat0 < tile_lat1) ||
	   (node_lat1 > tile_lat0) ||
	   (node_lon0 > tile_lon1) ||
	   (node_lon1 < tile_lon0))
	{
		// ignore
		return 1;
	}
	if(tile_lat0 > node_lat0)
	{
		lat0 = node_lat0;
	}
	if(tile_lon0 < node_lon0)
	{
		lon0 = node_lon0;
	}
	if(tile_lat1 < node_lat1)
	{
		lat1 = node_lat1;
	}
	if(tile_lon1 > node_lon1)
	{
		lon1 = node_lon1;
	}

	// determine range of intersection between node and tile
	int row0 = (int) (255.0*(lat0 - tile_lat0)/(tile_lat1 - tile_lat0));
	int row1 = (int) (255.0*(lat1 - tile_lat0)/(tile_lat1 - tile_lat0) + 0.5);
	int col0 = (int) (255.0*(lon0 - tile_lon0)/(tile_lon1 - tile_lon0));
	int col1 = (int) (255.0*(lon1 - tile_lon0)/(tile_lon1 - tile_lon0) + 0.5);

	int i;
	int j;
	double lat;
	double lon;
	float  xf  = (float) tile->x;
	float  yf  = (float) tile->y;
	int    bpp = texgz_tex_bpp(tile->tex);
	unsigned char pixel00[4];
	unsigned char pixel01[4];
	unsigned char pixel10[4];
	unsigned char pixel11[4];
	float         pixelf[4];
	for(i = row0; i <= row1; ++i)
	{
		int offset = bpp*i*tile->tex->stride + bpp*col0;
		for(j = col0; j <= col1; ++j)
		{
			// compute pixel center
			float ii = (float) i;
			float jj = (float) j;
			float xx = xf + jj/255.0f;
			float yy = yf + ii/255.0f;
			terrain_tile2coord(xx, yy, 17,
			                   &lat, &lon);

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
			tile->tex->pixels[offset + 0] = (unsigned char) pixelf[0];
			tile->tex->pixels[offset + 1] = (unsigned char) pixelf[1];
			tile->tex->pixels[offset + 2] = (unsigned char) pixelf[2];
			tile->tex->pixels[offset + 3] = (unsigned char) pixelf[3];
			offset += bpp;
		}
	}

	return 1;
}

static int
naip_database_nodeSample(naip_database_t* self,
                         naip_node_t* node)
{
	assert(self);
	assert(node);

	// import the node->tex
	if(naip_node_import(node) == 0)
	{
		return 0;
	}

	int x0;
	int y0;
	int x1;
	int y1;
	naip_node_tilesCovered(node, 17,
	                       &x0, &y0, &x1, &y1);

	int x;
	int y;
	for(y = y0; y <= y1; ++y)
	{
		for(x = x0; x <= x1; ++x)
		{
			// find/create the tile
			naip_tile_t*    tile;
			a3d_listitem_t* item;
			item = naip_database_tileFind(self, x, y);
			if(item)
			{
				tile = (naip_tile_t*) a3d_list_peekitem(item);
				if(naip_tile_cacheRestore(tile) == NULL)
				{
					return 0;
				}
			}
			else
			{
				tile = naip_tile_new(x, y);
				if(tile == NULL)
				{
					return 0;
				}

				if(a3d_list_insert(self->tiles, NULL,
				                   (const void*) tile) == NULL)
				{
					// otherwise assume tile was inserted
					naip_tile_delete(&tile);
					return 0;
				}
			}

			if(naip_database_nodeSampleTile(self, node, tile) == 0)
			{
				return 0;
			}
		}
	}

	return 1;
}

static void naip_database_tileRemove(naip_database_t* self,
                                     int zoom, int x, int y)
{
	assert(self);

	// end recursion
	if(zoom == 17)
	{
		// find the tile
		a3d_listitem_t* item = naip_database_tileFind(self, x, y);
		if(item == NULL)
		{
			// ignore
			return;
		}

		// remove the tile
		naip_tile_t* tile = (naip_tile_t*)
		                    a3d_list_remove(self->tiles, &item);
		naip_tile_delete(&tile);

		return;
	}

	// recursively remove tiles
	// terrain tiles only exist up to zoom 15
	// and zoom > 15 is covered by zoom 15
	short min   = 0;
	short max   = 0;
	int   flags = TERRAIN_NEXT_ALL;
	if(zoom < 15)
	{
		if(terrain_tile_header(".", x, y, zoom,
		                       &min, &max, &flags) == 0)
		{
			return;
		}
	}

	// recursively traverse terrain
	if(flags & TERRAIN_NEXT_TL)
	{
		naip_database_tileRemove(self,
		                         zoom + 1,
		                         2*x, 2*y);
	}

	if(flags & TERRAIN_NEXT_BL)
	{
		naip_database_tileRemove(self,
		                         zoom + 1,
		                         2*x, 2*y + 1);
	}

	if(flags & TERRAIN_NEXT_TR)
	{
		naip_database_tileRemove(self,
		                         zoom + 1,
		                         2*x + 1, 2*y);
	}

	if(flags & TERRAIN_NEXT_BR)
	{
		naip_database_tileRemove(self,
		                         zoom + 1,
		                         2*x + 1, 2*y + 1);
	}
}

static int naip_database_tileTrim(naip_database_t* self)
{
	assert(self);

	// trim tile cache
	int size = 0;
	a3d_listitem_t* iter = a3d_list_head(self->tiles);
	while(iter)
	{
		naip_tile_t* tile = (naip_tile_t*)
		                    a3d_list_peekitem(iter);

		const int GB = 1000*1024*1024;
		if(size > 2*GB)
		{
			if(naip_tile_cacheSave(tile) == 0)
			{
				return 0;
			}
		}
		else
		{
			size += naip_tile_cacheSize(tile);
		}

		iter =  a3d_list_next(iter);
	}

	return 1;
}

static texgz_tex_t*
naip_database_tileSample(naip_database_t* self,
                         int zend, int zoom, int x, int y);

static texgz_tex_t*
naip_database_checkpointRestore(naip_database_t* self,
                                int zoom, int x, int y)
{
	assert(self);

	// restore the tex at various checkpoints so
	// the algorithm can be restarted
	if((zoom == 12) || (zoom == 9) ||
	   (zoom == 6)  || (zoom == 3))
	{
		char fname[256];
		snprintf(fname, 256, "save/%i/%i/%i.texz",
		         zoom, x, y);
		fname[255] = '\0';

		if(naip_exists(fname))
		{
			texgz_tex_t* tex = texgz_tex_importz(fname);
			if(tex)
			{
				LOGI("restore %s", fname);
				return tex;
			}
		}
	}

	return NULL;
}

static int naip_database_checkpointSave(naip_database_t* self,
                                        texgz_tex_t* down,
                                        int zoom, int x, int y)
{
	assert(self);

	// save the tex at various checkpoints so
	// the algorithm can be restarted
	char fname[256];
	char pname[256];
	if((zoom == 12) || (zoom == 9) ||
	   (zoom == 6)  || (zoom == 3))
	{
		snprintf(fname, 256, "save/%i/%i/%i.texz",
		         zoom, x, y);
		fname[255] = '\0';

		snprintf(pname, 256, "%s.part", fname);
		pname[255] = '\0';

		if(naip_mkdir(pname) == 0)
		{
			return 0;
		}

		if(texgz_tex_exportz(down, pname) == 0)
		{
			return 0;
		}

		if(rename(pname, fname) != 0)
		{
			LOGE("rename failed %s", fname);
			goto fail_rename;
		}

		LOGI("save %s", fname);
	}

	// success
	return 1;

	// failure
	fail_rename:
		unlink(pname);
	return 0;
}

static int naip_database_jpgSave(texgz_tex_t* tex,
                                 int zoom, int x, int y)
{
	assert(tex);

	char fname[256];
	snprintf(fname, 256, "naipjpg/%i/%i/%i.jpg", zoom, x, y);
	fname[255] = '\0';

	char pname[256];
	snprintf(pname, 256, "%s.part", fname);
	pname[255] = '\0';

	if(naip_mkdir(pname) == 0)
	{
		return 0;
	}

	if(texgz_jpeg_export(tex, pname) == 0)
	{
		return 0;
	}

	if(rename(pname, fname) != 0)
	{
		LOGE("rename failed %s", fname);
		goto fail_rename;
	}

	LOGI("save %s", fname);

	// success
	return 1;

	// failure
	fail_rename:
		unlink(pname);
	return 0;
}

static void naip_database_jpgDelete(int zoom, int x, int y)
{
	char fname[256];
	snprintf(fname, 256, "naipjpg/%i/%i/%i.jpg", zoom, x, y);
	fname[255] = '\0';

	unlink(fname);
}

static texgz_tex_t*
naip_database_tileSample12(naip_database_t* self,
                           int x, int y)
{
	assert(self);

	// compute the tile coordinates
	double lat0;
	double lon0;
	double lat1;
	double lon1;
	terrain_sample2coord(x, y, 12, 0, 0,
	                     &lat0, &lon0);
	terrain_sample2coord(x, y, 12,
	                     TERRAIN_SAMPLES_TILE - 1,
	                     TERRAIN_SAMPLES_TILE - 1,
	                     &lat1, &lon1);

	// check if a node intersects the tile
	a3d_listitem_t* iter = a3d_list_head(self->nodes);
	while(iter)
	{
		naip_node_t* node = (naip_node_t*)
		                    a3d_list_peekitem(iter);
		if((node->t < lat1) ||
		   (node->b > lat0) ||
		   (node->l > lon1) ||
		   (node->r < lon0))
		{
			iter = a3d_list_next(iter);
			continue;
		}

		// sample the node
		if(naip_database_nodeSample(self, node) == 0)
		{
			return NULL;
		}

		// remove node once processed
		node = (naip_node_t*)
		       a3d_list_remove(self->nodes, &iter);
		naip_node_delete(&node);
	}

	// recursively sample the tiles
	texgz_tex_t* tex;
	tex = naip_database_tileSample(self, 17, 12, x, y);
	if(tex == NULL)
	{
		return NULL;
	}

	// remove unneeded tiles and trim cache
	naip_database_tileRemove(self, 12, x, y);
	if(naip_database_tileTrim(self) == 0)
	{
		goto fail_trim;
	}

	// success
	return tex;

	// failure
	fail_trim:
		texgz_tex_delete(&tex);
	return NULL;
}

static texgz_tex_t*
naip_database_tileSample17(naip_database_t* self,
                           int x, int y)
{
	assert(self);

	// find/create the tile
	naip_tile_t* tile;
	a3d_listitem_t* item = naip_database_tileFind(self, x, y);
	if(item)
	{
		tile = (naip_tile_t*) a3d_list_peekitem(item);
	}
	else
	{
		tile = naip_tile_new(x, y);
		if(tile == NULL)
		{
			return NULL;
		}

		if(a3d_list_insert(self->tiles, NULL,
		                   (const void*) tile) == NULL)
		{
			// otherwise assume tile was inserted
			naip_tile_delete(&tile);
			return NULL;
		}
	}

	// tex is a reference
	texgz_tex_t* tex = naip_tile_cacheRestore(tile);
	if(tex == NULL)
	{
		return NULL;
	}

	// compute the downscaled texture for the next LOD
	texgz_tex_t* down = texgz_tex_downscale(tex);
	if(down == NULL)
	{
		return NULL;
	}

	if(naip_database_jpgSave(tex, 17, x, y) == 0)
	{
		goto fail_jpg;
	}

	// success
	return down;

	// failure
	fail_jpg:
		texgz_tex_delete(&down);
	return NULL;
}

static texgz_tex_t*
naip_database_tileSample(naip_database_t* self,
                         int zend, int zoom, int x, int y)
{
	assert(self);
	assert((zend == 12) || (zend == 17));

	texgz_tex_t* tex;
	tex = naip_database_checkpointRestore(self, zoom, x, y);
	if(tex)
	{
		return tex;
	}

	// end recursion
	if(zend == zoom)
	{
		if(zoom == 12)
		{
			return naip_database_tileSample12(self, x, y);
		}
		else
		{
			return naip_database_tileSample17(self, x, y);
		}
	}

	// terrain tiles only exist up to zoom 15
	// and zoom > 15 is covered by zoom 15
	short min   = 0;
	short max   = 0;
	int   flags = TERRAIN_NEXT_ALL;
	if(zoom < 15)
	{
		if(terrain_tile_header(".", x, y, zoom,
		                       &min, &max, &flags) == 0)
		{
			return NULL;
		}
	}

	// create a working tex
	tex = texgz_tex_new(256, 256, 256, 256,
	                    TEXGZ_UNSIGNED_BYTE,
	                    TEXGZ_RGBA, NULL);
	if(tex == NULL)
	{
		return NULL;
	}

	// recursively traverse terrain
	texgz_tex_t* tmp;
	if(flags & TERRAIN_NEXT_TL)
	{
		tmp = naip_database_tileSample(self,
		                               zend, zoom + 1,
		                               2*x, 2*y);
		if(tmp == NULL)
		{
			goto fail_recursion;
		}

		texgz_tex_blit(tmp, tex, 128, 128,
		               0, 0, 0, 0);
		texgz_tex_delete(&tmp);
	}

	if(flags & TERRAIN_NEXT_BL)
	{
		tmp = naip_database_tileSample(self,
		                               zend, zoom + 1,
		                               2*x, 2*y + 1);
		if(tmp == NULL)
		{
			goto fail_recursion;
		}

		texgz_tex_blit(tmp, tex, 128, 128,
		               0, 0, 0, 128);
		texgz_tex_delete(&tmp);
	}

	if(flags & TERRAIN_NEXT_TR)
	{
		tmp = naip_database_tileSample(self,
		                               zend, zoom + 1,
		                               2*x + 1, 2*y);
		if(tmp == NULL)
		{
			goto fail_recursion;
		}

		texgz_tex_blit(tmp, tex, 128, 128,
		               0, 0, 128, 0);
		texgz_tex_delete(&tmp);
	}

	if(flags & TERRAIN_NEXT_BR)
	{
		tmp = naip_database_tileSample(self,
		                               zend, zoom + 1,
		                               2*x + 1, 2*y + 1);
		if(tmp == NULL)
		{
			goto fail_recursion;
		}

		texgz_tex_blit(tmp, tex, 128, 128,
		               0, 0, 128, 128);
		texgz_tex_delete(&tmp);
	}

	// compute the downscaled texture for the next LOD
	texgz_tex_t* down = texgz_tex_downscale(tex);
	if(down == NULL)
	{
		goto fail_down;
	}

	if(naip_database_jpgSave(tex, zoom, x, y) == 0)
	{
		goto fail_jpg;
	}

	if(naip_database_checkpointSave(self, down,
	                                zoom, x, y) == 0)
	{
		goto fail_save;
	}

	texgz_tex_delete(&tex);

	// success
	return down;

	// failure
	fail_save:
		naip_database_jpgDelete(zoom, x, y);
	fail_jpg:
		texgz_tex_delete(&down);
	fail_down:
	fail_recursion:
		texgz_tex_delete(&tex);
	return NULL;
}

/***********************************************************
* public                                                   *
***********************************************************/

naip_database_t* naip_database_new(const char* fname)
{
	assert(fname);

	naip_database_t* self = (naip_database_t*)
	                        malloc(sizeof(naip_database_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		return NULL;
	}

	self->tiles = a3d_list_new();
	if(self->tiles == NULL)
	{
		goto fail_tiles;
	}

	self->nodes = a3d_list_new();
	if(self->nodes == NULL)
	{
		goto fail_nodes;
	}

	if(naip_database_readXml(self, fname) == 0)
	{
		goto fail_read;
	}

	// success
	return self;

	// failure
	fail_read:
		{
			a3d_listitem_t* iter = a3d_list_head(self->nodes);
			while(iter)
			{
				naip_node_t* node = (naip_node_t*)
				                    a3d_list_remove(self->nodes,
				                                    &iter);
				naip_node_delete(&node);
			}
			a3d_list_delete(&self->nodes);
		}
	fail_nodes:
			a3d_list_delete(&self->tiles);
	fail_tiles:
		free(self);
	return NULL;
}

void naip_database_delete(naip_database_t** _self)
{
	assert(_self);

	naip_database_t* self = *_self;
	if(self)
	{
		a3d_listitem_t* iter = a3d_list_head(self->nodes);
		while(iter)
		{
			naip_node_t* node = (naip_node_t*)
			                    a3d_list_remove(self->nodes,
			                                    &iter);
			naip_node_delete(&node);
		}
		a3d_list_delete(&self->nodes);

		iter = a3d_list_head(self->tiles);
		while(iter)
		{
			naip_tile_t* tile = (naip_tile_t*)
			                    a3d_list_remove(self->tiles,
			                                    &iter);
			naip_tile_delete(&tile);
		}
		a3d_list_delete(&self->tiles);
		free(self);
		*_self = NULL;
	}
}

int naip_database_sample(naip_database_t* self)
{
	assert(self);

	texgz_tex_t* tex;
	tex = naip_database_tileSample(self, 12, 0, 0, 0);
	if(tex == NULL)
	{
		LOGI("FAILURE");
		return 0;
	}

	texgz_tex_delete(&tex);
	LOGI("SUCCESS");
	return 1;
}
