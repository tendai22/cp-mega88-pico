
#include "machine.h"

extern void platform_reset(void);

void platform_reset (void)
{
  asm ("mov r30, r1");
  asm ("mov r31, r1");
  asm ("ijmp");
}

void
setup
(void)
{
  machine_boot();
}

void
loop
(void)
{

}

