/***********************************************************************
 *  i386-stub.c
 *
 *  Description:  GDB target stub for the DJGPP (i386) platform.
 *
 *  Credits:      Based on Glenn Engel's (Uncopywrighted) port.
 *                Modifed by Jonathan Brogdon
 *
 *  Terms of use:  This software is provided for use under the terms
 *                 and conditions of the GNU General Public License.
 *                 You should have received a copy of the GNU General
 *                 Public License along with this program; if not, write
 *                 to the Free Software Foundation, Inc., 59 Temple Place
 *                 Suite 330, Boston, MA 02111-1307, USA.
 *
 *  History
 *  Engineer:           Date:              Notes:
 *  ---------           -----              ------
 *  Jonathan Brogdon    061700             Port from Glenn Engel's work.
 *
 *  Globals:  None.
 *  Functions:
 *
 *  Notes:
 *  The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

void gdb_start(void);
void gdb_checkpoint(void);

#ifdef NDEBUG
void gdb_start(void) {}
void gdb_checkpoint(void) {}
#else
#include <bios.h>
#include <dpmi.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/exceptn.h>
#include <go32.h>
#include <sys/farptr.h>


#ifdef GDB_DEBUG_PRINT
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

void set_debug_traps(void);
void restore_traps(void);

/*
 * BUFMAX defines the maximum number of characters in inbound/outbound buffers
 * at least NUMREGBYTES*2 are needed for register packets
 */
#define BUFMAX 400

/*
 * boolean flag. != 0 means we've been initialized
 */
static char gdb_initialized;

/*
 *  Hexadecimal character string.
 */
static char hexchars[] = "0123456789abcdef";

/*
 * Number of registers.
 */
#define NUMREGS 16

/*
 * Number of bytes of registers.
 */
#define NUMREGBYTES (NUMREGS * 4)

/*
 * i386 Registers
 */
enum regnames {
	EAX,
	ECX,
	EDX,
	EBX,
	ESP,
	EBP,
	ESI,
	EDI,
	PC /* also known as eip */,
	PS /* also known as eflags */,
	CS,
	SS,
	DS,
	ES,
	FS,
	GS
};

/*
 * Register storage buffer.
 */
int registers[NUMREGS];

static char *register_names[] = {"EAX",
								 "ECX",
								 "EDX",
								 "EBX",
								 "ESP",
								 "EBP",
								 "ESI",
								 "EDI",
								 "PC" /* also known as eip */,
								 "PS" /* also known as eflags */,
								 "CS",
								 "SS",
								 "DS",
								 "ES",
								 "FS",
								 "GS"};

/*
 * Address of a routine to RTE to if we get a memory fault.
 */
static void (*mem_fault_routine)() = NULL;

/* I/O buffers */
static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];

int was_interrupted = 0;

/*
 * BREAKPOINT macro
 */
#define BREAKPOINT() asm("    int $3   ")

#define Farpeekw(s, o) _farpeekw((s), (o))
#define UART_LINE_CONTROL 3
#define UART_LCR_DIVISOR_LATCH 0x80
#define UART_DIVISOR_LATCH_WORD 0
#define UART_BPS_DIVISOR_115200 1

#define UART_WRITE_BPS(BASE, D)                                                                  \
	{                                                                                            \
		outp(BASE + UART_LINE_CONTROL, inp(BASE + UART_LINE_CONTROL) | UART_LCR_DIVISOR_LATCH);  \
		_Outpw(BASE + UART_DIVISOR_LATCH_WORD, D);                                               \
		outp(BASE + UART_LINE_CONTROL, inp(BASE + UART_LINE_CONTROL) & ~UART_LCR_DIVISOR_LATCH); \
	}

#define _Outpw(a, w) (outpw(a, w))

int handle_exception(int);

void gdb_start(void) {
	// reference register_names, as in builds without GDB_PRINT_DEBUG it won't be referenced and generate a warning.
	((void) register_names[0]);
	_bios_serialcom(_COM_INIT, 0, _COM_9600 | _COM_NOPARITY | _COM_STOP1 | _COM_CHR8);
	unsigned int base = Farpeekw(0x0040, 0);
	UART_WRITE_BPS(base, UART_BPS_DIVISOR_115200);
	set_debug_traps();
	atexit(restore_traps);
	BREAKPOINT();
}

