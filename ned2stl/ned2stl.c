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
#include <math.h>
#include <stdio.h>
#include "a3d/a3d_GL.h"
#include "a3d/math/a3d_vec3f.h"
#include "nedgz/nedgz_util.h"
#include "nedgz/nedgz_tile.h"

#define LOG_TAG "ned2stl"
#include "nedgz/nedgz_log.h"

static void coord2xy(double lat, double lon,
              float* x, float* y)
{
	assert(x);
	assert(y);
	LOGD("debug lat=%lf, lon=%lf", lat, lon);

	// use home as the origin
	double lat2meter = 111072.12110934;
	double lon2meter = 85337.868965619;
	double home_lat  = 40.061295;
	double home_lon  =-105.214552;

	*x = (float) ((lon - home_lon)*lon2meter);
	*y = (float) ((lat - home_lat)*lat2meter);
}

static void getp(nedgz_tile_t* tile, float s, int r, int c, float* x, float* y, float* z)
{
	assert(tile);
	assert(x);
	assert(y);
	assert(z);
	LOGD("debug s=%f, r=%i, c=%i", s, r, c);

	int i = r/NEDGZ_SUBTILE_SIZE;
	int j = c/NEDGZ_SUBTILE_SIZE;
	int m = r % NEDGZ_SUBTILE_SIZE;
	int n = c % NEDGZ_SUBTILE_SIZE;
	short height = 0;
	nedgz_tile_height(tile, i, j, m, n, &height);
	*z = nedgz_feet2meters((float) height);

	double lat;
	double lon;
	nedgz_tile_coord(tile, i, j, m, n,
                     &lat, &lon);
	coord2xy(lat, lon, x, y);
	*x *= s;
	*y *= s;
	*z *= s;
}

