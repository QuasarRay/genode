/*
 * \brief  Multiboot 2 handling
 * \author Alexander Boettcher
 * \date   2026-06-17
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

	class Mbi2;

}


class Boloa::Mbi2 : Mmio<0x8>, Boloa::Mbi
{
	public:

		enum { MAGIC = 0x36d76289ul, BINARY_MAGIC = 0xe85250d6ul };

		struct Size : Register <0x0, 32> { };

		addr_t mbi_size() const override { return read<Size>(); }
		addr_t mbi_base() const override { return base(); }

		struct Tag : Genode::Mmio <8>
		{
			enum { LOG2_SIZE = 3 };

			struct Type : Register <0x00, 32> {
				enum Id {
					END         = 0,
					CMDLINE     = 1,
					MODULE      = 3,
					MEMORY      = 6,
					FRAMEBUFFER = 8,
					GENODE_HID  = 0xaffe,
				};
			};

			struct Size : Register <0x04, 32> { };

			Tag(addr_t addr) : Mmio({(char *)addr, Mmio::SIZE}) { }
		};

		struct Tag_binary : Genode::Mmio <16>
		{
			struct Type  : Register <0x00, 16> { };
			struct Flags : Register <0x02, 16> { };
			struct Size  : Register <0x04, 32> { };

			Tag_binary(addr_t addr) : Mmio({(char *)addr, Mmio::SIZE}) { }
		};

		struct Module: Genode::Mmio <0x10>
		{
			struct Type  : Register <0x00, 32> { };
			struct Size  : Register <0x04, 32> { };
			struct Start : Register <0x08, 32> { };
			struct End   : Register <0x0c, 32> { };

			Module(addr_t addr): Mmio({(char *)addr, Mmio::SIZE}) { }

			void with_cmdline(auto const &fn) const {
				fn(base() + Mmio::SIZE, read<Size>() - Mmio::SIZE); }

			auto start  () const { return read<Start  >(); }
			auto end    () const { return read<End    >(); }
		};

		struct Framebuffer : Genode::Mmio <30>
		{
			struct Tag    : Register <  0, 32> { };
			struct Addr   : Register <  8, 64> { };
			struct Pitch  : Register < 16, 32> { };
			struct Width  : Register < 20, 32> { };
			struct Height : Register < 24, 32> { };
			struct Bpp    : Register < 28,  8> { };
			struct Type   : Register < 29,  8> { };
			/* color info - 6 bytes */

			addr_t mmio_addr(addr_t base, addr_t offset)
			{
				Mbi2 mbi(base, offset);

				addr_t addr = 0;

				mbi.for_each_tag([&](Mbi2::Tag &tag) {
					if (tag.read<Mbi2::Tag::Type>() != Mbi2::Tag::Type::FRAMEBUFFER)
						return;

					addr = tag.base();
				});

				return addr;
			}

			Framebuffer(addr_t a = 0, addr_t o = 0)
			: Mmio({(char *)mmio_addr(a, o), Mmio::SIZE}) { }
		};

		struct Memory : Genode::Mmio<0x18>
		{
			struct Addr : Register <0x00, 64> { };
			struct Size : Register <0x08, 64> { };
			struct Type : Register <0x10, 32> { enum { MEMORY = 1 }; };

			Memory(addr_t mmap = 0) : Mmio({(char *)mmap, Mmio::SIZE}) { }
		};

		void enable_verbose() { verbose = true; }

		Mbi2(addr_t mbi, addr_t offset) : Mmio({(char *)mbi, Mmio::SIZE})
		{
			image_offset = offset;
		}

		void with_cmdline(auto const &fn)
		{
			for_each_tag([&](Tag &tag) {
				if (tag.read<Tag::Type>() != Tag::Type::CMDLINE)
					return;

				fn(tag.read<Tag::Size>() - Tag::SIZE,
				   reinterpret_cast<char const *>(tag.base() + Tag::SIZE));
			});
		}

		void with_cmdline_tag(auto const &fn)
		{
			for_each_tag([&](Tag &tag) {
				if (tag.read<Tag::Type>() != Tag::Type::CMDLINE)
					return;

				fn(tag);
			});
		}

		void with_fb(auto const &fn, auto const &fn_error)
		{
			Framebuffer fb(base(), image_offset);

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

		void for_each_tag(auto const &fn)
		{
			addr_t const size = read<Mbi2::Size>();

			for (auto tag_addr = base() + Tag::SIZE; tag_addr < base() + size;)
			{
				Tag tag(tag_addr);

				if (tag.read<Tag::Type>() == Tag::Type::END)
					return;

				fn(tag);

				tag_addr += align_addr(tag.read<Tag::Size>(),
				                       { .log2 = Tag::LOG2_SIZE });
			}
		}

		void for_each_memory_region(auto const &fn)
		{
			for_each_tag([&](Tag &tag) {
				if (tag.read<Tag::Type>() != Tag::Type::MEMORY)
					return;

				addr_t const mem_end = tag.base() + tag.read<Tag::Size>();
				addr_t       mem_mod = tag.base() + Tag::SIZE + 8;

				for (; mem_mod < mem_end; mem_mod += Memory::SIZE) {
					Memory mem(mem_mod);

					fn(mem.read<Memory::Addr>(),
					   mem.read<Memory::Addr>() + mem.read<Memory::Size>() - 1,
					   mem.read<Memory::Type>());
				}
			});
		}

		void dump_all_tags()
		{
			for_each_tag([&](Tag &tag) {
				auto const type = tag.read<Tag::Type>();

				log("tag ", type);

				if (type == Tag::Type::MODULE) {

					Module mod(tag.base());

					log(" module : ", Hex(mod.start()),
					    " - ", Hex(mod.end()));

				} else
				if (type == Tag::Type::MEMORY) {

					addr_t const mem_end = tag.base() + tag.read<Tag::Size>();
					addr_t       mem_mod = tag.base() + Tag::SIZE + 8;

					for (; mem_mod < mem_end; mem_mod += Memory::SIZE) {

						Memory mem(mem_mod);

						log(" memory: ", Hex(mem.read<Memory::Addr>()),
						    " + ",  Hex(mem.read<Memory::Size>()), " - ",
						    " type ", mem.read<Memory::Type>());
					}
				}
			});
		}

		void for_each_module(auto const &module_fn)
		{
			for_each_tag([&](Tag &tag) {
				if (tag.read<Tag::Type>() != Tag::Type::MODULE)
					return;

				Module mod(tag.base());

				module_fn(mod.start(),
				          mod.end(),
				          mod.read<Module::Size>() - Module::SIZE, mod);
			});
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

			for_each_module([&](auto const mod_s, auto const mod_e, auto, auto &) {

				if (overlap)
					return;

				if (mod_cnt++ == skip_module_id)
					return;

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

				if (verbose) {
					mod.with_cmdline([&](auto cmdline, auto /* size */) {
						log("module ", mod_id, "/", mod_cnt, "  ",
						    Hex(mod_s, Hex::PREFIX, Hex::PAD), "-",
						    Hex(mod_e, Hex::PREFIX, Hex::PAD),
						    " - '", (char const *)cmdline, "'");
					});
				}

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
			}

			return true;
		}

		struct Result { bool valid; unsigned boot_entry; };

		Result determine_boot_entry(Module const &mod, Elf_binary &elf) const
		{
			Result result { };

			auto const mod_s = mod.start();
			auto const mod_e = mod.end();
			auto const max   = (32768 < mod_e - mod_s + 1) /* see MBI2 spec */
			                 ?  32768 : mod_e - mod_s + 1;

			if (mod_e <= mod_s)
				return result;

			for (auto i = 0u; i < max; i += 4) {
				auto const hdr = *reinterpret_cast<unsigned *>(mod_s + i);
				if (hdr != BINARY_MAGIC)
					continue;

				auto tag    = mod_s + i + 16;
				auto chksum = hdr;

				for (auto j = i + 4; (j < i + 4 * 4) && (j < max); j += 4)
					chksum += *reinterpret_cast<unsigned *>(mod_s + j);

				if (chksum) {
					error ("MBI2 checksum mismatch");
					return result;
				}

				/* use elf boot entry by default */
				result = { .valid = true, .boot_entry = elf.entry() };

				/* boot entry may be overwritten by MBI 2 info, see spec */
				while (mod_s <= tag && tag + Tag_binary::SIZE - 1 <= mod_e) {

					Tag_binary const opt(tag);

					auto const type = opt.read<Tag_binary::Type>();
					auto const size = opt.read<Tag_binary::Size>();

					if (type == 0) /* end tag */
						break;

					switch (type) {
					case 1: /* multiboot2 info request */
						break; /* we ever report the multiboot2 info */
					case 3: /* boot entry */
						result.boot_entry = *(unsigned *)(tag + 8);
						log(" boot entry overwrite: ", Hex(result.boot_entry));
						break;
					default:
						warning(" unsupported MBI type:", type, " size:", size);
						break;
					}
					tag += align_addr(size, { .log2 = 3 });
				}
			}

			return result;
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

		unsigned prepare_mbi2_info()
		{
			Range mbi2 { .start = base(), .size = read<Size>() };

			bool relocate_mbi2_info = false;

			/* detect case that MBI2 data overlaps with one of the static ELFs */
			for_each_module([&](auto, auto, auto, Module const &mod) {
				Elf_binary elf(mod.start());

				if (!elf.valid())
					return;

				Elf_segment seg { };

				for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {

					if (verbose)
						log(" segment n="  , n,
						    " skip="       , seg.flags().skip,
						    " file_size="  , Hex(seg.file_size()),
						    " file_offset=", Hex(seg.file_offset()),
						    " mem_size="   , Hex(seg.mem_size()),
						    " phys_start=" , Hex(seg.phys_start()));

					if (seg.flags().skip) {
						if (verbose)
							warning(" segment ", n, " skipped");
						continue;
					}

					if (!intersect(mbi2, { .start = seg.phys_start(),
					                       .size  = seg.mem_size  () }))
						continue;

					relocate_mbi2_info = true;

					if (verbose)
						log("MBI2 data overlap with module detected");

					break;
				}
			});

			if (!relocate_mbi2_info)
				return unsigned(mbi2.start);

			Memory_args args { .min_phys  = max(highest_elf_physical_addr(),
			                                    phys_img_start() +
			                                    aligned_prog_img_size()),
			                   .max_phys  = ~0u,
			                   .min_align = 12 };

			find_memory(args, mbi2.size, [&](auto const start, auto const end,
			                                 auto const rounds) {

				if (verbose)
					log(" move MBI2 data from ", Hex(mbi2.start), " to ",
					    Hex(start, Hex::PREFIX, Hex::PAD), "-",
					    Hex(end  , Hex::PREFIX, Hex::PAD), " ",
					    rounds > 1 ? String<12>(rounds) : String<12>());

				sanitize_memcpy(unsigned(start), mbi2);

				mbi2.start = start;
			});

			return unsigned(mbi2.start);
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

			bool first_module = true;

			for_each_module([&](auto, auto, auto, Module &mod) {

				if (!first_module)
					return;

				first_module = false;

				Elf_binary elf(mod.start());

				if (verbose) {
					mod.with_cmdline([&](auto cmdline, auto /* size */) {
						log("module 1/", mod_cnt, "    ",
						    Hex(mod.start(), Hex::PREFIX, Hex::PAD), "-",
						    Hex(mod.end()  , Hex::PREFIX, Hex::PAD),
						    " entry ", Hex(elf.entry()),
						    " - '", (char const *)cmdline, "'");
					});
				}

				if (!elf.valid()) {
					error(" not a valid ELF binary");
					return;
				}

				auto boot_entry = determine_boot_entry(mod, elf);
				if (!boot_entry.valid) {
					error(" not a valid MBI2 binary");
					return;
				}

				auto const mbi2_info = prepare_mbi2_info();

				if (mbi2_info != base()) {
					/* restart, since *this* has to use the new mbi2_info */
					Mbi2 mbi2(mbi2_info, image_offset);
					mbi2.boot_next_module(preserve_report_mem);
					error(" MBI2 data relocation failure");
					return;
				}

				if (!prepare_elf(mod, elf)) {
					error(" preparation of next ELF module failed");
					return;
				}

				bool ok { };

				/* rewrite tag containing old command line with HID tag */
				with_cmdline_tag([&](Tag &tag) {
					auto size = align_addr(tag.base() + tag.read<Tag::Size>(),
					                       { .log2 = 3 }) - tag.base();

					ok = size >= Module::SIZE;

					if (!ok)
						return;

					Module m(tag.base());

					m.write<Module::Type >(Tag::Type::GENODE_HID);
					m.write<Module::Start>(unsigned(preserve_report_mem.start));
					m.write<Module::End  >(unsigned(preserve_report_mem.start +
					                                preserve_report_mem.size - 1));
				});

				if (!ok) {
					error(" adding HID report failed");
					return;
				}

				auto const mod_size = mod.read<Module::Size>();

				mod.write<Module::Type>(0xbad); /* invalidate module entry */
				mod.write<Module::Size>(Tag::SIZE);

				/* enable cmdline in-place of mod, just need re-write as tag */
				Tag tag(mod.base() + Module::Type::ACCESS_WIDTH / 8
				                   + Module::Size::ACCESS_WIDTH / 8);
				tag.write<Tag::Type>(Tag::Type::CMDLINE);
				tag.write<Tag::Size>(mod_size - Tag::SIZE);

				if (verbose)
					log(" calling next module ... ", Hex(boot_entry.boot_entry));

				/* prepare ip and sp to switch to protected mode */
				switch_to_boot_ip_and_stack(image_offset);

				/* switch back to protected mode and call next module entry */
				asm volatile ( "mov %1, %%cr0; push %%edx; ret;"
				              :
				              : "a" (Mbi2::MAGIC),
				                "r" (1 /* CR0 PE - protected mode enable */),
				                "d" (boot_entry.boot_entry),
				                "b" (mbi2_info));

				/* never reached */
			});
		}
};
