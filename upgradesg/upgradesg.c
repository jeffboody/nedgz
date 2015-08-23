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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "libpak/pak_file.h"
#include "nedgz/nedgz_scene.h"
#include "nedgz/nedgz_tile.h"

#define LOG_TAG "upgradesg"
#include "nedgz/nedgz_log.h"

/***********************************************************
* # 0_0.kv                                                 *
*                                                          *
* # current LOD                                            *
* # mask=top..bottom and is little endian bit order        *
* mask=00:00:00:00:00:00:00:00                             *
*                                                          *
* # next LOD                                               *
* # tl:tr:bl:br                                            *
* next=0:0:0:0                                             *
*                                                          *
* # bounding box (ned only)                                *
* min=0                                                    *
* max=14000                                                *
***********************************************************/

/***********************************************************
* private                                                  *
***********************************************************/

static int recursive_mkdir(const char* path)
{
	assert(path);
	LOGD("debug");

	int  len = strnlen(path, 255);
	char dir[256];
	int  i;
	for(i = 0; i < len; ++i)
	{
		dir[i]     = path[i];
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

static int export_pak(char* mask,
                      int zoom, int x, int y,
                      const char* in, const char* out)
{
	assert(mask);
	assert(in);
	assert(out);
	LOGD("debug zoom=%i, x=%i, y=%i, in=%s, out=%s"
	     zoom, x, y, in, out);

	unsigned char bmask[8] =
	{
		0, 0, 0, 0, 0, 0, 0, 0
	};

	pak_file_t* pak = pak_file_open(in, PAK_FLAG_READ);
	if(pak == NULL)
	{
		return 0;
	}

	int i;
	int j;
	char key[256];
	unsigned char buf[4096];
	FILE* f = NULL;
	for(i = 0; i < 8; ++i)
	{
		for(j = 0; j < 8; ++j)
		{
			snprintf(key, 256, "%i_%i", j, i);
			key[255] = '\0';
			int size = pak_file_seek(pak, key);
			if(size == 0)
			{
				continue;
			}

			char name_tex[256];
			snprintf(name_tex, 256, "%s/%i_%i", out, 8*x + j, 8*y + i);
			name_tex[255] = '\0';

			f = fopen(name_tex, "w");
			if(f == NULL)
			{
				LOGE("fopen %s failed", name_tex);
				goto fail_tex;
			}

			// copy the file
			int bytes;
			while(size > 0)
			{
				bytes = (size > 4096) ? 4096 : size;
				if(pak_file_read(pak, buf, bytes, 1) != 1)
				{
					LOGE("fread failed");
					goto fail_read;
				}

				if(fwrite(buf, bytes, 1, f) != 1)
				{
					LOGE("fwrite failed");
					goto fail_write;
				}

				size -= bytes;
			}
			fclose(f);
			f = NULL;

			// update the mask
			bmask[i] |= 1 << j;
		}
	}
	pak_file_close(&pak);

	snprintf(mask, 256, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
	         bmask[0], bmask[1], bmask[2], bmask[3],
	         bmask[4], bmask[5], bmask[6], bmask[7]);
	mask[255] = '\0';

	// success
	return 1;

	// failure
	fail_write:
	fail_read:
		fclose(f);
	fail_tex:
		pak_file_close(&pak);
	return 0;
}

static int upgrade_osm(nedgz_scene_t* scene,
                       int zoom, int x, int y,
                       const char* in, const char* out)
{
	assert(scene);
	LOGI("zoom=%i, x=%i, y=%i", zoom, x, y);

	char name_kv[256];
	char name_pak[256];
	char name_dir[256];
	snprintf(name_kv,  256, "%s/%i/%i_%i.kv",  out, zoom, x, y);
	snprintf(name_pak, 256, "%s/%i/%i_%i.pak", in,  zoom, x, y);
	snprintf(name_dir, 256, "%s/%i",  out, zoom);
	name_kv[255]  = '\0';
	name_pak[255] = '\0';
	name_dir[255] = '\0';

	if(recursive_mkdir(name_kv) == 0)
	{
		return 0;
	}

	FILE* f = fopen(name_kv, "w");
	if(f == NULL)
	{
		LOGE("fopen %s failed", name_kv);
		return 0;
	}

	if(scene->exists)
	{
		char mask[256];
		if(export_pak(mask, zoom, x, y, name_pak, name_dir) == 0)
		{
			goto fail_pak;
		}
		fprintf(f, "mask=%s\n", mask);
	}

	fprintf(f, "next=%i:%i:%i:%i\n",
	        scene->tl ? 1 : 0,
	        scene->tr ? 1 : 0,
	        scene->bl ? 1 : 0,
	        scene->br ? 1 : 0);

	fclose(f);

	// next LOD
	if(scene->tl)
	{
		if(upgrade_osm(scene->tl, zoom + 1, 2*x, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->tr)
	{
		if(upgrade_osm(scene->tr, zoom + 1, 2*x + 1, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->bl)
	{
		if(upgrade_osm(scene->bl, zoom + 1, 2*x, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->br)
	{
		if(upgrade_osm(scene->br, zoom + 1, 2*x + 1, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	// success
	return 1;

	// failure
	fail_pak:
		fclose(f);
	return 0;
}

static int upgrade_bluemarble(nedgz_scene_t* scene,
                              int zoom, int x, int y,
                              const char* in, const char* out)
{
	assert(scene);
	LOGI("zoom=%i, x=%i, y=%i", zoom, x, y);

	FILE* f = NULL;
	int month;
	for(month = 1; month <= 12; ++month)
	{
		char name_kv[256];
		char name_pak[256];
		char name_dir[256];
		snprintf(name_kv,  256, "%s/%i/%i/%i_%i.kv",  out, month, zoom, x, y);
		snprintf(name_pak, 256, "%s/%i/%i/%i_%i.pak", in, month, zoom, x, y);
		snprintf(name_dir, 256, "%s/%i/%i",  out, month, zoom);
		name_kv[255]  = '\0';
		name_pak[255] = '\0';
		name_dir[255] = '\0';

		if(recursive_mkdir(name_kv) == 0)
		{
			return 0;
		}

		if(scene->exists)
		{
			char mask[256];
			if(export_pak(mask, zoom, x, y, name_pak, name_dir) == 0)
			{
				goto fail_pak;
			}
		}
	}

	// next LOD
	if(scene->tl)
	{
		if(upgrade_bluemarble(scene->tl, zoom + 1, 2*x, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->tr)
	{
		if(upgrade_bluemarble(scene->tr, zoom + 1, 2*x + 1, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->bl)
	{
		if(upgrade_bluemarble(scene->bl, zoom + 1, 2*x, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->br)
	{
		if(upgrade_bluemarble(scene->br, zoom + 1, 2*x + 1, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	// success
	return 1;

	// failure
	fail_pak:
		fclose(f);
	return 0;
}

static int upgrade_hillshade(nedgz_scene_t* scene,
                             int zoom, int x, int y,
                             const char* in, const char* out)
{
	assert(scene);
	LOGI("zoom=%i, x=%i, y=%i", zoom, x, y);

	char name_kv[256];
	char name_pak[256];
	char name_dir[256];
	snprintf(name_kv,  256, "%s/%i/%i_%i.kv",  out, zoom, x, y);
	snprintf(name_pak, 256, "%s/%i/%i_%i.pak", in,  zoom, x, y);
	snprintf(name_dir, 256, "%s/%i",  out, zoom);
	name_kv[255]  = '\0';
	name_pak[255] = '\0';
	name_dir[255] = '\0';

	if(recursive_mkdir(name_kv) == 0)
	{
		return 0;
	}

	FILE* f = fopen(name_kv, "w");
	if(f == NULL)
	{
		LOGE("fopen %s failed", name_kv);
		return 0;
	}

	if(scene->exists)
	{
		char mask[256];
		if(export_pak(mask, zoom, x, y, name_pak, name_dir) == 0)
		{
			goto fail_pak;
		}
		fprintf(f, "mask=%s\n", mask);
	}

	fprintf(f, "next=%i:%i:%i:%i\n",
	        scene->tl ? 1 : 0,
	        scene->tr ? 1 : 0,
	        scene->bl ? 1 : 0,
	        scene->br ? 1 : 0);

	fclose(f);

	// next LOD
	if(scene->tl)
	{
		if(upgrade_hillshade(scene->tl, zoom + 1, 2*x, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->tr)
	{
		if(upgrade_hillshade(scene->tr, zoom + 1, 2*x + 1, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->bl)
	{
		if(upgrade_hillshade(scene->bl, zoom + 1, 2*x, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->br)
	{
		if(upgrade_hillshade(scene->br, zoom + 1, 2*x + 1, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	// success
	return 1;

	// failure
	fail_pak:
		fclose(f);
	return 0;
}

static int upgrade_ned(nedgz_scene_t* scene,
                       int zoom, int x, int y,
                       const char* in, const char* out)
{
	assert(scene);
	LOGI("zoom=%i, x=%i, y=%i", zoom, x, y);

	char name_kv[256];
	char name_dir[256];
	snprintf(name_kv,  256, "%s/%i/%i_%i.kv",  out, zoom, x, y);
	snprintf(name_dir, 256, "%s/%i",  out, zoom);
	name_kv[255]  = '\0';
	name_dir[255] = '\0';

	if(recursive_mkdir(name_kv) == 0)
	{
		return 0;
	}

	unsigned char bmask[8] =
	{
		0, 0, 0, 0, 0, 0, 0, 0
	};
	if(scene->exists)
	{
		nedgz_tile_t* ned = nedgz_tile_import(in, x, y, zoom);
		if(ned == NULL)
		{
			return 0;
		}

		int i;
		int j;
		for(i = 0; i < 8; ++i)
		{
			for(j = 0; j < 8; ++j)
			{
				if(nedgz_tile_getij(ned, i, j) == NULL)
				{
					continue;
				}

				// update the mask
				bmask[i] |= 1 << j;
			}
		}

		nedgz_tile_delete(&ned);
	}

	FILE* f = fopen(name_kv, "w");
	if(f == NULL)
	{
		LOGE("fopen %s failed", name_kv);
		return 0;
	}

	fprintf(f, "mask=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
	         bmask[0], bmask[1], bmask[2], bmask[3],
	         bmask[4], bmask[5], bmask[6], bmask[7]);
	fprintf(f, "next=%i:%i:%i:%i\n",
	        scene->tl ? 1 : 0,
	        scene->tr ? 1 : 0,
	        scene->bl ? 1 : 0,
	        scene->br ? 1 : 0);
	fprintf(f, "min=%i\n", (int) scene->min);
	fprintf(f, "max=%i\n", (int) scene->max);
	fclose(f);

	// next LOD
	if(scene->tl)
	{
		if(upgrade_ned(scene->tl, zoom + 1, 2*x, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->tr)
	{
		if(upgrade_ned(scene->tr, zoom + 1, 2*x + 1, 2*y, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->bl)
	{
		if(upgrade_ned(scene->bl, zoom + 1, 2*x, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	if(scene->br)
	{
		if(upgrade_ned(scene->br, zoom + 1, 2*x + 1, 2*y + 1, in, out) == 0)
		{
			return 0;
		}
	}

	return 1;
}

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, const char** argv)
{
	if(argc != 3)
	{
		LOGE("usage: %s <in> <out>", argv[0]);
		return EXIT_FAILURE;
	}

	const char* in  = argv[1];
	const char* out = argv[2];

	// import the scene graph
	char fname[256];
	snprintf(fname, 256, "%s/%s.sg", in, in);
	fname[255] = '\0';
	nedgz_scene_t* scene = nedgz_scene_import(fname);
	if(scene == NULL)
	{
		return EXIT_FAILURE;
	}

	// upgrade scene graph
	if(strcmp(in, "osm") == 0)
	{
		if(upgrade_osm(scene, 0, 0, 0, in, out) == 0)
		{
			goto fail_in;
		}
	}
	else if(strcmp(in, "bluemarble") == 0)
	{
		if(upgrade_bluemarble(scene, 0, 0, 0, in, out) == 0)
		{
			goto fail_in;
		}
	}
	else if(strcmp(in, "hillshade") == 0)
	{
		if(upgrade_hillshade(scene, 0, 0, 0, in, out) == 0)
		{
			goto fail_in;
		}
	}
	else if(strcmp(in, "ned") == 0)
	{
		if(upgrade_ned(scene, 0, 0, 0, in, out) == 0)
		{
			goto fail_in;
		}
	}
	else
	{
		LOGW("invalid %s", in);
		goto fail_in;
	}

	// success
	nedgz_scene_delete(&scene);
	return EXIT_SUCCESS;

	// failure
	fail_in:
		nedgz_scene_delete(&scene);
	return EXIT_FAILURE;
}
