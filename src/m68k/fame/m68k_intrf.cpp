#include <string.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "debug_uae4all.h"
#include "config.h"
#include "uae.h"
#include "options.h"
#include "memory-uae.h"
#include "custom.h"
#include "autoconf.h"
#include "ersatz.h"
#include "savestate.h"

#include "m68k/debug_m68k.h"

static unsigned short mimemoriadummy[65536/2];

void clear_fame_mem_dummy(void)
{
	memset((void *)&mimemoriadummy[0],0,65536);
}

static hostptr micontexto_fpa[256];


void process_exception(unsigned int vect);


#ifdef __3DS__
/* Diagnostics must never crash the host: get_long() goes through the address bank handlers, and e.g. kickmem_lget()
 * does not mask its argument, so reading address 4 while the ROM overlay is mapped at 0 (first frames after reset)
 * dereferences chipmem-style garbage. FAME's own map (m68k_fetch) masks to 24 bits and is valid for every page. */
static uae_u32 cd_long (uae_u32 a)
{
	a &= 0xFFFFFE;
	return ((uae_u32) m68k_fetch (a) << 16) | ((uae_u32) m68k_fetch (a + 2) & 0xFFFF);
}
static uae_u32 cd_word (uae_u32 a)
{
	return (uae_u32) m68k_fetch (a & 0xFFFFFE) & 0xFFFF;
}
/* ExecBase lives in chip RAM or in slow/fast RAM, never in the custom chip or CIA area */
static int cd_exec_ok (uae_u32 eb)
{
	return eb >= 0x400 && !(eb & 1) && (eb < 0xA00000 || (eb >= 0xC00000 && eb < 0xD80000));
}
#endif

#ifdef __3DS__
extern void autoconf_dump_traphist (void);
extern void hardfile_dump_history (void);

/* Registers, stack, exception vectors and recent native traps: enough to see who jumped where */
static void dump_cpu_context (unsigned opcode, unsigned pc)
{
	unsigned i, sp = _68k_areg(7);
	write_log ("crash: PC=%08x opcode=%04x SR=%04x USP=%08x ISP=%08x\n", pc, opcode, _68k_sreg, _68k_uspreg, _68k_ispreg);
	write_log ("crash: D %08x %08x %08x %08x %08x %08x %08x %08x\n", _68k_dreg(0), _68k_dreg(1), _68k_dreg(2), _68k_dreg(3), _68k_dreg(4), _68k_dreg(5), _68k_dreg(6), _68k_dreg(7));
	write_log ("crash: A %08x %08x %08x %08x %08x %08x %08x %08x\n", _68k_areg(0), _68k_areg(1), _68k_areg(2), _68k_areg(3), _68k_areg(4), _68k_areg(5), _68k_areg(6), _68k_areg(7));
	for (i = 0; i < 24; i += 6)
		write_log ("crash: stack %08x: %08x %08x %08x %08x %08x %08x\n", sp + i * 4,
			   cd_long (sp + i * 4), cd_long (sp + i * 4 + 4), cd_long (sp + i * 4 + 8),
			   cd_long (sp + i * 4 + 12), cd_long (sp + i * 4 + 16), cd_long (sp + i * 4 + 20));
	write_log ("crash: vectors 2..7: %08x %08x %08x %08x %08x %08x  ExecBase=%08x\n",
		   cd_long (8), cd_long (12), cd_long (16), cd_long (20), cd_long (24), cd_long (28), cd_long (4));
	write_log ("crash: code at PC: %04x %04x %04x %04x\n", m68k_fetch (pc), m68k_fetch (pc + 2), m68k_fetch (pc + 4), m68k_fetch (pc + 6));
	autoconf_dump_traphist ();
	hardfile_dump_history ();
}
#endif

#ifdef __3DS__
extern unsigned hardfile_request_count (void);

