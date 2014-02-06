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

#ifndef flt_tile_H
#define flt_tile_H

#define FLT_MSBFIRST -1
#define FLT_LSBFIRST 1

typedef struct
{
	int    lat;
	int    lon;
	double lonL;
	double latB;
	double lonR;
	double latT;
	float  nodata;
	int    byteorder;
	int    nrows;
	int    ncols;
	short* height;
} flt_tile_t;

flt_tile_t* flt_tile_import(int arcs, int lat, int lon);
void        flt_tile_delete(flt_tile_t** _self);
int         flt_tile_sample(flt_tile_t* self,
                            double lat, double lon,
                            short* height);

#endif
