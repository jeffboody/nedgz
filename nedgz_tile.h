/*
 * Copyright (c) 2013 Jeff Boody
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

#ifndef nedgz_tile_H
#define nedgz_tile_H

#define NEDGZ_SUBTILE_COUNT 8
#define NEDGZ_SUBTILE_SIZE  32
#define NEDGZ_NODATA        -9999

// data units are measured in feet because the highest
// point, Mt Everest is 29029 feet,  which matches up
// nicely with range of shorts (-32768 to 32767)
typedef struct
{
	short data[NEDGZ_SUBTILE_SIZE*NEDGZ_SUBTILE_SIZE];
} nedgz_subtile_t;

typedef struct
{
	int x;
	int y;
	int zoom;
	double latT;
	double lonL;
	double latB;
	double lonR;

	// individual subtiles may be null when not defined
	nedgz_subtile_t* subtile[NEDGZ_SUBTILE_COUNT*NEDGZ_SUBTILE_COUNT];
} nedgz_tile_t;

nedgz_tile_t*    nedgz_tile_new(int x, int y, int zoom);
void             nedgz_tile_delete(nedgz_tile_t** _self);
nedgz_tile_t*    nedgz_tile_import(const char* base,
                                   int x, int y, int zoom);
int              nedgz_tile_export(nedgz_tile_t* self,
                                   const char* base);
nedgz_subtile_t* nedgz_tile_getij(nedgz_tile_t* self,
                                  int i, int j);
int              nedgz_tile_set(nedgz_tile_t* self,
                                int i, int j,
                                int m, int n,
                                short h);
void             nedgz_tile_coord(nedgz_tile_t* self,
                                  int i, int j,
                                  int m, int n,
                                  double* lat, double* lon);

#endif
