/**
 * rutabaga: an OpenGL widget toolkit
 * Copyright (c) 2013 William Light.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "rutabaga/rutabaga.h"
#include "rutabaga/element.h"
#include "rutabaga/render.h"
#include "rutabaga/style.h"
#include "rutabaga/quad.h"
#include "rutabaga/stylequad.h"
#include "rutabaga/window.h"

#include "private/util.h"

static const GLubyte border_indices[] = {
	/**
	 * +---+---+---+
	 * | 1 | 2 | 3 |
	 * +---+---+---+
	 * | 4 | 5 | 6 |
	 * +---+---+---+
	 * | 7 | 8 | 9 |
	 * +---+---+---+
	 *
	 * the middle section is not drawn because it's conditionally drawn
	 * based on whether the texture has defined RTB_TEXTURE_FILL.
	 * if it is, we'll just draw with solid_indices.
	 */

	/* 1 */  0,  2,  1,  3,  2,  0,
	/* 2 */  1,  7,  4,  2,  7,  1,
	/* 3 */  4,  6,  5,  7,  6,  4,

	/* 4 */  3,  9,  2,  8,  9,  3,
	/* 5 */
	/* 6 */  7, 13,  6, 12, 13,  7,

	/* 7 */  8, 10,  9, 11, 10,  8,
	/* 8 */  9, 15, 12, 10, 15,  9,
	/* 9 */ 12, 14, 13, 15, 14, 12
};

static const GLubyte solid_indices[] = {
	2, 7, 9, 12
};

static const GLubyte outline_indices[] = {
	2, 7, 12, 9
};

/**
 * drawing
 */

static void
draw_solid(struct rtb_stylequad *self, const struct rtb_shader *shader,
		GLenum mode, const GLubyte *indices, GLsizei count)
{
	glBindBuffer(GL_ARRAY_BUFFER, self->vertices);
	glEnableVertexAttribArray(shader->vertex);
	glVertexAttribPointer(shader->vertex, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawElements(mode, count, GL_UNSIGNED_BYTE, indices);

	glDisableVertexAttribArray(shader->vertex);
}

static void
draw_textured(struct rtb_stylequad *self, const struct rtb_shader *shader,
		const struct rtb_stylequad_texture *tx)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tx->gl_handle);
	glUniform1i(shader->texture, 0);
	glUniform2f(shader->texture_size,
			tx->definition->w, tx->definition->h);

	glBindBuffer(GL_ARRAY_BUFFER, tx->coords);
	glEnableVertexAttribArray(shader->tex_coord);
	glVertexAttribPointer(shader->tex_coord,
			2, GL_FLOAT, GL_FALSE, 0, 0);

	draw_solid(self, shader, GL_TRIANGLES,
			border_indices, ARRAY_LENGTH(border_indices));

	if (tx->definition->flags & RTB_TEXTURE_FILL)
		draw_solid(self, shader, GL_TRIANGLE_STRIP,
				solid_indices, ARRAY_LENGTH(solid_indices));

	glDisableVertexAttribArray(shader->tex_coord);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUniform2f(shader->texture_size, 0.f, 0.f);
}

static void
draw(struct rtb_stylequad *self, struct rtb_shader *shader)
{
	struct rtb_element *owner = self->owner;

	rtb_render_set_position(owner, self->offset.x, self->offset.y);
	glUniform2f(shader->texture_size, 0.f, 0.f);

	if (self->properties.bg_color) {
		rtb_render_set_color(owner,
				self->properties.bg_color->r,
				self->properties.bg_color->g,
				self->properties.bg_color->b,
				self->properties.bg_color->a);

		draw_solid(self, shader, GL_TRIANGLE_STRIP,
				solid_indices, ARRAY_LENGTH(solid_indices));
	}

	if (self->border_image.definition)
		draw_textured(self, shader, &self->border_image);

	if (self->properties.border_color) {
		rtb_render_set_color(owner,
				self->properties.border_color->r,
				self->properties.border_color->g,
				self->properties.border_color->b,
				self->properties.border_color->a);

		glLineWidth(1.f);

		draw_solid(self, shader, GL_LINE_LOOP,
				outline_indices, ARRAY_LENGTH(outline_indices));
	}
}

void
rtb_stylequad_draw(struct rtb_stylequad *self)
{
	struct rtb_shader *shader = &self->owner->window->shaders.stylequad;

	rtb_render_reset(self->owner);
	rtb_render_use_shader(self->owner, shader);

	draw(self, shader);
}

void
rtb_stylequad_draw_with_modelview(struct rtb_stylequad *self, mat4 *modelview)
{
	struct rtb_shader *shader = &self->owner->window->shaders.stylequad;

	rtb_render_reset(self->owner);
	rtb_render_use_shader(self->owner, shader);
	rtb_render_set_modelview(self->owner, modelview->data);

	draw(self, shader);
}

/**
 * updating
 */