/* Free memory according to exec's MemList (ExecBase+322), in KB. Returns 0 if ExecBase is not (yet) sane. */
int crashdiag_mem (unsigned *chip_kb, unsigned *other_kb)
{
	uae_u32 eb = cd_long (4), node;
	unsigned n = 0;
	*chip_kb = *other_kb = 0;
	if (!cd_exec_ok (eb)) return 0;
	node = cd_long (eb + 322);
	while (n++ < 8 && node >= 0x400 && node < 0x1000000 && !(node & 1) && cd_long (node)) {
		uae_u32 freeb = cd_long (node + 28);
		if (cd_word (node + 14) & 2) *chip_kb += freeb / 1024; else *other_kb += freeb / 1024;
		node = cd_long (node);
	}
	return 1;
}

static int cd_was_sane, cd_logged_exec;
void crashdiag_reset (void) { cd_was_sane = 0; cd_logged_exec = 0; }

/* Called once per emulated frame: notices the moment exec's low-memory structures get trashed. */
void crashdiag_frame (void)
{
	static unsigned frame, reports, sane_hf, sane_frame;
	static uae_u32 snap[13];
	uae_u32 eb = cd_long (4);
	int sane = cd_exec_ok (eb) && cd_long (0x10) >= 0xe00000;
	unsigned i;
	frame++;
	if (sane) {
		if (!cd_logged_exec) {
			unsigned c, o;
			cd_logged_exec = 1;
			crashdiag_mem (&c, &o);
			write_log ("exec: base %08x version %d.%d AttnFlags %04x, free memory chip %u KB other %u KB\n", eb,
				   cd_word (eb + 20), cd_word (eb + 22), cd_word (eb + 296), c, o);
		}
		snap[0] = eb;
		for (i = 0; i < 12; i++) snap[1 + i] = cd_long (8 + i * 4);
		sane_hf = hardfile_request_count ();
		sane_frame = frame;
		cd_was_sane = 1;
		return;
	}
	if (!cd_was_sane) return;                  /* still booting / already reported */
	cd_was_sane = 0;
	if (reports++ >= 3) return;
	write_log ("lowmem: exec structures trashed during frame %u (last good frame %u, hardfile requests since: %u)\n",
		   frame, sane_frame, hardfile_request_count () - sane_hf);
	write_log ("lowmem: ExecBase %08x -> %08x\n", snap[0], eb);
	for (i = 0; i < 12; i += 4)
		write_log ("lowmem: vec %02x: was %08x %08x %08x %08x  now %08x %08x %08x %08x\n", 8 + i * 4,
			   snap[1 + i], snap[2 + i], snap[3 + i], snap[4 + i],
			   cd_long (8 + i * 4), cd_long (12 + i * 4), cd_long (16 + i * 4), cd_long (20 + i * 4));
	for (i = 0; i < 0x80; i += 0x20)
		write_log ("lowmem: %04x: %08x %08x %08x %08x %08x %08x %08x %08x\n", i,
			   cd_long (i), cd_long (i + 4), cd_long (i + 8), cd_long (i + 12),
			   cd_long (i + 16), cd_long (i + 20), cd_long (i + 24), cd_long (i + 28));
	dump_cpu_context (0, m68k_get_pc ());
}
#endif

