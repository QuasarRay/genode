/*
 * \brief   Framebuffer support
 * \author  Alexander Boettcher
 * \date    2026-06-08
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#pragma once


#include <os/pixel_rgb888.h>

#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/text_painter.h>
#include <nitpicker_gfx/tff_font.h>


namespace Boloa
{
	using namespace Genode;

	class Framebuffer;

	void init_uart(uint16_t);

	extern "C" char _binary_mono_tff_start[];
}


class Boloa::Framebuffer
{
	private:

		using Glyph = Tff_font::Static_glyph_buffer<4096>;

		Glyph    _glyph { };
		Tff_font _font  { _binary_mono_tff_start, _glyph };

		uint64_t _pixels { };
		uint32_t _width  { };
		uint32_t _height { };

		int32_t  _x { 0 };
		int32_t  _y { 0 };

	public:

		Framebuffer() { }

		void init(uint64_t const pixels, uint32_t width, uint32_t height)
		{
			_pixels = pixels;
			_width  = width;
			_height = height;
		}

		void out(char const c, Log::Type const type)
		{
			if (!_pixels)
				return;

			Color black(Color::rgb(  0,   0,   0));
			Color white(Color::rgb(255, 255, 255));
			Color red  (Color::rgb(255,   0,   0));
			Color blue (Color::rgb(  0,   0, 255));

			auto const box   = _font.bounding_box();
			auto const color = type == Log::Type::WARNING ? blue
			                 : type == Log::Type::ERROR   ? red : white;

			Surface<Pixel_rgb888> surface(reinterpret_cast<Pixel_rgb888 *>(_pixels),
			                              { _width, _height });

			if (_x == 0) {
				/* clear newline */
				Box_painter::paint(surface, { { 0, _y }, { _width, box.h * 2} },
				                   black);

				/* visible marker on next line */
				Box_painter::paint(surface, { { 0, _y + int32_t(box.h) },
				                              { box.w, box.h } }, white);
			}

			if (c == '\r') return;
			if (c == '\n') {
				_x = 0;
				_y = (uint32_t(_y + box.h * 2) >= _height) ? 0 : _y + box.h;
				return;
			}

			char buffer[] = { c, 0 };

			Text_painter::paint(surface, { _x, _y }, _font, color, buffer);

			_x += box.w;
		}

		static void init_fb(uint64_t, uint32_t, uint32_t);
};
