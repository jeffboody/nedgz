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

#define LOG_TAG "unpakblue"
#include "nedgz/nedgz_log.h"

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

static int export_pak(int zoom, int x, int y,
                      const char* in, const char* out)
{
	assert(in);
	assert(out);
	LOGD("debug zoom=%i, x=%i, y=%i, in=%s, out=%s"
	     zoom, x, y, in, out);

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
			snprintf(name_tex, 256, "%s/%i/%i.texz", out, 8*x + j, 8*y + i);
			name_tex[255] = '\0';

			if(recursive_mkdir(name_tex) == 0)
			{
				goto fail_mkdir;
			}


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
		}
	}
	pak_file_close(&pak);

	// success
	return 1;

	// failure
	fail_write:
	fail_read:
		fclose(f);
	fail_tex:
	fail_mkdir:
		pak_file_close(&pak);
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
		char name_pak[256];
		char name_dir[256];
		snprintf(name_pak, 256, "%s/%i/%i/%i_%i.pak", in, month, zoom, x, y);
		snprintf(name_dir, 256, "%s/%i/%i",  out, month, zoom);
		name_pak[255] = '\0';
		name_dir[255] = '\0';

		if(scene->exists)
		{
			if(export_pak(zoom, x, y, name_pak, name_dir) == 0)
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
	if(strcmp(in, "bluemarble") == 0)
	{
		if(upgrade_bluemarble(scene, 0, 0, 0, in, out) == 0)
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
