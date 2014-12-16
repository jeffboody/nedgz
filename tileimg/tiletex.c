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
#include <string.h>
#include "texgz/texgz_tex.h"
#include "texgz/texgz_png.h"

#define LOG_TAG "tileimg"
#include "texgz/texgz_log.h"

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		LOGE("usage: %s <png>", argv[0]);
		return EXIT_FAILURE;
	}

	texgz_tex_t* tex = texgz_png_import(argv[1]);
	if(tex == NULL)
	{
		return EXIT_FAILURE;
	}

	if(texgz_tex_convert(tex, TEXGZ_UNSIGNED_SHORT_5_6_5, TEXGZ_RGB) == 0)
	{
		goto fail_convert;
	}

	char filename[256];
	texgz_tex_t* tile = NULL;
	int i, j, r;
	int rows, cols;
	int bpp = texgz_tex_bpp(tex);
	unsigned char* src;
	unsigned char* dst;
	for(i = 0; i < tex->height; i += 256)
	{
		for(j = 0; j < tex->width; j += 256)
		{
			tile = texgz_tex_new(256, 256,
			                     256, 256,
			                     TEXGZ_UNSIGNED_SHORT_5_6_5,
			                     TEXGZ_RGB,
			                     NULL);
			if(tile == 0)
			{
				goto fail_tile_new;
			}

			rows  = ((tex->height - i) >= 256) ? 256 : (tex->height - i);
			cols  = ((tex->width - j) >= 256) ? 256 : (tex->width - j);
			for(r = 0; r < rows; ++r)
			{
				src = &tex->pixels[(i + r)*tex->stride*bpp + j*bpp];
				dst = &tile->pixels[r*256*bpp];
				memcpy(dst, src, cols*bpp);
			}

			snprintf(filename, 256, "%i_%i.texgz", j, i);
			if(texgz_tex_export(tile, filename) == 0)
			{
				goto fail_export;
			}

			texgz_tex_delete(&tile);
		}
	}

	// success
	texgz_tex_delete(&tex);
	return EXIT_SUCCESS;

	// failure
	fail_export:
		texgz_tex_delete(&tile);
	fail_tile_new:
	fail_convert:
		texgz_tex_delete(&tex);
	return EXIT_FAILURE;
}
