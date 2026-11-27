/*
 * \brief  Common Multiboot 1 & 2 functionality
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


namespace Boloa {

	using namespace Genode;

	struct Mbi;

	extern "C" unsigned _prog_img_beg;
	extern "C" unsigned _prog_img_end;

	extern "C" void switch_to_boot_ip_and_stack(unsigned) __attribute__((regparm(1)));
}


struct Boloa::Mbi
{
	enum { LOG2_4MB = 22, LOG2_16MB = 24 };

	struct Range { uint64_t start; unsigned size; };

	struct Memory_args {
		uint64_t const min_phys;
		uint64_t const max_phys;
		uint8_t  const min_align;
	};

	enum {
		PTE_PRESENT   = 1u << 0,
		PTE_WRITEABLE = 1u << 1,
		PTE_USER      = 1u << 2,
		PTE_SUPERPAGE = 1u << 7,

		PTE_ATTRIBUTES = PTE_PRESENT | PTE_WRITEABLE | PTE_USER
		               | PTE_SUPERPAGE
	};

	bool   verbose      { };
	addr_t image_offset { };

	Range  preserve_report_mem { };

	unsigned phys_img_start() { return image_offset + unsigned(&_prog_img_beg); };

	unsigned aligned_prog_img_size() const
	{
		return align_addr(unsigned(&_prog_img_end), { .log2 = LOG2_4MB }) -
		       unsigned(&_prog_img_beg);
	}

	unsigned page_table() const
	{
		auto phys_pagetable = 0u;
		asm volatile ("mov %%cr3,%0": "=r"(phys_pagetable):: "memory");
		return phys_pagetable;
	}

	void page_table(unsigned phys_pagetable)
	{
		/* reload pagetable */
		asm volatile ("mov %0, %%cr3" : : "r" (phys_pagetable) : "memory");
	}

	inline void clflush(volatile void *addr)
	{
		asm volatile("clflush %0" : "+m" (*(volatile char *)addr) :: "memory");
	}

	bool intersect(Range const &r1, Range const &r2) const
	{
		return !((r1.start + r1.size <= r2.start) ||
		         (r1.start           >= r2.start + r2.size));
	}

	bool overlap_with_segment(Elf_binary &elf, Range const &range)
	{
		Elf_segment seg;

		for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {

			if (seg.flags().skip)
				continue;

			Range rs { .start = unsigned(seg.phys_start()),
			           .size  = unsigned(seg.mem_size()) };

			if (intersect(rs, range))
				return true;
		}

		return false;
	}

	/*
	 * Make a non accessible 4MB region temporarily available
	 */
	void with_sanitized_region(Range const &region, bool const src, auto const &fn)
	{
		auto const phys_pagetable = page_table();
		auto const img_virt_start = unsigned(&_prog_img_beg);
		auto const img_phys_start = image_offset + img_virt_start;

		auto  const offset  = unsigned(region.start & ((1u << LOG2_4MB) - 1));
		Range const reg_tmp = { .start = img_phys_start + offset,
		                        .size  = min(region.size, (1u << LOG2_4MB) - offset) };

		auto const entry_index = img_phys_start >> LOG2_4MB;
		unsigned * entries = reinterpret_cast<unsigned *>(phys_pagetable - image_offset);

		entries[entry_index] = PTE_ATTRIBUTES
		                     | (src ? (addr_t(region.start) - offset)
		                            : img_virt_start);
		clflush(entries + entry_index);

		page_table(phys_pagetable); /* reload pagetable */

		fn(reg_tmp);

		entries[entry_index] = img_phys_start | PTE_ATTRIBUTES;
		clflush(entries + entry_index);

		page_table(phys_pagetable); /* reload pagetable */
	}

	/*
	 * Make non accessible 4MB destination region temporarily available
	 */
	void with_sanitized_region_dst(Range const &dst, auto const &fn) {
		with_sanitized_region(dst, false, fn); }

	/*
	 * Make non accessible 4MB source region temporarily available
	 */
	void with_sanitized_region_src(Range const &src, auto const &fn) {
		with_sanitized_region(src, true, fn); }

	void sanitize_memset(Range const &dst)
	{
		auto const prog_img_start = unsigned(&_prog_img_beg);

		Range const image { .start = prog_img_start,
		                    .size  = aligned_prog_img_size() };

		bool const overlap_dst = intersect(dst, image);

		if (verbose && overlap_dst)
			log("  overlap during memset detected ... sanitizing ... ",
			    overlap_dst ? "destination " : "",
			    Hex_range(image.start, image.size), " ",
			    Hex_range(dst.start, dst.size));

		if (!overlap_dst) {
			__builtin_memset((void *)(dst.start), 0, dst.size);
			return;
		}

		auto partial_size = 0u;

		with_sanitized_region_dst(dst, [&](Range const tmp_dst) {
			__builtin_memset((void *)(tmp_dst.start), 0, tmp_dst.size);

			partial_size = tmp_dst.size;
		});

		if (dst.size > partial_size)
			__builtin_memset((void *)(dst.start + partial_size), 0,
			                 dst.size - partial_size);
	}

	void sanitize_memcpy(unsigned const dst_start, Range const &src)
	{
		auto const prog_img_start = unsigned(&_prog_img_beg);

		Range const dst   { .start = dst_start, .size = src.size };
		Range const image { .start = prog_img_start,
		                    .size  = aligned_prog_img_size() };

		bool const overlap_dst = intersect(dst, image);
		bool const overlap_src = intersect(src, image);

		bool const ok = !overlap_src && !overlap_dst;

		if (verbose && !ok)
			log("  overlap during memcpy detected ... sanitizing ... ",
			    overlap_dst ? "destination " : "",
			    overlap_src ? "source " : "",
			    Hex_range(image.start, image.size), " ",
			    Hex(src.start), "+", Hex(src.size), " -> ",
			    Hex(dst.start), "+", Hex(dst.size),
			    " image_offset=", Hex(image_offset));

		if (overlap_src && overlap_dst) {
			warning("  overlap during memcpy detected ... boot will fail");
			return;
		}

		if (!overlap_dst && !overlap_src) {
			__builtin_memcpy(reinterpret_cast<void *>(dst.start),
			                 reinterpret_cast<void *>(src.start),
			                 src.size);
			return;
		}

		auto copied_size = 0u;

		if (overlap_src) {
			if (src.start < image.start) {
				copied_size += unsigned(image.start - src.start);
				__builtin_memcpy(reinterpret_cast<void *>(dst.start),
				                 reinterpret_cast<void *>(src.start),
				                 copied_size);
			}

			with_sanitized_region_src({ .start = src.start + copied_size,
			                            .size  = src.size  - copied_size },
				[&](Range const tmp_src) {
					__builtin_memcpy(reinterpret_cast<void *>(dst.start + copied_size),
					                 reinterpret_cast<void *>(tmp_src.start),
					                 tmp_src.size);

					copied_size += tmp_src.size;
				});
		} else
		if (overlap_dst) {

			if (dst.start < image.start) {
				copied_size += unsigned(image.start - dst.start);
				__builtin_memcpy(reinterpret_cast<void *>(dst.start),
				                 reinterpret_cast<void *>(src.start),
				                 copied_size);
			}

			with_sanitized_region_dst({ .start = dst.start + copied_size,
			                            .size  = dst.size  - copied_size },
				[&](Range const tmp_dst) {
					__builtin_memcpy(reinterpret_cast<void *>(tmp_dst.start),
					                 reinterpret_cast<void *>(src.start + copied_size),
					                 tmp_dst.size);

					copied_size += tmp_dst.size;
				});
		}

		if (src.size > copied_size)
			__builtin_memcpy(reinterpret_cast<void *>(dst.start + copied_size),
			                 reinterpret_cast<void *>(src.start + copied_size),
			                 src.size - copied_size);
	}

	bool relocation_myself_required(Elf_binary &elf)
	{
		auto const phys_img_end = phys_img_start() + aligned_prog_img_size() - 1;

		Elf_segment seg;

		for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {

			if (seg.flags().skip) {
				warning(" segment ", n, " skipped");
				continue;
			}

			auto seg_start = seg.phys_start();
			auto seg_end   = seg.phys_start() + seg.mem_size() - 1;

			if (seg_end < phys_img_start()) continue;
			if (seg_start > phys_img_end) continue;

			log(" bootloader overlap with module detected");

			if (verbose)
				log("  ", Hex(phys_img_start()), "-", Hex(phys_img_end),
				    " vs ", Hex(seg_start), "-", Hex(seg_end));

			return true;
		}

		return false;
	}

	void relocate_myself_physical(unsigned const dst, auto const dst_size)
	{
		auto const previous_offset = image_offset;
		auto const src = unsigned(&_prog_img_beg);

		log(" move bootloader from ", Hex(src + image_offset), " to ",
		    Hex(dst, Hex::PREFIX, Hex::PAD), "-",
		    Hex(dst + dst_size, Hex::PREFIX, Hex::PAD));

		auto const prog_index_start = src >> LOG2_4MB;
		auto const prog_index_last  = (src + aligned_prog_img_size()) >> LOG2_4MB;

		/* make copy */
		__builtin_memcpy(reinterpret_cast<void *>(dst),
		                 reinterpret_cast<void *>(src), addr_t(dst_size));

		/* set program page table entry to new physical region */
		auto phys_pagetable = page_table();
		auto entries = reinterpret_cast<unsigned *>(phys_pagetable - image_offset);

		for (auto i = prog_index_start; i < prog_index_last; i++) {
			entries[i] = (dst + ((i - prog_index_start) << LOG2_4MB))
			           | PTE_ATTRIBUTES;
			clflush(entries + i);
		}

		/* calculate new physical page table address */
		phys_pagetable &= (1u << LOG2_16MB) - 1;
		phys_pagetable += dst;

		/* adjust relocation offset */
		image_offset = dst - unsigned(&_prog_img_beg);

		/* copy region, inclusive in-use stack, and reload pagetable */
		asm volatile( "cld;"           /* DF==0 -> increment upwards */
		              "rep movsb;"     /* copy data */
		              "mfence;"
		              "mov %3, %%cr3;" /* reload pagetable */
		             :
		             : "D" (dst), "S" (src), "c" (unsigned(dst_size)),
		               "r" (phys_pagetable)
		             : "memory", "cc");

		/* revert to 1:1 mapping if it wasn't due to offset */
		if (previous_offset) {
			auto phys_pagetable = page_table();
			auto entries = reinterpret_cast<unsigned *>(phys_pagetable - image_offset);

			auto const prog_index_start = src >> LOG2_4MB;
			auto const prog_index_last  = (src + aligned_prog_img_size()) >> LOG2_4MB;

			for (auto i = prog_index_start; i < prog_index_last; i++) {
				entries[i] = (src + ((i - prog_index_start) << LOG2_4MB))
				           | PTE_ATTRIBUTES;
				clflush(entries + i);
			}

			asm volatile( "mov %0, %%cr3;" /* reload pagetable */
			             : : "r" (phys_pagetable) : "memory");
		}
	}

	/**
	 * Check for overlap of a region with known fixed regions, e.g.
	 *  boot loader, modules, reserved memory, MBI data, GDT table,
	 *  HID data, ...
	 */
	bool overlap(uint64_t region_start, uint64_t region_end,
	             bool details, unsigned skip_module_id = ~0u)
	{
		Range region { .start = region_start,
		               .size  = unsigned(region_end - region_start + 1) };

		if (region_start > region_end) {
			if (details) {
				log("overlap detected");
				log(" region : ", Hex(region_start), "-", Hex(region_end));
			}
			return true;
		}

		if (overlap_with_reserved_memory(region_start, region_end)) {
			if (details) {
				log("overlap with reserved memory detected");
				log(" region : ", Hex(region_start), "-", Hex(region_end));
			}
			return true;
		}

		if (overlap_with_modules(region_start, region_end, details, skip_module_id)) {
			if (details) {
				log("overlap with other module");
				log(" region : ", Hex(region_start), "-", Hex(region_end));
			}
			return true;
		}

		uint32_t gdt { };
		uint32_t cs, ds, es, fs, gs, ss, max_seg { };

		asm volatile ("sgdt %0": "=m"(gdt));

		asm volatile ("mov %%cs, %0" : "=r"(cs));
		if (cs > max_seg) max_seg = ds;

		asm volatile ("mov %%ds, %0" : "=r"(ds));
		if (ds > max_seg) max_seg = ds;

		asm volatile ("mov %%es, %0" : "=r"(es));
		if (es > max_seg) max_seg = es;

		asm volatile ("mov %%fs, %0" : "=r"(fs));
		if (fs > max_seg) max_seg = fs;

		asm volatile ("mov %%gs, %0" : "=r"(gs));
		if (gs > max_seg) max_seg = gs;

		asm volatile ("mov %%ss, %0" : "=r"(ss));
		if (ss > max_seg) max_seg = ss;

		/* estimate gdt table size based on highest segment setup */
		auto const gdt_base = gdt & ~0xffful;
		auto const gdt_end  = align_addr(gdt + max_seg * 8, /* desc size */
		                                 { .log2 = 12 });

		Range const gdt_data { .start = gdt_base,
		                       .size  = gdt_end - gdt_base };
		Range const mbi_data { .start = mbi_base(),
		                       .size  = mbi_size() };
		Range const phys_img { .start = phys_img_start(),
		                       .size  = aligned_prog_img_size() };

		bool const overlap_gdt      = intersect(gdt_data, region);
		bool const overlap_mbi      = intersect(region, mbi_data);
		bool const overlap_phys_img = intersect(region, phys_img);
		bool const overlap_report   = intersect(region, preserve_report_mem);

		bool const ok = !overlap_mbi && !overlap_phys_img &&
		                !overlap_gdt && !overlap_report;

		if (details && !ok) {
			log("overlap detected of region with",
			    overlap_mbi      ? " MBI" : "",
			    overlap_gdt      ? " GDT" : "",
			    overlap_report   ? " HID" : "",
			    overlap_phys_img ? " boot loader" : "");
			log(" region     : ", Hex_range(region  .start, region  .size));
			log(" boot loader: ", Hex_range(phys_img.start, phys_img.size));
			log(" mbi        : ", Hex_range(mbi_data.start, mbi_data.size));
			log(" gdt        : ", Hex_range(gdt_data.start, gdt_data.size));
			log(" HID report : ", Hex_range(preserve_report_mem.start,
			                                preserve_report_mem.size));
		}

		return !ok;
	}

	bool relocation_required(Elf_binary &elf, auto const &mod)
	{
		Elf_segment seg;

		auto const mod_start = mod.start();
		auto const mod_end   = mod.end();

		if (overlap(mod_start, mod_end, false, 0 /* skip first module */))
			return true;

		for (unsigned n = 0; (seg = elf.get_segment(n)).valid(); ++n) {

			if (seg.flags().skip) {
				warning(" segment ", n, " skipped");
				continue;
			}

			if (overlap(seg.phys_start(), seg.phys_start() + seg.mem_size() - 1, false))
				return true;
		}

		return false;
	}

	virtual ~Mbi() { }

	virtual bool overlap_with_reserved_memory(uint64_t region_start,
	                                          uint64_t region_end) = 0;

	virtual bool overlap_with_modules(uint64_t region_start,
	                                  uint64_t region_end,
	                                  bool     details,
	                                  unsigned skip_module_id = ~0u) = 0;

	virtual addr_t mbi_size() const = 0;
	virtual addr_t mbi_base() const = 0;
};
