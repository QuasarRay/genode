/*
 * \brief  Kernel-specific core's 'log' backend
 * \author Stefan Kalkowski
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <core_log.h>

/* Genode includes */
#include <drivers/uart/x86_pc.h>
#include <util/reconstructible.h>

using namespace Genode;


X86_uart &uart(uint16_t ioport = 0)
{
	enum { CLOCK = 0, BAUDRATE = 115200 };

	static Constructible<X86_uart> serial { };

	if (ioport)
		serial.destruct();

	if (!serial.constructed())
		serial.construct(ioport, CLOCK, BAUDRATE);

	return *serial;
}


void Core::init_core_uart(uint16_t port)
{
	uart(port);
}


void Core::Core_log::out(char const c)
{
	if (c == '\n')
		uart().put_char('\r');
	uart().put_char(c);
}
