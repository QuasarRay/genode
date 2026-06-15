/*
 * \brief   Entry code of boot loader
 * \author  Alexander Boettcher
 * \date    2026-06-03
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/internal/elf.h>

#include <util/hid.h>

#include "mbi1.h"
#include "mbi2.h"
#include "fb.h"
#include "util.h"


extern "C" void entry(unsigned, void *, unsigned) __attribute__((regparm(3)));

using namespace Genode;


/*
 * Check some conditions which are fatal and not easy to spot if boot continues
 */
bool sanity_check_bootloader(auto const img_offset)
{
	using namespace Boloa;

	auto const img_start = unsigned(&_prog_img_beg);
	auto const img_size  = unsigned(&_prog_img_end) - img_start;

	/* check for too large boot loader size */
	if (img_size > (1 << Mbi::LOG2_4MB))
		error("boot loader too large - assembly entry needs adjustments ...");
	else
	if (img_start & ((1u << Mbi::LOG2_16MB) - 1))
		error("boot loader not 16MB aligned linked");
	else
	if (img_offset & ((1u << Mbi::LOG2_16MB) - 1))
		error("boot loader not 16MB aligned loaded");
	else
		return true;

	return false;
}


template <typename T>
void with_mbi(void * mbi_ptr, auto offset, auto name, auto const &fn)
{
	using namespace Boloa;

	bool verbose     = false;
	auto uart_ioport = 0u;

	T mbi((addr_t(mbi_ptr)), offset);

	mbi.with_cmdline([&](auto cmdline_size, auto cmdline) {
		parse_cmdline(cmdline, cmdline_size, [&](auto ioport) {
			if (!ioport)
				return;

			uart_ioport = ioport;

			init_uart(uint16_t(ioport));

			sanity_check_bootloader(offset);
		}, [&](auto v) {
			verbose = v;
			if (verbose)
				mbi.enable_verbose();
		});
	});

	mbi.with_fb([&](auto addr, auto pitch, auto width, auto height, auto bpp, auto type) {

		bool const accessible_fb = (addr <= ~0u - pitch * height);

		if (accessible_fb && bpp == 32 && type == 1 /* LINEAR FRAMEBUFFER */)
			Framebuffer::init_fb(addr, pitch / 4, height);
		else
			warning("fb: no usable mode",
			        accessible_fb ? "" : ", fb above 4G",
			        bpp  == 32    ? "" : ", wrong bpp",
			        type == 1     ? "" : ", non linear fb");

		log("fb: ", Hex(addr), ", pitch:", Hex(pitch), ", ",
		    width, "x", height, "@", bpp, " type:", type);

	}, [] {
		warning("fb: not available");
	});

	if (verbose) {
		auto const img = unsigned(&_prog_img_beg);

		log("booted via ", name, " protocol, relocation offset: ",
		    offset > img ? "-" : "+",
		    Hex(offset > img ? img + offset : offset));

		mbi.dump_all_tags();
	} else
		log("booted via ", name, " protocol");

	if (!sanity_check_bootloader(offset))
		return;

	fn(mbi, uart_ioport);
}


void entry(unsigned magic, void *mbi, uint32_t offset)
{
	using namespace Boloa;

	auto run_mbi = [](auto &mbi, auto const uart_ioport) {

		auto hid_report_max_size = 1ul * 1024 * 1024;

		mbi.find_memory({ .min_phys  = 0x10000u,
		                  .max_phys  = ~0u,
		                  .min_align = 12 },
		                hid_report_max_size, [&](auto start, auto, auto) {

			/* memory above 4G not accessible */
			if (start > ((~0u - hid_report_max_size) & ~0xffful))
				return;

			Byte_range_ptr range((char *)start, hid_report_max_size);

			auto result = Generator::generate(range, "preboot", [&](auto &g){

				g.node("boot", [&] {
					mbi.with_fb([&](auto addr, auto pitch, auto width, auto height, auto bpp, auto type) {
						g.node("framebuffer", [&] {
							g.attribute("phys",   String<32>(Hex(addr)));
							g.attribute("width",  width);
							g.attribute("height", height);
							g.attribute("bpp",    bpp);
							g.attribute("type",   type);
							g.attribute("pitch",  pitch);
						});
					}, [] { });

					if (uart_ioport)
						g.node("uart", [&] {
							g.attribute("ioport", String<8>(Hex(uart_ioport)));
						});
				});
			});

			result.with_result([&](auto const size) {
				mbi.boot_next_module({unsigned(start), size});
			}, [] (auto) {
				error("hid failure");
			});
		});
	};

	if      (magic == Mbi1::MAGIC) with_mbi<Mbi1>(mbi, offset, "MBI1", run_mbi);
	else if (magic == Mbi2::MAGIC) with_mbi<Mbi2>(mbi, offset, "MBI2", run_mbi);

	error("boot failed - bye");

	while (true)
		asm volatile ("hlt");
}
