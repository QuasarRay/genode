/*
 * \brief  Output support
 * \author Alexander Boettcher
 * \date   2026-06-03
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <base/log.h>
#include <base/thread.h>

#include <drivers/uart/x86_pc.h>

#include "fb.h"


using namespace Genode;


static Boloa::Framebuffer & fb(uint64_t pixels = 0, uint32_t w = 0, uint32_t h = 0)
{
	static Boloa::Framebuffer fbc;

	if (pixels)
		fbc.init(pixels, w, h);

	return fbc;
}


static X86_uart & uart(uint16_t port = 0)
{
	static X86_uart serial(port, 0 /* clock */, 115'200 /* baudrate */);

	return serial;
}


void Boloa::Framebuffer::init_fb(uint64_t pixels, uint32_t w, uint32_t h)
{
	fb(pixels, w, h);
}


void Boloa::init_uart(uint16_t port)
{
	uart(port);
}


static Genode::Log::Type log_type;


void Log::_acquire(Type type)
{
	_type    = type;
	log_type = type;
}


void Log::_release()
{
	/* reset newline */
	_output.out_string("\n");

	log_type = { };
	_type    = { };
}


struct Boot_log
{
	void out_serial(char const *);
	void out_fb    (char const *);

	void output(char const * str);
};


Log &Log::log()
{
	struct Buffer
	{
		struct Write_fn : Boot_log
		{
			void operator () (char const *s) { output(s); }
		} function { };

		Buffered_output<512, Write_fn> buffer { function };
		Log                            log    { buffer   };
	};

	static Buffer buffer { };
	return buffer.log;
}


void Boot_log::output(char const * str)
{
	if (!str)
		return;

	out_serial(str);
	out_fb    (str);
}


void Boot_log::out_fb(char const * const str)
{
	for (unsigned i = 0; str[i]; i++) fb().out(str[i], log_type);
}


void Boot_log::out_serial(char const * const str)
{
	/* color the warning and error messages */
	switch (log_type) {
	case Log::WARNING: {

		auto const color = "\033[34mWarning: ";

		for (unsigned i = 0; color[i]; i++) uart().put_char(color[i]);

		break;
	}
	case Log::ERROR: {

		auto const color = "\033[31mError: ";

		for (unsigned i = 0; color[i]; i++) uart().put_char(color[i]);

		break;
	}
	case Log::LOG:
		break;
	};

	/* message */
	for (unsigned i = 0; str[i]; i++) {
		auto c = str[i];
		if (c == '\n')
			uart().put_char('\r');

		uart().put_char(c);
	}

	/* reset color */
	switch (log_type) {
	case Log::WARNING:
	case Log::ERROR: {
		auto const color = "\033[0m";

		for (unsigned i = 0; color[i]; i++) uart().put_char(color[i]);

		break;
	}
	case Log::LOG:
		break;
	};
}