void putDebugChar(char c) { _bios_serialcom(_COM_SEND, 0, c); }

int getDebugChar(void) {
	return (_bios_serialcom(_COM_RECEIVE, 0, 0) & 0xff);
}

/***********************************************************************
 *  save_regs
 *
 *  Description:  Retreives the i386 registers as they were when the
 *                exception occurred.  Registers are stored in the
 *                local static buffer.
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
void save_regs(void) {
	registers[EAX] = (int) __djgpp_exception_state->__eax;
	registers[ECX] = (int) __djgpp_exception_state->__ecx;
	registers[EDX] = (int) __djgpp_exception_state->__edx;
	registers[EBX] = (int) __djgpp_exception_state->__ebx;
	registers[ESP] = (int) __djgpp_exception_state->__esp;
	registers[EBP] = (int) __djgpp_exception_state->__ebp;
	registers[ESI] = (int) __djgpp_exception_state->__esi;
	registers[EDI] = (int) __djgpp_exception_state->__edi;
	registers[PC] = (int) __djgpp_exception_state->__eip;
	registers[PS] = (int) __djgpp_exception_state->__eflags;
	registers[CS] = (int) __djgpp_exception_state->__cs;
	registers[SS] = (int) __djgpp_exception_state->__ss;
	registers[DS] = (int) __djgpp_exception_state->__ds;
	registers[ES] = (int) __djgpp_exception_state->__es;
	registers[FS] = (int) __djgpp_exception_state->__fs;
	registers[GS] = (int) __djgpp_exception_state->__gs;
}

void end_save_regs(void) {}

// https://chromium.googlesource.com/chromiumos/third_party/gdb/+/3a0081862f33c17f3779e30ba71aacde6e7ab1bd/gdb/stubs/i386-stub.c#139
extern void return_to_prog();
/* Restore the program's registers (including the stack pointer, which
   means we get the right stack and don't have to worry about popping our
   return address and any stack frames and so on) and return.  */
asm(".text");
asm(".globl _return_to_prog");
asm("_return_to_prog:");
asm("        movw _registers+44, %ss");
asm("        movl _registers+16, %esp");
asm("        movl _registers+4, %ecx");
asm("        movl _registers+8, %edx");
asm("        movl _registers+12, %ebx");
asm("        movl _registers+20, %ebp");
asm("        movl _registers+24, %esi");
asm("        movl _registers+28, %edi");
asm("        movw _registers+48, %ds");
asm("        movw _registers+52, %es");
asm("        movw _registers+56, %fs");
asm("        movw _registers+60, %gs");
asm("        movl _registers+36, %eax");
asm("        pushl %eax"); /* saved eflags */
asm("        movl _registers+40, %eax");
asm("        pushl %eax"); /* saved cs */
asm("        movl _registers+32, %eax");
asm("        pushl %eax"); /* saved eip */
asm("        movl _registers, %eax");
/* use iret to restore pc and flags together so
   that trace flag works right.  */
asm("        iret");
void _return_from_interrupt() { return_to_prog(); }

/***********************************************************************
 *  sigsegv_handler
 *
 *  Description:  Handles SIGSEGV signal
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
static void sigsegv_handler(int except_num) {

	/* Save general purpose registers */
	save_regs();

	debug("==== SIGSEGV\n");
	/* Dispatch memory fault handling routine if one is registered. */
	if (mem_fault_routine != 0) {
		(*mem_fault_routine)();
		mem_fault_routine = NULL;
	} else {
		/* Call the general exception handler */
		handle_exception(except_num);
	}

	_return_from_interrupt();
}

static void end_sigsegv_handler(void) {}

/***********************************************************************
 *  sigfpe_handler
 *
 *  Description:  Handles SIGFPE signal
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
static void sigfpe_handler(int except_num) {
	/* Save general purpose registers */
	save_regs();

	/* Call the general purpose exception handler */
	handle_exception(except_num);

	_return_from_interrupt();
}

static void end_sigfpe_handler(void) {}

/***********************************************************************
 *  sigtrap_handler
 *
 *  Description:  Handles SIGTRAP signal
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
static void sigtrap_handler(int except_num) {

	/* Save general purpose registers */
	save_regs();

	debug("==== SIGTRAP\n");
	/* Call the general purpose exception handler */
	handle_exception(except_num);

	_return_from_interrupt();
}