static void
set_border_tex_coords(struct rtb_stylequad_texture *tx)
{
	const struct rtb_style_texture_definition *d = tx->definition;

	GLfloat
		hpxl    = 1.f / d->w,
		vpxl    = 1.f / d->h,
		bdr_top = (d->border.top    * vpxl),
		bdr_rgt = (d->border.right  * hpxl),
		bdr_btm = (d->border.bottom * vpxl),
		bdr_lft = (d->border.left   * hpxl);

	GLfloat v[16][2] = {
		{0.f,           1.f},
		{0.f + bdr_lft, 1.f},
		{0.f + bdr_lft, 1.f - bdr_top},
		{0.f,           1.f - bdr_top},

		{1.f - bdr_rgt, 1.f},
		{1.f,           1.f},
		{1.f,           1.f - bdr_top},
		{1.f - bdr_rgt, 1.f - bdr_top},

		{0.f,           bdr_btm},
		{0.f + bdr_lft, bdr_btm},
		{0.f + bdr_lft, 0.f},
		{0.f,           0.f},

		{1.f - bdr_rgt, bdr_btm},
		{1.f,           bdr_btm},
		{1.f,           0.f},
		{1.f - bdr_rgt, 0.f},
	};

	glBindBuffer(GL_ARRAY_BUFFER, tx->coords);
	glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static int
load_texture(struct rtb_stylequad_texture *dst,
		const struct rtb_style_texture_definition *src)
{
	if (dst->definition == src)
		return 0;

	glBindTexture(GL_TEXTURE_2D, dst->gl_handle);

	if (!src) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0,
				0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
				src->w, src->h,
				0, GL_BGRA, GL_UNSIGNED_BYTE,
				RTB_ASSET_DATA(RTB_ASSET(src)));

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	dst->definition = src;
	return 0;
}

int
rtb_stylequad_set_border_image(struct rtb_stylequad *self,
		const struct rtb_style_texture_definition *tx)
{
	if (load_texture(&self->border_image, tx))
		return -1;

	if (tx)
		set_border_tex_coords(&self->border_image);

	return 0;
}

void
rtb_stylequad_update_style(struct rtb_stylequad *self)
{
	const struct rtb_style_property_definition *prop;
	struct rtb_element *elem = self->owner;

#define ASSIGN_AND_MAYBE_MARK_DIRTY(dest, val) do {        \
	if (dest != val) rtb_elem_mark_dirty(elem);            \
	dest = val;                                            \
} while (0)

#define CACHE_PROP(dest, name, type, member) do {          \
	prop = rtb_style_query_prop(elem->style,               \
			elem->state, name, type, 0);                   \
	if (prop)                                              \
		ASSIGN_AND_MAYBE_MARK_DIRTY(                       \
				self->properties.dest, &prop->member);   \
	else                                                   \
		ASSIGN_AND_MAYBE_MARK_DIRTY(                       \
				self->properties.dest, NULL);            \
} while (0)

#define CACHE_COLOR(dest, name) \
		CACHE_PROP(dest, name, RTB_STYLE_PROP_COLOR, color)
#define CACHE_TEXTURE(dest, name) \
		CACHE_PROP(dest, name, RTB_STYLE_PROP_TEXTURE, texture)

	CACHE_COLOR(bg_color, "background-color");
	CACHE_COLOR(fg_color, "color");
	CACHE_COLOR(border_color, "border-color");

	CACHE_TEXTURE(border_image, "border-image");

#undef CACHE_TEXTURE
#undef CACHE_COLOR
#undef CACHE_PROP
#undef ASSIGN_AND_MAYBE_MARK_DIRTY

	rtb_stylequad_set_border_image(self, self->properties.border_image);
}

void
rtb_stylequad_update_geometry(struct rtb_stylequad *self,
		const struct rtb_rect *rect)
{
	struct rtb_rect r;

	r.x  = -(rect->w / 2.f);
	r.y  = -(rect->h / 2.f);
	r.x2 = -r.x;
	r.y2 = -r.y;

	self->offset.x = rect->x + r.x2;
	self->offset.y = rect->y + r.y2;

	glBindBuffer(GL_ARRAY_BUFFER, self->vertices);

	if (self->border_image.definition) {
		const struct rtb_style_texture_definition *tx =
			self->border_image.definition;
		unsigned int
			bdr_top = tx->border.top,
			bdr_rgt = tx->border.right,
			bdr_btm = tx->border.bottom,
			bdr_lft = tx->border.left;

		GLfloat v[16][2] = {
			{r.x,            r.y},
			{r.x  + bdr_lft, r.y},
			{r.x  + bdr_lft, r.y + bdr_top},
			{r.x,            r.y + bdr_top},

			{r.x2 - bdr_rgt, r.y},
			{r.x2,           r.y},
			{r.x2,           r.y + bdr_top},
			{r.x2 - bdr_rgt, r.y + bdr_top},

			{r.x,            r.y2 - bdr_btm},
			{r.x  + bdr_lft, r.y2 - bdr_btm},
			{r.x  + bdr_lft, r.y2},
			{r.x,            r.y2},

			{r.x2 - bdr_rgt, r.y2 - bdr_btm},
			{r.x2,           r.y2 - bdr_btm},
			{r.x2,           r.y2},
			{r.x2 - bdr_rgt, r.y2}
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
	} else {
		GLfloat v[16][2] = {
			[2]  = {r.x,  r.y},
			[7]  = {r.x2, r.y},
			[12] = {r.x2, r.y2},
			[9]  = {r.x,  r.y2}
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
 * lifecycle
 */

#define INIT_STYLEQUAD_TEXTURE(tx) do {    \
	(tx)->definition = NULL;               \
	glGenBuffers(1, &(tx)->coords);        \
	glGenTextures(1, &(tx)->gl_handle);    \
} while (0)

#define FINI_STYLEQUAD_TEXTURE(tx) do {    \
	glDeleteBuffers(1, &(tx)->coords);     \
	glDeleteTextures(1, &(tx)->gl_handle); \
} while (0)

void
rtb_stylequad_init(struct rtb_stylequad *self, struct rtb_element *owner)
{
	self->owner = owner;

	glGenBuffers(1, &self->vertices);

	INIT_STYLEQUAD_TEXTURE(&self->border_image);
	INIT_STYLEQUAD_TEXTURE(&self->background_image);
}

void rtb_stylequad_fini(struct rtb_stylequad *self)
{
	FINI_STYLEQUAD_TEXTURE(&self->border_image);
	FINI_STYLEQUAD_TEXTURE(&self->background_image);

	glDeleteBuffers(1, &self->vertices);
}
