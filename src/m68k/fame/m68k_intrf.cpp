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
static unsigned cd_byte (uae_u32 a)
{
	return ((uae_u32) m68k_fetch (a & 0xFFFFFE) >> ((a & 1) ? 0 : 8)) & 0xFF;
}
static void cd_str (uae_u32 a, char *out, unsigned n)
{
	unsigned i;
	for (i = 0; i + 1 < n; i++) {
		unsigned c = cd_byte (a + i);
		if (!c) break;
		out[i] = (c >= 32 && c < 127) ? (char) c : '?';
	}
	out[i] = 0;
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

/* ---- write watch on the Amiga's low memory --------------------------------------------------------------------------
 * Nothing legitimate writes to the reset vectors (address 0..7: initial SSP/PC, and the ExecBase pointer at 4) once exec
 * is up. When a program does it anyway (e.g. a Read() into a NULL buffer), the whole vector table and ExecBase pointer
 * are gone and the machine runs away. FAME accesses chip RAM through direct page pointers, so write handlers are
 * installed on the page(s) that map chip offset 0 (and on the pages of the mirror above the real chip RAM, e.g.
 * 0x100000..0x1FFFFF with 1 MB); reads stay direct. The handler sees the exact PC and registers of the writer.
 * Every hit gets one short log line (boot-time exec writes in supervisor mode are expected); a full dump (registers,
 * task, memory chunks, DOS packet candidates, stack) is written for the first few hits from *different* PCs that are
 * either in user mode or happen well after the reset. */
static int cd_armed, cd_trashed;
static unsigned cd_wp_hits, cd_frames_since_reset, cd_full_dumps;
static unsigned cd_dump_pcs[6];
static uae_u32 cd_last_eb;
void crashdiag_reset (void) { cd_was_sane = 0; cd_logged_exec = 0; cd_armed = 0; cd_trashed = 0; cd_wp_hits = 0; cd_frames_since_reset = 0; cd_full_dumps = 0; }
unsigned crashdiag_frame_count (void) { return cd_frames_since_reset; }

/* name, type and (for a Process running a CLI command) the command name of a Task; returns the node type, 0 if not plausible */
static unsigned cd_task_names (uae_u32 task, char *name, unsigned nn, char *cmd, unsigned cn)
{
	unsigned type;
	uae_u32 nm;
	strcpy (name, "?");
	cmd[0] = 0;
	if (task < 0x400 || task >= 0xA00000 || (task & 1)) return 0;
	type = cd_byte (task + 8);
	nm = cd_long (task + 10);
	if (nm >= 0x400 && nm < 0xA00000) cd_str (nm, name, nn);
	if (type == 13) {                           /* NT_PROCESS: name of the CLI command it is running */
		uae_u32 cli = cd_long (task + 0xAC) << 2;
		if (cli >= 0x400 && cli < 0xA00000) {
			uae_u32 s = cd_long (cli + 16) << 2;
			if (s >= 0x400 && s < 0xA00000) {
				unsigned l = cd_byte (s), i;
				if (l > cn - 1) l = cn - 1;
				for (i = 0; i < l; i++) { unsigned ch = cd_byte (s + 1 + i); cmd[i] = (ch >= 32 && ch < 127) ? (char) ch : '?'; }
				cmd[l] = 0;
			}
		}
	}
	return type;
}

/* CLI command name (or task name) of the task that is running right now, for the status-bar message */
void crashdiag_task_name (char *out, unsigned n)
{
	uae_u32 eb = cd_long (4);
	char name[40], cmd[48];
	strcpy (name, "?");
	cmd[0] = 0;
	if (cd_exec_ok (eb)) cd_task_names (cd_long (eb + 0x114), name, sizeof name, cmd, sizeof cmd);
	snprintf (out, n, "%s", cmd[0] ? cmd : name);
}

/* "'name' cmd 'xxx'" of the task that is running right now (for the AllocMem trace in filesys.cpp) */
void crashdiag_task_desc (char *out, unsigned n)
{
	uae_u32 eb = cd_long (4);
	char name[40], cmd[48];
	unsigned type = 0;
	uae_u32 task = 0;
	if (cd_exec_ok (eb)) {
		task = cd_long (eb + 0x114);
		type = cd_task_names (task, name, sizeof name, cmd, sizeof cmd);
	}
	if (!type) { strcpy (name, "?"); cmd[0] = 0; }
	snprintf (out, n, "%08x '%s' type %u cmd '%s'", task, name, type, cmd);
}

static void cd_dump_task (void)
{
	uae_u32 eb = cd_last_eb, task;
	char name[40], cmd[48];
	unsigned type;
	if (!eb) { write_log ("lowwrite: no ExecBase known\n"); return; }
	task = cd_long (eb + 0x114);
	type = cd_task_names (task, name, sizeof name, cmd, sizeof cmd);
	write_log ("lowwrite: ExecBase %08x ThisTask %08x '%s' type %u CLI command '%s', SPReg %08x SPLower %08x SPUpper %08x, IDNest %d TDNest %d\n",
		   eb, task, name, type, cmd, task >= 0x400 ? cd_long (task + 0x36) : 0,
		   task >= 0x400 ? cd_long (task + 0x3A) : 0, task >= 0x400 ? cd_long (task + 0x3E) : 0,
		   (int) (signed char) cd_byte (eb + 0x126), (int) (signed char) cd_byte (eb + 0x127));
}

/* exec's memory lists: attributes, bounds, free bytes and the free chunks (is a big AllocMem plausible to fail?) */
static void cd_dump_mem (void)
{
	uae_u32 eb = cd_last_eb, node;
	unsigned n = 0;
	if (!eb) return;
	node = cd_long (eb + 322);
	while (n++ < 6 && node >= 0x400 && node < 0x1000000 && !(node & 1) && cd_long (node)) {
		uae_u32 chunk = cd_long (node + 16);
		unsigned k = 0, largest = 0;
		write_log ("lowwrite: memhdr %08x attr %04x lower %08x upper %08x free %u bytes; free chunks:", node, cd_word (node + 14),
			   cd_long (node + 20), cd_long (node + 24), cd_long (node + 28));
		while (chunk >= 0x400 && chunk < 0x1000000 && !(chunk & 1) && k < 400) {
			unsigned sz = cd_long (chunk + 4);
			if (sz > largest) largest = sz;
			if (k < 14) write_log (" %x+%x", chunk, sz);
			chunk = cd_long (chunk);
			k++;
		}
		write_log ("%s (%u chunks, largest %u bytes)\n", k > 14 ? " ..." : "", k, largest);
		node = cd_long (node);
	}
}

/* Largest free chunk of the chip RAM list in KB (0 if unknown): a program asking for more fails even if the total is large */
unsigned crashdiag_largest_chip_kb (void)
{
	uae_u32 eb = cd_long (4), node;
	unsigned n = 0, largest = 0;
	if (!cd_exec_ok (eb)) return 0;
	node = cd_long (eb + 322);
	while (n++ < 6 && node >= 0x400 && node < 0x1000000 && !(node & 1) && cd_long (node)) {
		if (cd_word (node + 14) & 2) {
			uae_u32 chunk = cd_long (node + 16);
			unsigned k = 0;
			while (chunk >= 0x400 && chunk < 0x1000000 && !(chunk & 1) && k++ < 400) {
				unsigned sz = cd_long (chunk + 4);
				if (sz > largest) largest = sz;
				chunk = cd_long (chunk);
			}
		}
		node = cd_long (node);
	}
	return largest / 1024;
}

/* A DosPacket in a register or on the stack: dp_Link points back to the StandardPacket's message (p - 20) */
static void cd_find_packets (void)
{
	uae_u32 cand[8 + 48], seen[4];
	unsigned n = 0, i, found = 0, sp = _68k_areg (7);
	for (i = 0; i < 7; i++) cand[n++] = _68k_areg (i);
	for (i = 0; i < 48; i++) cand[n++] = cd_long (sp + i * 4);
	for (i = 0; i < n && found < 4; i++) {
		uae_u32 p = cand[i];
		unsigned j, dup = 0;
		if (p < 0x420 || p >= 0xA00000 || (p & 1)) continue;
		if (cd_long (p) != p - 20 || cd_long (p + 8) > 0x1000) continue;
		for (j = 0; j < found; j++) if (seen[j] == p) dup = 1;
		if (dup) continue;
		seen[found++] = p;
		write_log ("lowwrite: DosPacket? %08x port %08x type %u res1 %08x res2 %08x arg1 %08x arg2 %08x arg3 %08x (ACTION_READ=82 WRITE=87: arg2=buffer arg3=length)\n",
			   p, cd_long (p + 4), cd_long (p + 8), cd_long (p + 12), cd_long (p + 16), cd_long (p + 20), cd_long (p + 24), cd_long (p + 28));
		{                                       /* who sent it: the reply port belongs to the client's Process */
			uae_u32 port = cd_long (p + 4), client = (port >= 0x400 && port < 0xA00000 && !(port & 1)) ? cd_long (port + 16) : 0;
			char name[40], cmd[48];
			unsigned type = cd_task_names (client, name, sizeof name, cmd, sizeof cmd);
			write_log ("lowwrite:   packet sent by task %08x '%s' type %u CLI command '%s'\n", client, name, type, cmd);
		}
	}
}

static void cd_dump_stack (const char *what, uae_u32 sp, unsigned n)
{
	unsigned i;
	for (i = 0; i < n; i += 6)
		write_log ("lowwrite: %s %08x: %08x %08x %08x %08x %08x %08x\n", what, sp + i * 4,
			   cd_long (sp + i * 4), cd_long (sp + i * 4 + 4), cd_long (sp + i * 4 + 8),
			   cd_long (sp + i * 4 + 12), cd_long (sp + i * 4 + 16), cd_long (sp + i * 4 + 20));
}

static void cd_wp_hit (uae_u32 addr, unsigned data, int size)
{
	M68K_CONTEXT *c = m68k_get_context ();
	unsigned pc = m68k_get_pc (), i, full = 0;
	if (cd_trashed) return;                       /* the machine is already running amok: nothing more to learn */
	cd_wp_hits++;
	if (cd_wp_hits <= 80)
		write_log ("lowwrite: #%u %s write %0*x to %08x by PC=%08x (%s mode, A7=%08x, %u frames after reset)\n", cd_wp_hits,
			   size == 1 ? "byte" : "word", size * 2, data, addr, pc, c->flag_s ? "supervisor" : "user",
			   _68k_areg (7), cd_frames_since_reset);
	if (cd_full_dumps >= 5 || (c->flag_s && cd_frames_since_reset < 300)) return;
	for (i = 0; i < cd_full_dumps; i++) if (cd_dump_pcs[i] == pc) return;
	cd_dump_pcs[cd_full_dumps++] = pc;
	write_log ("lowwrite: ---- full dump for hit #%u ----\n", cd_wp_hits);
	write_log ("lowwrite: D %08x %08x %08x %08x %08x %08x %08x %08x\n", _68k_dreg(0), _68k_dreg(1), _68k_dreg(2), _68k_dreg(3), _68k_dreg(4), _68k_dreg(5), _68k_dreg(6), _68k_dreg(7));
	write_log ("lowwrite: A %08x %08x %08x %08x %08x %08x %08x %08x  USP=%08x\n", _68k_areg(0), _68k_areg(1), _68k_areg(2), _68k_areg(3), _68k_areg(4), _68k_areg(5), _68k_areg(6), _68k_areg(7), _68k_uspreg);
	write_log ("lowwrite: code before PC:");
	for (i = 0; i < 12; i++) write_log (" %04x", cd_word (pc - 24 + i * 2));
	write_log ("  | at PC:");
	for (i = 0; i < 4; i++) write_log (" %04x", cd_word (pc + i * 2));
	write_log ("\n");
	cd_dump_task ();
	cd_dump_mem ();
	cd_find_packets ();
	cd_dump_stack ("stack", _68k_areg (7), 48);
	if (c->flag_s) cd_dump_stack ("ustack", _68k_uspreg, 24);
	if (cd_full_dumps == 1) {
		autoconf_dump_traphist ();
		hardfile_dump_history ();
	}
}

static void cd_wp_b (s32 addr, s32 data)
{
	uae_u32 a = (uae_u32) addr & chipmem_mask;
	if ((uae_u32) addr >= allocated_chipmem || (a < 8 && *((uae_u8 *) (chipmemory + (a ^ 1))) != (uae_u8) data))
		cd_wp_hit (addr, data & 0xFF, 1);
	*((uae_u8 *) (chipmemory + (a ^ 1))) = data;
}

static void cd_wp_w (s32 addr, s32 data)
{
	uae_u32 a = (uae_u32) addr & chipmem_mask;
	if ((uae_u32) addr >= allocated_chipmem || (a < 8 && ((a & 1) || *((uae_u16 *) (chipmemory + a)) != (uae_u16) data)))
		cd_wp_hit (addr, data & 0xFFFF, 2);
	if (a & 1) {                               /* same layout as FAME's direct odd-address word write */
		uae_u8 *p = chipmemory + a;
		*(p - 1) = data >> 8;
		*(p + 2) = data;
	} else
		*((uae_u16 *) (chipmemory + a)) = data;
}

/* called by map_zone() for every page that maps chip RAM offset 0 */
static void crashdiag_watch_page (unsigned lo, unsigned hi)
{
	famec_SetWriteHandlers (lo, hi, (void *) &cd_wp_b, (void *) &cd_wp_w);
}

/* Called once per emulated frame: notices the moment exec's low-memory structures get trashed. */
void crashdiag_frame (void)
{
	static unsigned frame, reports, sane_hf, sane_frame;
	static uae_u32 snap[13];
	uae_u32 eb = cd_long (4);
	int sane = cd_exec_ok (eb) && cd_long (0x10) >= 0xe00000;
	unsigned i;
	frame++;
	cd_frames_since_reset++;
	if (sane) {
		if (!cd_logged_exec) {
			unsigned c, o;
			cd_logged_exec = 1;
			crashdiag_mem (&c, &o);
			write_log ("exec: base %08x version %d.%d AttnFlags %04x, free memory chip %u KB other %u KB\n", eb,
				   cd_word (eb + 20), cd_word (eb + 22), cd_word (eb + 296), c, o);
		}
		snap[0] = eb;
		cd_last_eb = eb;
		for (i = 0; i < 12; i++) snap[1 + i] = cd_long (8 + i * 4);
		sane_hf = hardfile_request_count ();
		sane_frame = frame;
		cd_was_sane = 1;
		return;
	}
	if (!cd_was_sane) return;                  /* still booting / already reported */
	cd_was_sane = 0;
	cd_trashed = 1;
	{
		char who[48], msg[48];
		unsigned kb = 0, at = 0;
		extern int alloctrace_last_failure (char *name, unsigned nn, unsigned *kb, unsigned *frame);
		extern void gui_set_message (const char *msg, int t);
		if (alloctrace_last_failure (who, sizeof who, &kb, &at) && cd_frames_since_reset - at < 3000)
			snprintf (msg, sizeof msg, "CRASH: %.9s: alloc failed %uK", who, kb);
		else
			snprintf (msg, sizeof msg, "CRASH: Amiga low memory overwritten");
		gui_set_message (msg, 3000);
	}
	if (reports++ >= 3) return;
	write_log ("lowmem: exec structures trashed during frame %u (last good frame %u, hardfile requests since: %u)\n",
		   frame, sane_frame, hardfile_request_count () - sane_hf);
	write_log ("lowmem: write watch saw %u writes to chip offset 0..7 / the chip mirror since the reset (a trash with no hit after the boot-time ones = not the CPU, e.g. DMA/native code)\n", cd_wp_hits);
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
#ifdef __3DS__
    if (banco == &chipmem_bank && (addr == realstart || low_addr >= allocated_chipmem))
      crashdiag_watch_page(low_addr, high_addr);
#endif
	}
	else
	{
    famec_SetBank(low_addr, high_addr, ((hostptr)&mimemoriadummy)-low_addr, 
      (void*)banco->bget, (void*)banco->wget, (void*)banco->bput, (void*)banco->wput, NULL);
	}
}