static void end_sigtrap_handler(int except_num) {}

/***********************************************************************
 *  sigill_handler
 *
 *  Description:  Handles SIGILL signal
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
static void sigill_handler(int except_num) {

	/* Save general purpose registers */
	save_regs();

	debug("==== SIGILL\n");

	/* Call the general purpose exception handler */
	handle_exception(except_num);

	_return_from_interrupt();
}

static void end_sigill_handler(int except_num) {}

/***********************************************************************
 *  hex
 *
 *  Description:  Convert ASCII character values, representing hex
 *                digits, to the integer value.
 *
 *  Inputs:
 *    ch      - data character
 *  Outputs:  None.
 *  Returns:  integer value represented by the input character.
 *
 ***********************************************************************/
static int hex(char ch) {
	if ((ch >= 'a') && (ch <= 'f'))
		return (ch - 'a' + 10);
	if ((ch >= '0') && (ch <= '9'))
		return (ch - '0');
	if ((ch >= 'A') && (ch <= 'F'))
		return (ch - 'A' + 10);
	return (-1);
}

static void end_hex(void) {}

/***********************************************************************
 *  getpacket
 *
 *  Description:  Retrieve GDB data packet.
 *                Scan for the sequence $<data>#<checksum>
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  Beginning of packet buffer.
 *
 ***********************************************************************/
static unsigned char *getpacket(void) {
	register unsigned char *buffer = (unsigned char *) remcomInBuffer;
	register unsigned char checksum;
	register unsigned char xmitcsum;
	register int count;
	register char ch;

	while (1) {
		/* wait around for the start character, ignore all other characters */
		while ((ch = getDebugChar()) != '$')
			;

	retry:
		checksum = 0;
		xmitcsum = -1;
		count = 0;

		/* now, read until a # or end of buffer is found */
		while (count < BUFMAX) {
			ch = getDebugChar();
			if (ch == '$')
				goto retry;
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}
		buffer[count] = 0;

		if (ch == '#') {
			ch = getDebugChar();
			xmitcsum = hex(ch) << 4;
			ch = getDebugChar();
			xmitcsum += hex(ch);

			if (checksum != xmitcsum) {
				putDebugChar('-'); /* failed checksum */
			} else {
				putDebugChar('+'); /* successful transfer */

				/* if a sequence char is present, reply the sequence ID */
				if (buffer[2] == ':') {
					putDebugChar(buffer[0]);
					putDebugChar(buffer[1]);

					return &buffer[3];
				}

				return &buffer[0];
			}
		}
	}
}

static void end_getpacket(void) {}

/***********************************************************************
 *  putpacket
 *
 *  Description:  Send GDB data packet.
 *
 *  Inputs:   Buffer of data to send.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
static void putpacket(unsigned char *buffer) {
	unsigned char checksum;
	int count;
	char ch;

	/*  $<packet info>#<checksum>. */
	do {
		putDebugChar('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count])) {
			putDebugChar(ch);
			checksum += ch;
			count += 1;
		}

		putDebugChar('#');
		putDebugChar(hexchars[checksum >> 4]);
		putDebugChar(hexchars[checksum % 16]);

	} while (getDebugChar() != '+');
}

static void end_putpacket(void) {}

/*
 * Indicate to caller of mem2hex or hex2mem that there has been an error.
 */
static int mem_err = 0;

/***********************************************************************
 *  set_mem_err
 *
 *  Description:  set memory error flag
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
static void set_mem_err(void) { mem_err = 1; }

static void end_set_mem_err(void) {}

/***********************************************************************
 *  get_char
 *
 *  Description:  Retreive a character from the specified address.
 *                These are separate functions so that they are so
 *                short and sweet that the compiler won't save any
 *                registers (if there is a fault to mem_fault, they
 *                won't get restored, so there better not be any saved).
 *
 *  Inputs:   addr  -  The address to read from.
 *  Outputs:  None.
 *  Returns:  data read from address.
 *
 ***********************************************************************/
static int get_char(char *addr) { return *addr; }

static void end_get_char(void) {}

