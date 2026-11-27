/*
 * \brief  Multiboot 1 handling
 * \author Alexander Boettcher
 * \date   2026-06-15
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#pragma once


#include <util/mmio.h>
#include <util/string.h>

#include "mbi.h"


namespace Boloa {

	using namespace Genode;

	class Mbi1;

}


class Boloa::Mbi1 : Mmio<116>, Boloa::Mbi
{
	public:

		enum { MAGIC = 0x2badb002, };

		addr_t mbi_size() const override { return Mmio::SIZE; }
		addr_t mbi_base() const override { return base(); }

		struct Flags : Register<0x00, 32>
		{
			struct Cmdline : Bitfield<2, 1> { };
			struct Module  : Bitfield<3, 1> { };
			struct Mem_map : Bitfield<6, 1> { };
		};

		struct Cmdline : Register < 16, 32> { };

		struct Mmap_length: Register<0x2c, 32> { };
		struct Mmap_addr  : Register<0x30, 32> { };

		struct Drives_length : Register<0x34, 32> { };
		struct Drives_addr   : Register<0x38, 32> { };

		struct Memory : Genode::Mmio<0x1c> /* spec: Mmap */
		{
			struct Size   : Register <0x00, 32> { };
			struct Addr   : Register <0x04, 64> { };
			struct Length : Register <0x0c, 64> { };
			struct Type   : Register <0x14,  8> { enum { MEMORY = 1 }; };

			Memory(addr_t mmap = 0) : Mmio({(char *)mmap, Mmio::SIZE}) { }
		};

		struct Modules : Genode::Mmio<28>
		{
			struct Mods_count : Register < 20, 32> { };
			struct Mods_addr  : Register < 24, 32> { };

			Modules(addr_t a = 0) : Mmio({(char *)a, Mmio::SIZE}) { }
		};

		struct Module : Genode::Mmio<16>
		{
			struct Start   : Register < 0, 32> { }; /* spec: Mod_start */
			struct End     : Register < 4, 32> { }; /* spec: Mod_end */
			struct Cmdline : Register < 8, 32> { };

			Module(addr_t a = 0) : Mmio({(char *)a, Mmio::SIZE}) { }

			auto start  () const { return read<Start  >(); }
			auto end    () const { return read<End    >(); }
			auto cmdline() const { return read<Cmdline>(); }
		};

		struct Framebuffer : Genode::Mmio<116>
		{
			struct Addr   : Register < 88, 64> { };
			struct Pitch  : Register < 96, 32> { };
			struct Width  : Register <100, 32> { };
			struct Height : Register <104, 32> { };
			struct Bpp    : Register <108,  8> { };
			struct Type   : Register <109,  8> { };
			/* color info - 6 bytes */

			Framebuffer(addr_t a = 0, addr_t = 0)
			: Mmio({(char *)a, Mmio::SIZE}) { }
		};

		void enable_verbose() { verbose = true; }

		Mbi1(addr_t mbi, addr_t offset) : Mmio({(char *)mbi, Mmio::SIZE})
		{
			image_offset = offset;
		}

		void with_cmdline(auto const &fn)
		{
			if (!read<Flags::Cmdline>())
				return;

			fn(128, /* len of string - arbitrarily chosen */
			   reinterpret_cast<char const * >(read<Cmdline>()));
		}

		void with_fb(auto const &fn, auto const &fn_error)
		{
			Framebuffer fb(base());

			if (!fb.base()) {
				fn_error();
				return;
			}

			fn(fb.read<Framebuffer::Addr>(),
			   fb.read<Framebuffer::Pitch>(),
			   fb.read<Framebuffer::Width>(),
			   fb.read<Framebuffer::Height>(),
			   fb.read<Framebuffer::Bpp>(),
			   fb.read<Framebuffer::Type>());
		}

		void for_each_memory_region(auto const &fn)
		{
			if (!read<Flags::Mem_map>())
				return;

			auto const mmap_start  = read<Mmap_addr  >();
			auto const mmap_length = read<Mmap_length>();

			for (auto mmap = mmap_start; mmap < mmap_start + mmap_length;) {

				Memory region(mmap);

				fn(region.read<Memory::Addr>(),
				   region.read<Memory::Addr>() + region.read<Memory::Length>() - 1,
				   region.read<Memory::Type>());

				mmap += region.read<Memory::Size>() + Memory::Size::ACCESS_WIDTH / 8;
			}
		}

		void dump_all_tags()
		{
			warning(__func__, " not supported");
		}

		void for_each_module(auto const &module_fn)
		{
			Modules modules(base());

			auto mod_cnt  = modules.read<Modules::Mods_count>();
			auto mod_addr = modules.read<Modules::Mods_addr>();

			for (unsigned i = 0; i < mod_cnt; i ++) {

				Module mod(mod_addr);

				mod_addr += Module::SIZE;

				auto const mod_s = mod.start();
				auto const mod_e = mod.end();
				auto const cmdl  = mod.cmdline();

				module_fn(mod_s, mod_e, cmdl, mod);
			}
		}

		bool overlap_with_reserved_memory(uint64_t region_start, uint64_t region_end) override
		{
			bool overlap = false;

			for_each_memory_region([&](auto start, auto end, auto type) {
				if (overlap) return;

				if (type == Memory::Type::MEMORY) return;

				if (region_end   < start) return;
				if (region_start > end)   return;

				log(" overlap detected - ",
				    Hex(region_start), "-", Hex(region_end), " vs ",
				    Hex(start), "-", Hex(end));

				overlap = true;
			});

			return overlap;
		}

		bool overlap_with_modules(uint64_t const region_start,
		                          uint64_t const region_end,
		                          bool     const details,
		                          unsigned const skip_module_id = ~0u) override
		{
			unsigned mod_cnt = 0;
			bool     overlap = false;

			for_each_module([&](auto const mod_s, auto const mod_e, auto const cmdl, auto &) {

				if (overlap)
					return;

				if (mod_cnt++ == skip_module_id)
					return;

				if (cmdl) {
					unsigned const max_len = 128; /* arbitrarily chosen */

					Cstring str((char *)cmdl, max_len);

					bool no_overlap = (region_end   <  cmdl) ||
					                  (region_start >= cmdl + str.length());

					if (!no_overlap && details)
						log(__LINE__, ": ", mod_cnt, " ",
						    Hex(mod_s), "-", Hex(mod_e), " ", Hex(cmdl));

					if (!no_overlap) {
						overlap = true;
						return;
					}
				}

				if (region_end   < mod_s) return;
				if (region_start > mod_e) return;

				if (details)
					log(" overlap with ", mod_cnt, ". module ",
					    "- ", Hex(mod_s), "-", Hex(mod_e));

				overlap = true;
			});

			return overlap;
		}

		void find_memory(Memory_args const  args,
		                 uint64_t    const  size,
		                 auto        const &fn)
		{
			uint64_t region_start, region_end;

			if (!size)
				return;

			auto overlap_round = 0u; /* debug */
			auto next_phys     = args.min_phys;

			do {
				region_start = 0;
				region_end   = 0;

				/* lookup up memory */
				for_each_memory_region([&](auto const start, auto const end,
				                           auto const type) {

					if (region_end)                   return;
					if (type != Memory::Type::MEMORY) return;
					if (end  <  next_phys)            return;
					if (start > args.max_phys)        return;

					/* align according to args request */
					auto s = align_addr(start, { .log2 = args.min_align } );

					if (s < next_phys)
						s = align_addr(next_phys, { .log2 = args.min_align });

					if (end < s)
						return;

					if (s + size > end)
						return;

					if (s + size > args.max_phys)
						return;

					region_start = s;
					region_end   = region_start + size - 1;
				});

				if (!region_end)
					return;

				next_phys = region_start + region_end;

				overlap_round ++;

			} while (overlap(region_start, region_end, false));

			if (region_end)
				fn(region_start, region_end, overlap_round);
		}

		void relocate_module(unsigned const id,
		                     uint64_t const min_physical,
		                     unsigned const mod_cnt)
		{
			bool     overlap = false;
			unsigned mod_id  = 0u;

			for_each_module([&](auto const mod_s, auto const mod_e, auto,
			                    Module &mod) {

				if (overlap)
					return;

				if (mod_id++ != id)
					return;

				if (verbose)
					log("module ", mod_id, "/", mod_cnt, "  ",
					    Hex(mod_s, Hex::PREFIX, Hex::PAD), "-",
					    Hex(mod_e, Hex::PREFIX, Hex::PAD));

				auto const size = mod.end() - mod.start() + 1;

				Memory_args args { .min_phys  = min_physical,
				                   .max_phys  = ~0u,
				                   .min_align = 12 };

				find_memory(args, size, [&](auto s, auto e, auto rounds) {

					auto const start = unsigned(s);
					auto const end   = unsigned(e);

					if (verbose)
						log(" relocated to ",
						    Hex(start, Hex::PREFIX, Hex::PAD), "-",
						    Hex(end  , Hex::PREFIX, Hex::PAD), " ",
						    rounds > 1 ? String<12>(rounds) : String<12>());

					sanitize_memcpy(start,
					                { .start = mod.start(),
					                  .size  = size });

					mod.write<Module::Start>(start);
					mod.write<Module::End  >(end);
				});
			});
		}

		uint64_t highest_elf_physical_addr()
		{
			uint64_t paddr { };

			for_each_module([&](auto, auto, auto, Module const &mod) {
				Elf_binary elf(mod.start());

				Elf_segment seg;

				for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {

					if (paddr < seg.phys_start() + seg.mem_size())
						paddr = align_addr(seg.phys_start() + seg.mem_size(),
						                   { .log2 = 12 });
				}
			});

			return paddr;
		}

		bool prepare_elf(Module const &mod, Elf_binary &elf)
		{
			if (!elf.valid())
				return false;

			Elf_segment seg;

			/* check segments conditions, e.g. overlap, valid memory */
			for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {

				bool valid_mem = false;
				auto seg_start = seg.phys_start();
				auto seg_end   = seg.phys_start() + seg.mem_size() - 1;

				if (seg.flags().skip) {
					warning(" segment ", n, " skipped");
					continue;
				}

				for_each_memory_region([&](auto start, auto end, auto type) {
					if (type != Memory::Type::MEMORY) return;

					if (start <= seg_start && seg_end <= end)
						valid_mem = true;
				});

				if (valid_mem && !overlap(seg_start, seg_end, true))
					continue;

				error("module load error -",
				      " segment n="  , n,
				      " file_size="  , Hex(seg.file_size()),
				      " file_offset=", Hex(seg.file_offset()),
				      " mem_size="   , Hex(seg.mem_size()),
				      " phys_start=" , Hex(seg.phys_start()),
				      " ", !valid_mem ? " outside valid memory" : "");

				return false;
			}

			auto last_segment_end = 0ull;

			for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {

				if (verbose)
					log(" segment n="  , n,
					    " skip="       , seg.flags().skip,
					    " file_size="  , Hex(seg.file_size()),
					    " file_offset=", Hex(seg.file_offset()),
					    " mem_size="   , Hex(seg.mem_size()),
					    " phys_start=" , Hex(seg.phys_start()));

				if (seg.flags().skip) {
					warning(" segment ", n, " skipped");
					continue;
				}

				auto dst = unsigned(seg.phys_start());
				auto src = mod.start() + seg.file_offset();

				sanitize_memcpy(dst, { .start = src, .size = seg.file_size() });

				if (seg.file_size() < seg.mem_size())
					sanitize_memset({ .start = dst + seg.file_size(),
					                  .size  = seg.mem_size() - seg.file_size() });

				if (verbose)
					log("  relocated to ", Hex(dst, Hex::PREFIX, Hex::PAD), "-",
					    Hex(dst + seg.mem_size() - 1 , Hex::PREFIX, Hex::PAD));

				if (dst + seg.mem_size() > last_segment_end)
					last_segment_end = dst + seg.mem_size();
			}

			/* put info about preboot memory behind prepared ELF (for OKL4) */
			auto preboot_info = reinterpret_cast<unsigned *>(last_segment_end);
			*(preboot_info + 0) = preserve_report_mem.size;
			*(preboot_info + 1) = unsigned(preserve_report_mem.start);

			return true;
		}

		void move_and_adjust_range(Range &range, char const * name)
		{
			Memory_args args { .min_phys  = max(highest_elf_physical_addr(),
			                                    phys_img_start() +
			                                    aligned_prog_img_size()),
			                   .max_phys  = ~0u,
			                   .min_align = 12 };

			find_memory(args, align_addr(range.size, { .log2 = 12 }),
			            [&](auto const start, auto end, auto const rounds) {

				log(" move ", name, " range from ", Hex(range.start), " to ",
				    Hex(start, Hex::PREFIX, Hex::PAD), "-",
				    Hex(end, Hex::PREFIX, Hex::PAD), " ",
				    rounds > 1 ? String<12>(rounds) : String<12>());

				sanitize_memcpy(unsigned(start), range);

				range.start = unsigned(start);
			});
		}

		void move_myself_physical()
		{
			auto const phys_img_end   = image_offset + unsigned(&_prog_img_end);
			auto const phys_size      = phys_img_end - phys_img_start();

			Memory_args args { .min_phys  = max(highest_elf_physical_addr(),
			                                    phys_img_start() +
			                                    aligned_prog_img_size()),
			                   .max_phys  = ~0u,
			                   .min_align = LOG2_16MB };

			find_memory(args, align_addr(phys_size, { .log2 = LOG2_4MB }),
			            [&](auto const dst, auto const end, auto const) {
				relocate_myself_physical(unsigned(dst), end - dst + 1);
			});
		}

		void boot_next_module(Range report_mem)
		{
			bool reloc_required { };
			auto mod_cnt = 0u;

			/* check for overlaps and whether relocation is required */
			for_each_module([&](auto, auto, auto, Module const &mod) {

				Elf_binary elf(mod.start());

				if (relocation_myself_required(elf))
					move_myself_physical();

				if (overlap_with_segment(elf, report_mem))
					move_and_adjust_range(report_mem, "PREBOOT HID");

				if (!reloc_required) {
					reloc_required = relocation_required(elf, mod);
					if (verbose && reloc_required)
						log(" relocation required because of ", mod_cnt + 1,
						    ". module ");
				}
				mod_cnt ++;
			});

			/* remember region to be considered in overlap() invocations */
			preserve_report_mem = report_mem;

			if (reloc_required) {
				/*
				 * Relocate modules behind last elf segment of all modules
				 * and keep a bit space, because some kernels expect it.
				 */
				auto const min_physical = highest_elf_physical_addr() +
				                          (1ul << LOG2_4MB);

				for (unsigned i = mod_cnt; i > 0; i--) {
					relocate_module(i - 1, min_physical, mod_cnt);
				}

				reloc_required = false;

				bool first_module = true;

				/* re-check if relocation requirement is fulfilled */
				for_each_module([&](auto, auto, auto, Module const &mod) {
					if (first_module || reloc_required)
						return;

					Elf_binary elf(mod.start());

					reloc_required = relocation_required(elf, mod);

					first_module = false;
				});
			}

			if (reloc_required) {
				error(" unable to boot due to relocation issues");
				return;
			}

			{
				Modules    modules(base());
				Module     mod    (modules.read<Modules::Mods_addr>());
				Elf_binary elf    (mod.start());

				if (verbose)
					log("module 1/", mod_cnt, "    ",
					    Hex(mod.start()  , Hex::PREFIX, Hex::PAD), "-",
					    Hex(mod.end()    , Hex::PREFIX, Hex::PAD), " cmdline ",
					    Hex(mod.cmdline(), Hex::PREFIX, Hex::PAD), " ",
					    " entry ", Hex(elf.entry()));

				if (!elf.valid()) {
					error(" not a valid ELF binary");
					return;
				}

				if (!prepare_elf(mod, elf)) {
					error(" preparation of next ELF module failed");
					return;
				}

				/* adjust module start entry */
				modules.write<Modules::Mods_count>(mod_cnt - 1);
				modules.write<Modules::Mods_addr >(modules.read<Modules::Mods_addr>() + Module::SIZE);

				/* replace command line */
				write<Cmdline>(mod.cmdline());
				write<Flags::Cmdline>(1);

				/* "re-use" drives entry */
				write<Drives_length>(preserve_report_mem.size);
				write<Drives_addr  >(unsigned(preserve_report_mem.start));

				auto const entry = elf.entry();

				if (verbose)
					log(" calling next module ... ", Hex(entry));

				/* prepare ip and sp to switch to protected mode */
				switch_to_boot_ip_and_stack(image_offset);

				/* switch back to protected mode and call next module entry */
				asm volatile ( "mov %1, %%cr0; push %%edx; ret;"
				              :
				              : "a" (Mbi1::MAGIC),
				                "r" (1 /* CR0 PE - protected mode enable */),
				                "d" (entry),
				                "b" (base()));

				/* never reached */
			}
		}
};
