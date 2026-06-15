/*
 * \brief  Utilities
 * \author Alexander Boettcher
 * \date   2026-07-09
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <util/token.h>
#include <util/string.h>


struct Scanner_policy
{
	/**
	 * Return true if character belongs to a valid identifier
	 *
	 * \param c  character
	 * \param i  index of character in token
	 * \return   true if character is a valid identifier character
	 *
	 * Letters and equal characters are allowed anywhere in an
	 * identifier, digits must not appear at the beginning.
	 */
	static bool identifier_char(char c, unsigned i) {
		return Genode::is_letter(c) || (c == '=') || (i && Genode::is_digit(c)); }

	/**
	 * Check for end of quotation
	 *
	 * Checks if next character is non-backslashed quotation mark.
	 * The end of a quoted string is reached when encountering a '"'
	 * character that is not preceded by a backslash.
	 *
	 * \param s  pointer to null-terminated string with at least one
	 *           character
	 */
	static bool end_of_quote(const char *s) {
		return s[0] != '\\' && s[1] == '\"'; }
};


static void parse_cmdline(char const * cmdline, unsigned len,
                          auto const &fn_uart, auto const &fn_verbose)
{
	using namespace Genode;

	Token<Scanner_policy> token(cmdline, len);

	while (token.valid()) {

		if (token.len() >= 7) {

			String<8> text((char const *)token.start(), token.len());

			if (text == "serial=") {

				Span span(token.start() + 7, token.len() - 7);

				auto ioport = 0u;

				parse(span, ioport);

				fn_uart(ioport);
			} else
				if (text == "verbose")
					fn_verbose(true);
		}

		token = token.next();
	}
}