/***********************************************************************
 *  set_char
 *
 *  Description:  Write a value to the specified address.
 *                These are separate functions so that they are so
 *                short and sweet that the compiler won't save any
 *                registers (if there is a fault to mem_fault, they
 *                won't get restored, so there better not be any saved).
 *
 *  Inputs:
 *     addr  -  The address to read from.
 *     val   -  value to write.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
static void set_char(char *addr, int val) { *addr = val; }

static void end_set_char(void) {}

/***********************************************************************
 *  mem2hex
 *
 *  Description:  Convert the memory pointed to by mem into hex, placing
 *                result in buf.  Return a pointer to the last char put
 *                in buf (null).  If MAY_FAULT is non-zero, then we should
 *                set mem_err in response to a fault; if zero treat a
 *                fault like any other fault in the stub.
 *
 *  Inputs:
 *     mem        -  Memory address
 *     buf        -  data buffer
 *     count      -  number of bytes
 *     may_fault  -  flag indicating that the operation may cause a mem fault.
 *  Outputs:  None.
 *  Returns:  Pointer to last character.
 *
 ***********************************************************************/
static char *mem2hex(char *mem, char *buf, int count, int may_fault) {
	int i;
	unsigned char ch;

	if (may_fault)
		mem_fault_routine = set_mem_err;
	for (i = 0; i < count; i++) {
		ch = get_char(mem++);
		debug("%x ", ch);
		if (may_fault && mem_err)
			return (buf);
		*buf++ = hexchars[ch >> 4];
		*buf++ = hexchars[ch % 16];
	}
	*buf = 0;
	if (may_fault)
		mem_fault_routine = NULL;
	return (buf);
}

static void end_mem2hex(void) {}

/***********************************************************************
 *  hex2mem
 *
 *  Description:  Convert the hex array pointed to by buf into binary to
 *                be placed in mem. Return a pointer to the character
 *                AFTER the last byte written
 *  Inputs:
 *     buf
 *     mem
 *     count
 *     may_fault
 *  Outputs:  None.
 *  Returns:  Pointer to buffer after last byte.
 *
 ***********************************************************************/
static char *hex2mem(char *buf, char *mem, int count, int may_fault) {
	int i;
	unsigned char ch;

	if (may_fault)
		mem_fault_routine = set_mem_err;
	for (i = 0; i < count; i++) {
		ch = hex(*buf++) << 4;
		ch = ch + hex(*buf++);
		debug("%x ", ch);
		set_char(mem++, ch);
		if (may_fault && mem_err)
			return (mem);
	}
	if (may_fault)
		mem_fault_routine = NULL;
	return (mem);
}

static void end_hex2mem(void) {}

/***********************************************************************
 *  computeSignal
 *
 *  Description:  This function takes the 386 exception vector and
 *                attempts to translate this number into a unix
 *                compatible signal value.
 *  Inputs:
 *     exceptionVector
 *  Outputs:  None.
 *  Returns:
 *
 ***********************************************************************/
static int computeSignal(int exceptionVector) {
	int sigval;
	switch (exceptionVector) {
		case 0:
			sigval = 8;
			break; /* divide by zero */
		case 1:
			sigval = 5;
			break; /* debug exception */
		case 302:
		case 3:
			sigval = 5;
			break; /* breakpoint */
		case 4:
			sigval = 16;
			break; /* into instruction (overflow) */
		case 5:
			sigval = 16;
			break; /* bound instruction */
		case 6:
			sigval = 4;
			break; /* Invalid opcode */
		case 7:
			sigval = 8;
			break; /* coprocessor not available */
		case 8:
			sigval = 7;
			break; /* double fault */
		case 9:
			sigval = 11;
			break; /* coprocessor segment overrun */
		case 10:
			sigval = 11;
			break; /* Invalid TSS */
		case 11:
			sigval = 11;
			break; /* Segment not present */
		case 12:
			sigval = 11;
			break; /* stack exception */
		case 13:
			sigval = 11;
			break; /* general protection */
		case 14:
			sigval = 11;
			break; /* page fault */
		case 16:
			sigval = 7;
			break; /* coprocessor error */
		default:
			sigval = 7; /* "software generated"*/
	}
	return (sigval);
}

static void end_computeSignal(void) {}

