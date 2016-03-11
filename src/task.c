#include "task.h"
#include "common.h"

volatile task_t *current_task;

// The start of the task linked list.
volatile task_t *ready_queue;

// Some externs are needed to access members in paging.c...
extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;
extern void alloc_frame(page_t*,int,int);
extern u32int initial_esp;
extern u32int read_eip();

// The next available process ID.
u32int next_pid = 1;

void initialise_tasking() {
  asm volatile("cli");

  // relocate the stack
  move_stack((void*)0xe0000000, 0x5000);

  current_task = ready_queue = (task_t*)kmalloc(sizeof(task_t));
  current_task->id = next_pid++;
  current_task->esp = current_task->ebp = 0;
  current_task->eip = 0;
  current_task->page_directory = current_directory;
  current_task->next = 0;

  asm volatile("sti");
}

int fork() {
  asm volatile("cli");

  task_t *parent_task = (task_t*)current_task;

  page_directory_t *directory = clone_directory(current_directory);

  task_t *new_task = (task_t*)kmalloc(sizeof(task_t));
  new_task->id = next_pid++;
  new_task->esp = new_task->ebp = 0;
  new_task->eip = 0;
  new_task->page_directory = directory;
  new_task->next = 0;

  task_t *tmp_task = (task_t*)ready_queue;
  while (tmp_task->next) {
    tmp_task = tmp_task->next;
  }

  tmp_task->next = new_task;

  uint32_t eip = read_eip();

  if (current_task == parent_task) {
    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    uint32_t ebp;
    asm volatile("mov %%ebp, %0" : "=r"(ebp));

    new_task->esp = esp;
    new_task->ebp = ebp;
    new_task->eip = eip;

    asm volatile("sti");
    return new_task->id;
  }

  return 0;
}

void switch_task() {
  if (!current_task) {
    return ;
  }

  uint32_t esp, ebp, eip;
  asm volatile("mov %%esp, %0" : "=r"(esp));
  asm volatile("mov %%ebp, %0" : "=r"(ebp));

  eip = read_eip();
  if (eip == 0x12345) {
    return;
  }

  current_task->eip = eip;
  current_task->esp = esp;
  current_task->ebp = ebp;

  current_task = current_task->next;
  if (!current_task) {
    current_task = ready_queue;
  }

  eip = current_task->eip;
  esp = current_task->esp;
  ebp = current_task->ebp;

  current_directory = current_task->page_directory;
  // Here we:
  // * Stop interrupts so we don't get interrupted.
  // * Temporarily put the new EIP location in ECX.
  // * Load the stack and base pointers from the new task struct.
  // * Change page directory to the physical address (physicalAddr) of the new directory.
  // * Put a dummy value (0x12345) in EAX so that above we can recognise that we've just
  // switched task.
  // * Restart interrupts. The STI instruction has a delay - it doesn't take effect until after
  // the next instruction.
  // * Jump to the location in ECX (remember we put the new EIP in there).
  do_fucking_jump(eip, ebp, esp, current_directory->physicalAddr);
}

void move_stack(void *new_stack_start, uint32_t size) {
  int i;
  for (i = new_stack_start;
         i >= (int)new_stack_start - size; i -= 0x1000) {
    //monitor_write("\ndoing\n");
    //monitor_write_hex(i);
    alloc_frame(get_page(i, 1, current_directory), 0, 1);
  }

  monitor_write("\n\n");
  monitor_write_dec(size);
  monitor_write("\n");
  monitor_write_hex(new_stack_start - size);
  monitor_write("\n");
  memset(new_stack_start - size, 0, size);
  monitor_write("\ncool\n");
  uint32_t pd_addr;
  asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
  asm volatile("mov %0, %%cr3" : : "r" (pd_addr));

  // Old ESP and EBP, read from registers.
  uint32_t old_stack_pointer;
  asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
  uint32_t old_base_pointer;
  asm volatile("mov %%ebp, %0" : "=r" (old_base_pointer));

  uint32_t offset = (uint32_t)new_stack_start - initial_esp;
  uint32_t new_stack_pointer = old_stack_pointer + offset;
  uint32_t new_base_pointer = old_base_pointer + offset;

  monitor_write_hex(new_stack_pointer);
  monitor_write("\nhaaaaa\n");
  monitor_write_hex(initial_esp);
  monitor_write("\n");
  monitor_write_hex(old_stack_pointer);
  //monitor_write_hex(initial_esp - old_stack_pointer);
  memcpy((void*)new_stack_pointer, (void*)old_stack_pointer,
      initial_esp - old_stack_pointer);

  monitor_write("\nhaaaaa\n");
  for (i = (int)new_stack_start; i > (int)new_stack_start - size;
      i -= 4) {
    uint32_t tmp = *(uint32_t*)i;

    if (old_stack_pointer < tmp && tmp < initial_esp) {
      tmp = tmp + offset;
      uint32_t *tmp2 = (uint32_t*)i;
      *tmp2 = tmp;
    }
  }

  asm volatile("mov %0, %%esp" : : "r" (new_stack_pointer));
  asm volatile("mov %0, %%ebp" : : "r" (new_base_pointer));
}

int getpid() {
  return current_task->id;
}

void switch_to_user_mode() {
  set_kernel_stack(current_task->kernel_stack+KERNEL_STACK_SIZE);

  asm volatile("  \
      cli; \
      mov $0x23, %ax; \
      mov %ax, %ds; \
      mov %ax, %es; \
      mov %ax, %fs; \
      mov %ax, %gs; \
      \
      mov %esp, %eax; \
      pushl $0x23; \
      pushl %eax; \
      pushf; \
      pushl $0x1B; \
      push $1f; \
      iret; \
      1: \
      ");
}
