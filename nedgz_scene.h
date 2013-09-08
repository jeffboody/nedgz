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

#ifndef nedgz_scene_H
#define nedgz_scene_H

// scene graph node
typedef struct nedgz_scene_s
{
	// higher LOD nodes
	struct nedgz_scene_s* tl;
	struct nedgz_scene_s* tr;
	struct nedgz_scene_s* bl;
	struct nedgz_scene_s* br;

	// nedgz file exists
	int exists;

	// bounding box
	short min;
	short max;
} nedgz_scene_t;

nedgz_scene_t* nedgz_scene_new(void);
void           nedgz_scene_delete(nedgz_scene_t** _self);
nedgz_scene_t* nedgz_scene_import(const char* fname);
int            nedgz_scene_export(nedgz_scene_t* self, const char* fname);

#endif