/***********************************************************************
 *  hexToInt
 *
 *  Description:  Convert an ASCII string to an integer.
 *
 *  Inputs:
 *  Outputs:  None.
 *  Returns:
 *
 ***********************************************************************/
static int hexToInt(char **ptr, int *intValue) {
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue >= 0) {
			*intValue = (*intValue << 4) | hexValue;
			numChars++;
		} else
			break;

		(*ptr)++;
	}

	return (numChars);
}

static void end_hexToInt(void) {}

/***********************************************************************
 *  handle_exception
 *
 *  Description:  This function does all command procesing for interfacing
 *                to GDB.
 *  Inputs:
 *    exceptionVector  - number of the vector.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
int handle_exception(int exceptionVector) {
	int sigval, stepping;
	int addr, length;
	char *ptr;

	/* reply to host that an exception has occurred */
	sigval = computeSignal(exceptionVector);
	debug("\n=== STOPPED: sig: %i, evec: %i, ip %p, [ip] %x\n", sigval, exceptionVector,
		  (void *) registers[PC], *(unsigned char *) registers[PC]);
	for (int l = 0; l < NUMREGS; l++)
		debug("%s: %x ", register_names[l], registers[l]);
	debug("\n");

	remcomOutBuffer[0] = 'S';
	remcomOutBuffer[1] = hexchars[sigval >> 4];
	remcomOutBuffer[2] = hexchars[sigval % 16];
	remcomOutBuffer[3] = 0;

	putpacket((unsigned char *) remcomOutBuffer);

	stepping = 0;

	while (1 == 1) {
		remcomOutBuffer[0] = 0;
		ptr = (char *) getpacket();
		char cmd = *ptr++;
		switch (cmd) {
			case '?':
				debug("? (Query the reason the target halted on connect)\n");
				remcomOutBuffer[0] = 'S';
				remcomOutBuffer[1] = hexchars[sigval >> 4];
				remcomOutBuffer[2] = hexchars[sigval % 16];
				remcomOutBuffer[3] = 0;
				break;
			case 'D':
				debug("D (Detach)\n");
				exit(0);
				break;
			case 'H':
				debug("H (Set thread for subsequent operations)\n");
				strcpy(remcomOutBuffer, "OK");
				break;
			case 'q':
				if (!strcmp(ptr, "C")) {
					debug("qC (Return the current thread ID.)\n");
					remcomOutBuffer[0] = 'Q';
					remcomOutBuffer[1] = 'C';
					remcomOutBuffer[2] = '0';
					remcomOutBuffer[3] = 0;
				} else if (!strcmp(ptr, "Attached")) {
					debug("qAttached (Check if attached to existing or new process)\n");
					remcomOutBuffer[0] = '1';
					remcomOutBuffer[1] = 0;
				} else if (!strcmp(ptr, "fThreadInfo")) {
					debug("qfThreadInfo (Obtain a list of all active thread IDs)\n");
					remcomOutBuffer[0] = 'm';
					remcomOutBuffer[1] = '0';
					remcomOutBuffer[2] = 0;
				} else if (!strcmp(ptr, "sThreadInfo")) {
					debug("qsThreadInfo (Obtain a list of all active thread IDs, subsequent)\n");
					remcomOutBuffer[0] = 'l';
					remcomOutBuffer[1] = 0;
				} else if (!strcmp(ptr, "Symbol::")) {
					debug("Symbol:: (Notify the target that GDB is prepared to serve symbol lookup requests)\n");
					strcpy(remcomOutBuffer, "OK");
				} else if (!strcmp(ptr, "")) {
				} else {
					debug("Unhandled: %c%s\n", cmd, ptr);
				}
				break;
			case 'd':
				debug("d (Toggle debug flag)\n");
				break;
			case 'g': /* return the value of the CPU registers */
				debug("g (Read general registers)\n");
				for (int l = 0; l < NUMREGS; l++)
					debug("%s: %x "
						  "",
						  register_names[l], registers[l]);
				debug("\n");
				mem2hex((char *) registers, remcomOutBuffer, NUMREGBYTES, 0);
				break;
			case 'G': /* set the value of the CPU registers - return OK */
				debug("G (Write general registers)\n");
				hex2mem(ptr, (char *) registers, NUMREGBYTES, 0);
				strcpy(remcomOutBuffer, "OK");
				break;
			case 'P': /* set the value of a single CPU register - return OK */
			{
				debug("P (Write register n with value r)\n");
				int regno;

				if (hexToInt(&ptr, &regno) && *ptr++ == '=')
					if (regno >= 0 && regno < NUMREGS) {
						debug("set reg: %i, ", regno);
						hex2mem(ptr, (char *) &registers[regno], 4, 0);
						debug("\n");
						strcpy(remcomOutBuffer, "OK");
						break;
					}

				strcpy(remcomOutBuffer, "E01");
				break;
			}

				/* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
			case 'm':
				debug("m (Read length addressable memory units starting at address addr)\n");
				/* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
				if (hexToInt(&ptr, &addr)) {
					debug("read, addr: %p, ", (void *) addr);
					if (*(ptr++) == ',') {
						if (hexToInt(&ptr, &length)) {
							ptr = 0;
							mem_err = 0;
							mem2hex((char *) addr, remcomOutBuffer, length, 1);
							if (mem_err) {
								strcpy(remcomOutBuffer, "E03");
							}
						}
					}
				}
				debug("\n");
				if (ptr) {
					strcpy(remcomOutBuffer, "E01");
				}
				break;

				/* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
			case 'M':
				debug("M (Write length addressable memory units starting at address addr)\n");
				/* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
				if (hexToInt(&ptr, &addr)) {
					debug("write, addr: %p, ", (void *) addr);
					if (*(ptr++) == ',') {
						if (hexToInt(&ptr, &length))
							if (*(ptr++) == ':') {
								mem_err = 0;
								hex2mem(ptr, (char *) addr, length, 1);

								if (mem_err) {
									strcpy(remcomOutBuffer, "E03");
								} else {
									strcpy(remcomOutBuffer, "OK");
								}

								ptr = 0;
							}
					}
					debug("\n");
				}
				if (ptr) {
					strcpy(remcomOutBuffer, "E02");
				}
				break;

				/* cAA..AA    Continue at address AA..AA(optional) */
				/* sAA..AA   Step one instruction from AA..AA(optional) */
			case 's':
				stepping = 1;
			case 'c': {
				/* try to read optional parameter, pc unchanged if no parm */
				addr = 0;
				if (hexToInt(&ptr, &addr)) {
					registers[PC] = addr;
				}

				debug("%c, offset: %p, ip: %p (%s)\n", cmd, (void *) addr, (void *) registers[PC], cmd == 'c' ? "Continue" : "Step");

				/* clear the trace bit */
				registers[PS] &= 0xfffffeff;

				/* set the trace bit if we're stepping */
				if (stepping)
					registers[PS] |= 0x100;// FIXME?

				return stepping;
			}
				/* kill the program */
			case 'k': /* do nothing */
				break;
			default:
				debug("Unhandled: %c%s\n", cmd, ptr);
		} /* switch */

		/* reply to the request */
		putpacket((unsigned char *) remcomOutBuffer);
	}
	return 0;
}

