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

#include "texgz/texgz_tex.h"

typedef struct
{
	char   fname[256];
	char   id[256];
	char   url[256];
	double t;
	double l;
	double b;
	double r;

	texgz_tex_t* tex;
} naip_node_t;

naip_node_t* naip_node_new(const char* id,
                           const char* url,
                           const char* t,
                           const char* l,
                           const char* b,
                           const char* r);
void         naip_node_delete(naip_node_t** _self);
int          naip_node_import(naip_node_t* self);
void         naip_node_tilesCovered(naip_node_t* self,
                                    int zoom,
                                    int* _x0, int* _y0,
                                    int* _x1, int* _y1);
