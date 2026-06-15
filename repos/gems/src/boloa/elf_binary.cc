/**
 * \brief  ELF binary utility
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \date   2006-05-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* base-internal includes */
#include <base/internal/elf_format.h>
#include <base/internal/elf.h>

using namespace Genode;


int Elf_binary::_ehdr_check_compat()
{
	auto const &ehdr = *reinterpret_cast<Elf_Ehdr *>(_start);

	if (memcmp(&ehdr, ELFMAG, SELFMAG) != 0) {
		error("binary is not an ELF");
		return -1;
	}

	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32 &&
	    ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
		error("support for 32/64-bit objects only");
		return -1;
	}

	if (ehdr.e_type != ET_EXEC) {
		error("program is no executable");
		return -1;
	}

	return 0;
}


int Elf_binary::_ph_table_check_compat() { return 0; }


static bool elf32(auto const start)
{
	auto const &ehdr = *reinterpret_cast<Elf_Ehdr *>(start);
	return ehdr.e_ident[EI_CLASS] == ELFCLASS32;
}


Elf_segment Elf_binary::get_segment(unsigned num)
{
	if (!valid()) return Elf_segment();

	if (!(num < _phnum)) return Elf_segment();

	auto phdr_typed = [&](auto const &phdr)
	{
		auto start  = (void *)phdr.p_vaddr;
		auto offset = unsigned(phdr.p_offset);
		auto filesz = unsigned(phdr.p_filesz);
		auto memsz  = unsigned(phdr.p_memsz);

		Elf_binary::Flags flags;

		flags.r = (phdr.p_flags & PF_R) ? true : false;
		flags.w = (phdr.p_flags & PF_W) ? true : false;
		flags.x = (phdr.p_flags & PF_X) ? true : false;

		/*
		 * Skip loading of ELF segments that are not PT_LOAD or have no memory
		 * size.
		 */
		if (phdr.p_type != PT_LOAD || !memsz)
			flags.skip = true;

		return Elf_segment(this, start, offset, filesz, memsz, flags,
		                   phdr.p_paddr);
	};

	if (elf32(_start)) {
		auto const &phdr32 = ((Elf32_Phdr *)_ph_table)[num];
		return phdr_typed(phdr32);
	} else {
		auto const &phdr64 = ((Elf64_Phdr *)_ph_table)[num];
		return phdr_typed(phdr64);
	}
}


Elf_binary::Elf_binary(addr_t start)
: _valid(false), _dynamic(false), _interp(false), _start(start)
{
	/* check for unsupported ELF features */
	if (_ehdr_check_compat()) return;

	auto const &ehdr32 = *reinterpret_cast<Elf32_Ehdr *>(start);
	auto const &ehdr64 = *reinterpret_cast<Elf64_Ehdr *>(start);

	auto const entry = elf32(start) ? ehdr32.e_entry : ehdr64.e_entry;
	auto const phoff = elf32(start) ? ehdr32.e_phoff : ehdr64.e_phoff;
	auto const phnum = elf32(start) ? ehdr32.e_phnum : ehdr64.e_phnum;
	auto const phent = elf32(start) ? ehdr32.e_phentsize : ehdr64.e_phentsize;

	/* entries above 32bit are not supported */
	if (entry > ~0u) return;
	if (phoff > ~0u) return;
	if (_start + phoff > ~0u) return;

	/* program entry point */
	if (!(_entry = unsigned(entry))) return;

	/* segment tables */
	_ph_table  = unsigned(_start + phoff);
	_phentsize = phent;
	_phnum     = phnum;

	/* program segments */
	if (_ph_table_check_compat()) return;

	/* ready to rock */
	_valid = true;
}
