// main.c -- Defines the C-code kernel entry point, calls initialisation routines.
//           Made for JamesM's tutorials <www.jamesmolloy.co.uk>

#include "monitor.h"
#include "descriptor_tables.h"
#include "timer.h"
#include "paging.h"
#include "kheap.h"
#include "task.h"
#include "syscall.h"

uint32_t initial_esp;

void do_stuff() {
  monitor_write_dec(getpid());
  monitor_write("heelo, !!\n");
}

void do_other_stuff() {
  monitor_write_dec(getpid());
  monitor_write("baack in black\n");
}

int caca(int a) {
  int x = 3;
  monitor_write_dec(a);
  return x;
}

int kernel_main(void *ptr, uint32_t initial_stack) {
  initial_esp = initial_stack;

  // Initialise all the ISRs and segmentation
  init_descriptor_tables();
  // Initialise the screen (by clearing it)
  monitor_clear();
  // Initialise the PIT to 100Hz
  asm volatile("sti");
  init_timer(100);

  initialise_paging();

  monitor_write_hex(initial_esp);
  initialise_tasking();

  //monitor_write("\nha\n");
  /*int ret = fork();

  monitor_write("fork returned: ");
  if (ret == 0) {
    do_stuff();
  } else {
    do_other_stuff();
  }*/

  initialise_syscalls();

  switch_to_user_mode();

  //monitor_write("daaa");
  //syscall_monitor_write("ce mai faci?");
  
  void *x = kmalloc(0x500000);
  monitor_write("da");

  for (;;);

  return 0;
}