void uae_chk_handler(unsigned vector)
{
	unsigned opcode=m68k_fetch(m68k_get_pc());
	unsigned pc=m68k_get_pc();

	if (cloanto_rom && (opcode & 0xF100) == 0x7100) {
		_68k_dreg((opcode >> 9) & 7) = (uae_s8)(opcode & 0xFF);
		m68k_set_register(M68K_REG_PC,pc+2);
		return;
	}

	if (opcode == 0x4E7B && get_long (0x10) == 0 && (pc & 0xF80000) == 0xF80000) {
		write_log ("Your Kickstart requires a 68020 CPU. Giving up.\n");
		set_special (SPCFLAG_BRK);
		quit_program = 1;
		return;
	}

	if (opcode == 0xFF0D) {
		if ((pc & 0xF80000) == 0xF80000) {
			// This is from the dummy Kickstart replacement
			uae_u16 arg = m68k_fetch(pc+2);
			m68k_set_register(M68K_REG_PC,pc+4);
			ersatz_perform (arg);
			return;
		}
		else
		if ((pc & 0xFFFF0000) == RTAREA_BASE) {
			// User-mode STOP replacement
			M68KCONTEXT.execinfo|=0x0080;
			m68k_set_register(M68K_REG_PC,pc+2);
			return;
		}
	}

	if ((opcode & 0xF000) == 0xA000 && (pc & 0xFFFF0000) == RTAREA_BASE) {
		// Calltrap.
		m68k_set_register(M68K_REG_PC,pc+2);
		call_calltrap (opcode & 0xFFF);
		return;
	}

	if ((opcode & 0xF000) == 0xF000) {
		// Exception 0xB
		process_exception(0xB);
		return;
	}

	if ((opcode & 0xF000) == 0xA000) {
		if ((pc & 0xFFFF0000) == RTAREA_BASE) {
			// Calltrap.
			call_calltrap (opcode & 0xFFF);
		}
		process_exception(0xA);
		return;
	}

#ifdef __3DS__
	{
		static unsigned n_illegal;
		/* a runaway CPU raises thousands of these; every log line is a synced SD write */
		if (n_illegal < 30 || n_illegal % 2000 == 0)
			write_log ("Illegal instruction: %04x at %08lx (#%u)\n", opcode, pc, n_illegal + 1);
		if (n_illegal == 0 && opcode != 0x4e7b)
			dump_cpu_context (opcode, pc);
		else if (n_illegal == 1 || (n_illegal == 2 && opcode != 0x4e7b))
			dump_cpu_context (opcode, pc);
		n_illegal++;
	}
#else
	write_log ("Illegal instruction: %04x at %08lx\n", opcode, pc);
#endif
	process_exception(0x4);
}


void init_memmaps(addrbank* banco)
{
	unsigned i;

  M68K_CONTEXT *context = m68k_get_context();
  
	memset(context,0,sizeof(M68K_CONTEXT));
	memset(&micontexto_fpa,0,sizeof(hostptr)*256);

	micontexto_fpa[0x04]=(hostptr)&uae_chk_handler;

	/* PocketUAE/WinUAE traps */
	micontexto_fpa[0x0A]=(hostptr)&uae_chk_handler;
	micontexto_fpa[0x10]=(hostptr)&uae_chk_handler;
	micontexto_fpa[0x14]=(hostptr)&uae_chk_handler;
	micontexto_fpa[0x15]=(hostptr)&uae_chk_handler;

	context->icust_handler = (hostptr *)&micontexto_fpa;

	for(i=0;i<256;i++)
	{
		hostptr offset=(hostptr)banco->baseaddr;
		unsigned low_addr=(i<<16);
		unsigned high_addr=((i+1)<<16)-1;
		void *data=NULL;
		void *mem_handler_r8=NULL;
		void *mem_handler_r16=NULL;
		void *mem_handler_w8=NULL;
		void *mem_handler_w16=NULL;

		if (offset)
			data=(void *)(offset-low_addr);
		else
		{
			mem_handler_r8=(void *)banco->bget;
			mem_handler_r16=(void *)banco->wget;
			mem_handler_w8=(void *)banco->bput;
			mem_handler_w16=(void *)banco->wput;
		}

    famec_SetBank(low_addr, high_addr, ((hostptr)&mimemoriadummy)-low_addr, mem_handler_r8, 
      mem_handler_r16, mem_handler_w8, mem_handler_w16, data);
	}
}

void map_zone(unsigned addr, addrbank* banco, unsigned realstart)
{
	hostptr offset=(hostptr)banco->baseaddr;
	if (addr>255)
		return;

	unsigned low_addr=(addr<<16);
	unsigned high_addr=((addr+1)<<16)-1;

	if (offset)
	{
		offset+=((addr-realstart)<<16);
    famec_SetBank(low_addr, high_addr, offset-low_addr, NULL, NULL, NULL, NULL, (void *)(offset-low_addr));
	}
	else
	{
    famec_SetBank(low_addr, high_addr, ((hostptr)&mimemoriadummy)-low_addr, 
      (void*)banco->bget, (void*)banco->wget, (void*)banco->bput, (void*)banco->wput, NULL);
	}
}
