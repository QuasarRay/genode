/*
 * \brief  C++ runtime support and miscellaneous
 * \author Alexander Boettcher
 * \date   2026-06-03
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <base/lock.h>
#include <base/log.h>
#include <base/error.h>
#include <base/sleep.h>

/* XXX */
unsigned long __gxx_personality_v0;


Genode::Lock::Lock(Genode::Lock::State) : _owner(nullptr) { }


using __guard = int;


extern "C" int  __cxa_guard_acquire(__guard * guard) { return *guard ? 0 : 1; }
extern "C" void __cxa_guard_release(__guard * guard) { *guard = 1; }
extern "C" void __cxa_guard_abort  (__guard *) { asm volatile ("ud2"); }
extern "C" int  __cxa_atexit(void(*)(void*), void *, void *) { return -1; }

void operator delete (void *) { return; }
void operator delete (void *, unsigned long) { return; }


void Genode::sleep_forever()
{
	while (true) {
		asm volatile("hlt");
	}
}


void Genode::raise(Genode::Unexpected_error)
{
	Genode::error(__func__, " unexpected error");
	Genode::sleep_forever();
}