int main(int argc, char** argv)
{
	if(argc != 6)
	{
		LOGE("usage: %s <base> <zoom> <x> <y> <out.stl>", argv[0]);
		return EXIT_FAILURE;
	}

	int           zoom = (int) strtol(argv[2], NULL, 0);
	int           x    = (int) strtol(argv[3], NULL, 0);
	int           y    = (int) strtol(argv[4], NULL, 0);
	nedgz_tile_t* tile = nedgz_tile_import(argv[1], x, y, zoom);
	if(tile == NULL)
	{
		return EXIT_FAILURE;
	}

	FILE* f = fopen(argv[5], "w");
	if(f == NULL)
	{
		LOGE("fopen failed");
		goto fail_fopen;
	}

	char header[80];
	if(fwrite(&header, sizeof(header), 1, f) != 1)
	{
		LOGE("fwrite failed");
		goto fail_fwrite;
	}

	int size = NEDGZ_SUBTILE_COUNT*NEDGZ_SUBTILE_SIZE;
	unsigned int count = 4*(size - 1)*(size - 1) + 2*4*(size - 1);
	if(fwrite(&count, sizeof(unsigned int), 1, f) != 1)
	{
		LOGE("fwrite failed");
		goto fail_fwrite;
	}

	// scale to 6 inches
	// 25.4 mm is 1 inch
	a3d_vec3f_t a;
	a3d_vec3f_t b;
	getp(tile, 1.0f, 0, 0, &a.x, &a.y, &a.z);
	getp(tile, 1.0f, size - 1, size - 1, &b.x, &b.y, &b.z);
	float dx = fabs(b.x - a.x);
	float s = (6.0f*0.0254f)/dx;

	// find min/max
	int r;
	int c;
	short       attrib = 0;
	float       xmin = 0.0f;
	float       ymin = 0.0f;
	float       zmin = 0.0f;
	float       xmax = 0.0f;
	float       ymax = 0.0f;
	float       zmax = 0.0f;
	for(r = 0; r < size; ++r)
	{
		for(c = 0; c < size; ++c)
		{
			a3d_vec3f_t p;
			getp(tile, s, r, c, &p.x, &p.y, &p.z);

			if((r == 0) && (c == 0))
			{
				xmin = p.x;
				ymin = p.y;
				zmin = p.z;
				xmax = p.x;
				ymax = p.y;
				zmax = p.z;
			}
			else
			{
				if(p.x < xmin) xmin = p.x;
				if(p.y < ymin) ymin = p.y;
				if(p.z < zmin) zmin = p.z;
				if(p.x > xmax) xmax = p.x;
				if(p.y > ymax) ymax = p.y;
				if(p.z > zmax) zmax = p.z;
			}
		}
	}

	// export points, normals and attrib for top/bottom
	float cx = xmin + (xmax - xmin)/2.0f;
	float cy = ymin + (ymax - ymin)/2.0f;
	for(r = 0; r < (size - 1); ++r)
	{
		for(c = 0; c < (size - 1); ++c)
		{
			// points
			a3d_vec3f_t p00;
			a3d_vec3f_t p01;
			a3d_vec3f_t p10;
			a3d_vec3f_t p11;
			getp(tile, s, r + 0, c + 0, &p00.x, &p00.y, &p00.z);
			getp(tile, s, r + 0, c + 1, &p01.x, &p01.y, &p01.z);
			getp(tile, s, r + 1, c + 0, &p10.x, &p10.y, &p10.z);
			getp(tile, s, r + 1, c + 1, &p11.x, &p11.y, &p11.z);
			p00.x -= cx;
			p01.x -= cx;
			p10.x -= cx;
			p11.x -= cx;
			p00.y -= cy;
			p01.y -= cy;
			p10.y -= cy;
			p11.y -= cy;
			p00.z = p00.z - zmin + s*100.0f;
			p01.z = p01.z - zmin + s*100.0f;
			p10.z = p10.z - zmin + s*100.0f;
			p11.z = p11.z - zmin + s*100.0f;

			// vectors
			a3d_vec3f_t v0010;
			a3d_vec3f_t v0001;
			a3d_vec3f_t v1101;
			a3d_vec3f_t v1110;
			a3d_vec3f_subv_copy(&p10, &p00, &v0010);
			a3d_vec3f_subv_copy(&p01, &p00, &v0001);
			a3d_vec3f_subv_copy(&p01, &p11, &v1101);
			a3d_vec3f_subv_copy(&p10, &p11, &v1110);

			// normals
			a3d_vec3f_t n0;
			a3d_vec3f_t n1;
			a3d_vec3f_cross_copy(&v0010, &v0001, &n0);
			a3d_vec3f_cross_copy(&v1101, &v1110, &n1);
			a3d_vec3f_normalize(&n0);
			a3d_vec3f_normalize(&n1);

			/*
			 * top-triangle 0
			 */

			// write normal
			if(fwrite(&n0, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write points
			if(fwrite(&p00, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write "null" attrib
			if(fwrite(&attrib, sizeof(short), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			/*
			 * top-triangle 1
			 */

			// write normal
			if(fwrite(&n1, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write points
			if(fwrite(&p11, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write "null" attrib
			if(fwrite(&attrib, sizeof(short), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			/*
			 * setup bottom triangle
			 */

			a3d_vec3f_t nb;
			nb.x = 0.0f;
			nb.y = 0.0f;
			nb.z = -1.0f;
			p00.z = 0.0f;
			p01.z = 0.0f;
			p10.z = 0.0f;
			p11.z = 0.0f;

			/*
			 * bottom-triangle 0
			 */

			// write normal
			if(fwrite(&nb, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write points
			if(fwrite(&p00, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write "null" attrib
			if(fwrite(&attrib, sizeof(short), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			/*
			 * bottom-triangle 1
			 */

			// write normal
			if(fwrite(&nb, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write points
			if(fwrite(&p11, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
			if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}

			// write "null" attrib
			if(fwrite(&attrib, sizeof(short), 1, f) != 1)
			{
				LOGE("fwrite failed");
				goto fail_fwrite;
			}
		}
	}

	// export points, normals and attrib for north edge
	int i;
	for(i = 0; i < (size - 1); ++i)
	{
		// points
		a3d_vec3f_t p00;
		a3d_vec3f_t p01;
		a3d_vec3f_t p10;
		a3d_vec3f_t p11;
		getp(tile, s, 0, i + 0, &p00.x, &p00.y, &p00.z);
		getp(tile, s, 0, i + 1, &p01.x, &p01.y, &p01.z);
		p00.x -= cx;
		p01.x -= cx;
		p10.x = p00.x;
		p11.x = p01.x;
		p00.y -= cy;
		p01.y -= cy;
		p10.y = p00.y;
		p11.y = p01.y;
		p00.z = p00.z - zmin + s*100.0f;
		p01.z = p01.z - zmin + s*100.0f;
		p10.z = 0.0f;
		p11.z = 0.0f;

		/*
		 * triangle 0
		 */

		// write normal
		a3d_vec3f_t n;
		n.x = 0.0f;
		n.y = 1.0f;
		n.z = 0.0f;
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p00, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		/*
		 * triangle 1
		 */

		// write normal
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p11, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
	}

	// export points, normals and attrib for south edge
	for(i = 0; i < (size - 1); ++i)
	{
		// points
		a3d_vec3f_t p00;
		a3d_vec3f_t p01;
		a3d_vec3f_t p10;
		a3d_vec3f_t p11;
		getp(tile, s, (size - 1), i + 0, &p00.x, &p00.y, &p00.z);
		getp(tile, s, (size - 1), i + 1, &p01.x, &p01.y, &p01.z);
		p00.x -= cx;
		p01.x -= cx;
		p10.x = p00.x;
		p11.x = p01.x;
		p00.y -= cy;
		p01.y -= cy;
		p10.y = p00.y;
		p11.y = p01.y;
		p00.z = p00.z - zmin + s*100.0f;
		p01.z = p01.z - zmin + s*100.0f;
		p10.z = 0.0f;
		p11.z = 0.0f;

		/*
		 * triangle 0
		 */

		// write normal
		a3d_vec3f_t n;
		n.x = 0.0f;
		n.y = -1.0f;
		n.z = 0.0f;
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p00, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		/*
		 * triangle 1
		 */

		// write normal
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p11, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
	}

	// export points, normals and attrib for west edge
	for(i = 0; i < (size - 1); ++i)
	{
		// points
		a3d_vec3f_t p00;
		a3d_vec3f_t p01;
		a3d_vec3f_t p10;
		a3d_vec3f_t p11;
		getp(tile, s, i + 0, 0, &p00.x, &p00.y, &p00.z);
		getp(tile, s, i + 1, 0, &p01.x, &p01.y, &p01.z);
		p00.x -= cx;
		p01.x -= cx;
		p10.x = p00.x;
		p11.x = p01.x;
		p00.y -= cy;
		p01.y -= cy;
		p10.y = p00.y;
		p11.y = p01.y;
		p00.z = p00.z - zmin + s*100.0f;
		p01.z = p01.z - zmin + s*100.0f;
		p10.z = 0.0f;
		p11.z = 0.0f;

		/*
		 * triangle 0
		 */

		// write normal
		a3d_vec3f_t n;
		n.x = -1.0f;
		n.y = 0.0f;
		n.z = 0.0f;
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p00, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		/*
		 * triangle 1
		 */

		// write normal
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p11, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
	}

	// export points, normals and attrib for east edge
	for(i = 0; i < (size - 1); ++i)
	{
		// points
		a3d_vec3f_t p00;
		a3d_vec3f_t p01;
		a3d_vec3f_t p10;
		a3d_vec3f_t p11;
		getp(tile, s, i + 0, (size - 1), &p00.x, &p00.y, &p00.z);
		getp(tile, s, i + 1, (size - 1), &p01.x, &p01.y, &p01.z);
		p00.x -= cx;
		p01.x -= cx;
		p10.x = p00.x;
		p11.x = p01.x;
		p00.y -= cy;
		p01.y -= cy;
		p10.y = p00.y;
		p11.y = p01.y;
		p00.z = p00.z - zmin + s*100.0f;
		p01.z = p01.z - zmin + s*100.0f;
		p10.z = 0.0f;
		p11.z = 0.0f;

		/*
		 * triangle 0
		 */

		// write normal
		a3d_vec3f_t n;
		n.x = 1.0f;
		n.y = 0.0f;
		n.z = 0.0f;
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p00, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		/*
		 * triangle 1
		 */

		// write normal
		if(fwrite(&n, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write points
		if(fwrite(&p11, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p10, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
		if(fwrite(&p01, 3*sizeof(float), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}

		// write "null" attrib
		if(fwrite(&attrib, sizeof(short), 1, f) != 1)
		{
			LOGE("fwrite failed");
			goto fail_fwrite;
		}
	}

	nedgz_tile_delete(&tile);
	fclose(f);

	// success
	return EXIT_SUCCESS;

	// failure
	fail_fwrite:
		fclose(f);
	fail_fopen:
		nedgz_tile_delete(&tile);
	return EXIT_FAILURE;
}
