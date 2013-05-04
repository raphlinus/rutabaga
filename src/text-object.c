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

#include <stdlib.h>

#include "rutabaga/rutabaga.h"
#include "rutabaga/object.h"
#include "rutabaga/window.h"
#include "rutabaga/render.h"
#include "rutabaga/shader.h"
#include "rutabaga/style.h"

#include "rutabaga/text-object.h"

#include "freetype-gl/freetype-gl.h"
#include "freetype-gl/vertex-buffer.h"

#include "private/utf8.h"

struct text_vertex {
	float x, y;
	float s, t;
};

void rtb_text_object_update(rtb_text_object_t *self, const rtb_utf8_t *text)
{
	texture_font_t *font = self->font->txfont;
	float x0, y0, x1, y1;
	float x, y;

	char32_t codepoint, prev_codepoint;
	uint32_t state, prev_state;

	assert(text);
	vertex_buffer_clear(self->vertices);

	x  = 0.f;
	x1 = 0.f;
	y  = ceilf(font->height / 2.f) - font->descender + 1.f;

	state = prev_state = UTF8_ACCEPT;
	prev_codepoint = 0;

	for (; *text; prev_state = state, text++) {
		texture_glyph_t *glyph;

		switch(u8dec(&state, &codepoint, *text)) {
		case UTF8_ACCEPT:
			break;

		case UTF8_REJECT:
			printf(" :: what\n");

			if (prev_state != UTF8_ACCEPT)
				text--;

			codepoint = 0xFFFD;
			state = UTF8_ACCEPT;
			break;

		default:
			continue;
		}

		glyph = texture_font_get_glyph(font, codepoint);
		if (!glyph)
			continue;

		if (prev_codepoint)
			x += texture_glyph_get_kerning(glyph, prev_codepoint);

		x0 = x + glyph->offset_x;
		y0 = y - glyph->offset_y;
		x1 = x0 + glyph->width;
		y1 = y0 + glyph->height;

		GLuint indices[6] = {0, 1, 2, 0, 2, 3};
		struct text_vertex vertices[4] = {
			{   .x = x0,
				.y = y0,
				.s = glyph->s0,
				.t = glyph->t0},
			{   .x = x0,
				.y = y1,
				.s = glyph->s0,
				.t = glyph->t1},
			{   .x = x1,
				.y = y1,
				.s = glyph->s1,
				.t = glyph->t1},
			{   .x = x1,
				.y = y0,
				.s = glyph->s1,
				.t = glyph->t0}
		};

		vertex_buffer_push_back(self->vertices, vertices, 4, indices, 6);

		x += glyph->advance_x;
		prev_codepoint = codepoint;
	}

	vertex_buffer_upload(self->vertices);
	self->h = font->height;
	self->w = ceilf(x1);

	self->xpad = floorf(font->size * 2.f);
	self->ypad = floorf(font->height / 2.f);
}

void rtb_text_object_render(rtb_text_object_t *self, rtb_obj_t *parent,
		float x, float y, rtb_draw_state_t state)
{
	rtb_font_manager_t *fm = self->fm;
	GLuint program = fm->shaders.alpha.program;

	if (!vertex_buffer_size(self->vertices))
		return;

	rtb_render_use_program(parent, &fm->shaders.alpha);
	glBindTexture(GL_TEXTURE_2D, fm->atlas->id);

	glUniform1i(glGetUniformLocation(program, "texture"), 0);

	/* normal */
	glUniform2f(glGetUniformLocation(program, "offset"), x, y);
	rtb_render_use_style_fg(parent, state);

	vertex_buffer_render(self->vertices, GL_TRIANGLES);
}

rtb_text_object_t *rtb_text_object_new(rtb_font_manager_t *fm,
		rtb_font_t *font, const rtb_utf8_t *text)
{
	rtb_text_object_t *self = calloc(1, sizeof(*self));

	if (!font)
		font = &fm->fonts.main;

	self->fm = fm;
	self->vertices = vertex_buffer_new("position:2f,tex_coord:2f");
	self->font = font;

	if (text)
		rtb_text_object_update(self, text);

	self->xpad = floorf(font->txfont->size * 2.f);
	self->ypad = floorf(font->txfont->height / 2.f);

	return self;
}

void rtb_text_object_free(rtb_text_object_t *self)
{
	vertex_buffer_delete(self->vertices);
	free(self);
}