static void end_handle_exception(void) {}

// tick handler that polls the serial port for new bytes and calls
// handle_exception(302)
_go32_dpmi_seginfo old_handle_tick, new_handle_tick;
void handle_tick(void) {
	int status = _bios_serialcom(_COM_STATUS, 0, 0);
	if (status & (1 << 8)) {
		printf("Data on serial port, triggering exception handler.");
		was_interrupted = 1;
	}
}

void end_handle_tick() {}

void gdb_checkpoint(void) {
	if (was_interrupted) {
		was_interrupted = 0;
		BREAKPOINT();
	}
}

/***********************************************************************
 *  restore_traps
 *
 *  Description:  This function restores all used signal handlers to
 *                defaults.
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
void restore_traps(void) {
	/* Restore default signal handlers */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGTRAP, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGTRAP, SIG_DFL);
	signal(SIGILL, SIG_DFL);

	/* Clear init flag */
	gdb_initialized = 0;
}

/***********************************************************************
 *  lock_handler_data
 *
 *  Description:  This function locks all data that is used by the signal
 *                handlers.
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
extern jmp_buf *__djgpp_exception_state_ptr;
static void lock_handler_data(void) {
	_go32_dpmi_lock_data(&was_interrupted, sizeof(was_interrupted));
	_go32_dpmi_lock_data(&old_handle_tick, sizeof(old_handle_tick));
	_go32_dpmi_lock_data(&new_handle_tick, sizeof(new_handle_tick));

	_go32_dpmi_lock_data(&gdb_initialized, sizeof(gdb_initialized));
	_go32_dpmi_lock_data(hexchars, sizeof(hexchars));
	_go32_dpmi_lock_data(registers, sizeof(registers));

	_go32_dpmi_lock_data(remcomInBuffer, sizeof(remcomInBuffer));
	_go32_dpmi_lock_data(remcomOutBuffer, sizeof(remcomOutBuffer));

	_go32_dpmi_lock_code(getpacket,
						 (unsigned long) end_getpacket - (unsigned long) getpacket);
	_go32_dpmi_lock_code(putpacket,
						 (unsigned long) end_putpacket - (unsigned long) putpacket);

	_go32_dpmi_lock_data(&mem_fault_routine, sizeof(mem_fault_routine));
	_go32_dpmi_lock_data(&mem_err, sizeof(mem_err));

	_go32_dpmi_lock_code(set_mem_err, (unsigned long) end_set_mem_err -
											  (unsigned long) set_mem_err);
	_go32_dpmi_lock_code(get_char,
						 (unsigned long) end_get_char - (unsigned long) get_char);
	_go32_dpmi_lock_code(set_char,
						 (unsigned long) end_set_char - (unsigned long) set_char);
	_go32_dpmi_lock_code(mem2hex, (unsigned long) end_hex - (unsigned long) hex);
	_go32_dpmi_lock_code(mem2hex,
						 (unsigned long) end_mem2hex - (unsigned long) mem2hex);
	_go32_dpmi_lock_code(hex2mem,
						 (unsigned long) end_hex2mem - (unsigned long) hex2mem);
	_go32_dpmi_lock_code(computeSignal, (unsigned long) end_computeSignal -
												(unsigned long) computeSignal);
	_go32_dpmi_lock_code(hexToInt,
						 (unsigned long) end_hexToInt - (unsigned long) hexToInt);
	_go32_dpmi_lock_code(handle_exception, (unsigned long) end_handle_exception -
												   (unsigned long) handle_exception);

	_go32_dpmi_lock_code(handle_tick,
						 (unsigned long) end_handle_tick - (unsigned long) handle_tick);

	_go32_dpmi_lock_code(sigsegv_handler, (unsigned long) end_sigsegv_handler -
												  (unsigned long) sigsegv_handler);

	_go32_dpmi_lock_code(sigfpe_handler, (unsigned long) end_sigfpe_handler -
												 (unsigned long) sigfpe_handler);

	_go32_dpmi_lock_code(sigtrap_handler, (unsigned long) end_sigtrap_handler -
												  (unsigned long) sigtrap_handler);

	_go32_dpmi_lock_code(sigill_handler, (unsigned long) end_sigill_handler -
												 (unsigned long) sigill_handler);

	_go32_dpmi_lock_code(save_regs,
						 (unsigned long) end_save_regs - (unsigned long) save_regs);
}

/***********************************************************************
 *  set_debug_traps
 *
 *  Description:  This function installs signal handlers.
 *
 *  Inputs:   None.
 *  Outputs:  None.
 *  Returns:  None.
 *
 ***********************************************************************/
void set_debug_traps(void) {
	/* Lock any data that may be used by the trap handlers */
	lock_handler_data();

	/* Install signal handlers here */
	signal(SIGSEGV, sigsegv_handler);
	signal(SIGFPE, sigfpe_handler);
	signal(SIGTRAP, sigtrap_handler);
	signal(SIGILL, sigill_handler);

	/* Install the timer interrupt handler exec-interrupt messages from the debugger */
	_go32_dpmi_get_protected_mode_interrupt_vector(0x1c, &old_handle_tick);
	new_handle_tick.pm_offset = (int) handle_tick;
	new_handle_tick.pm_selector = _go32_my_cs();
	_go32_dpmi_allocate_iret_wrapper(&new_handle_tick);
	_go32_dpmi_set_protected_mode_interrupt_vector(0x1c, &new_handle_tick);

	/* Set init flag */
	gdb_initialized = 1;
}

#endif