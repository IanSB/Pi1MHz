#include "rpi.h"
#include "rpi-aux.h"
#include "arm-start.h"

// From here: https://www.raspberrypi.org/forums/viewtopic.php?f=72&t=53862
static void reboot_now(void)
{
  const int PM_PASSWORD = 0x5a000000;
  const int PM_RSTC_WRCFG_FULL_RESET = 0x00000020;
  unsigned int *PM_WDOG = (unsigned int *) (PERIPHERAL_BASE + 0x00100024);
  unsigned int *PM_RSTC = (unsigned int *) (PERIPHERAL_BASE + 0x0010001c);

  // timeout = 1/16th of a second? (whatever)
  *PM_WDOG = PM_PASSWORD | 1;
  *PM_RSTC = PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
  while (1);
}

static void dump_digit(unsigned char c) {
   c &= 15;
   if (c < 10) {
      c = '0' + c;
   } else {
      c = 'A' + c - 10;
   }
   RPI_AuxMiniUartWrite(c);
}

static void dump_hex(unsigned int value) {
  for (int i = 8; i !=0 ; i--) {
   dump_digit((uint8_t)(value >> 28));
   value <<= 4;
  }
}

static void dump_binary(unsigned int value) {
  int maxbits = (sizeof(value) * 8);
  for (int i = 0; i < maxbits; i++) {
    RPI_AuxMiniUartWrite('0' + (value >> (maxbits - 1)));
    value <<= 1;
  }
}

static void dump_string(char *string) {
  char c;
  while ((c = *string++) != 0) {
    RPI_AuxMiniUartWrite(c);
  }
}

// For some reason printf generally doesn't work here
static void dump_info(unsigned int *context, int offset, char *type) {
  unsigned int *addr;
  unsigned int *reg;
  unsigned int flags;

  // context point into the exception stack, at flags, followed by registers 0 .. 13
  reg = context + 1;
  dump_string(type);
  dump_string(" at ");
  // The stacked LR points one or two words afer the exception address
  addr = (unsigned int *)((reg[13] & ~3) - offset);
  dump_hex((unsigned int)addr);
#ifdef HAS_MULTICORE
  dump_string(" on core ");
  dump_digit(_get_core());
#endif
  dump_string("\r\n");
  dump_string("Registers:\r\n");
  for (int i = 0; i <= 13; i++) {
    int j = (i < 13) ? i : 14; // slot 13 actually holds the link register
    dump_string("  r[");
    RPI_AuxMiniUartWrite('0' + (j / 10));
    RPI_AuxMiniUartWrite('0' + (j % 10));
    dump_string("]=");
    dump_hex(reg[i]);
    dump_string("\r\n");
  }
  dump_string("Memory:\r\n");
  for (int i = -4; i <= 4; i++) {
    dump_string("  ");
    dump_hex((unsigned int) (addr + i));
    RPI_AuxMiniUartWrite('=');
    dump_hex(*(addr + i));
    if (i == 0) {
      dump_string(" <<<<<< \r\n");
    } else {
      dump_string("\r\n");
    }
  }
  // The flags are pointed to by context, before the registers
  flags = *context;
  dump_string("Flags: \r\n  NZCV--------------------IFTMMMMM\r\n  ");
  dump_binary(flags);
  dump_string(" (");
  // The last 5 bits of the flags are the mode
  switch (flags & 0x1f) {
  case 0x10:
    dump_string("User");
    break;
  case 0x11:
    dump_string("FIQ");
    break;
  case 0x12:
    dump_string("IRQ");
    break;
  case 0x13:
    dump_string("Supervisor");
    break;
  case 0x17:
    dump_string("Abort");
    break;
  case 0x1B:
    dump_string("Undefined");
    break;
  case 0x1F:
    dump_string("System");
    break;
  default:
    dump_string("Illegal");
    break;
  };
  dump_string(" Mode)\r\n");

  dump_string("Halted waiting for reset\r\n");

  reboot_now();
}

void undefined_instruction_handler(unsigned int *context) {
  dump_info(context, 4, "Undefined Instruction");
}

void prefetch_abort_handler(unsigned int *context) {
  dump_info(context, 4, "Prefetch Abort");
}

void data_abort_handler(unsigned int *context) {
  dump_info(context, 8, "Data Abort");
}

void swi_handler(unsigned int *context) {
  dump_info(context, 4, "SWI");
}
