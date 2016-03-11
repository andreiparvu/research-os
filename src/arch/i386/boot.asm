global start

extern kernel_main

extern code
bits 32

start:
  mov edi, ebx ; Multiboot info

  call check_multiboot

  push esp
  push ebx

  call kernel_main

  hlt

check_multiboot:
  cmp eax, 0x36d76289
  jne .no_multiboot
  ret

.no_multiboot:
  mov al, "0"
  jmp error

; Prints `ERR: ` and the given error code to screen and hangs.
; parameter: error code (in ascii) in al
error:
  mov dword [0xb8000], 0x4f524f45
  mov dword [0xb8004], 0x4f3a4f52
  mov dword [0xb8008], 0x4f204f20
  mov byte  [0xb800a], al
  hlt

global gdt_flush

gdt_flush:
  mov eax, [esp + 4]
  lgdt [eax]

  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  jmp 0x08:.flush
.flush:
  ret

global idt_flush    ; Allows the C code to call idt_flush().

idt_flush:
  mov eax, [esp+4]  ; Get the pointer to the IDT, passed as a parameter. 
  lidt [eax]        ; Load the IDT pointer.
  ret

%macro ISR_NOERRCODE 1
  global isr%1
  isr%1:
    cli
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
  global isr%1
  isr%1:
    cli
    push byte %1
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31
ISR_NOERRCODE 128

extern isr_handler

isr_common_stub:
   pusha                    ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

   mov ax, ds               ; Lower 16-bits of eax = ds.
   push eax                 ; save the data segment descriptor

   mov ax, 0x10  ; load the kernel data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   call isr_handler

   pop eax        ; reload the original data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa                     ; Pops edi,esi,ebp...
   add esp, 8     ; Cleans up the pushed error code and pushed ISR number
   sti
   iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

%macro IRQ 2
  global irq%1
  irq%1:
    cli
    push byte 0
    push byte %2
    jmp irq_common_stub
%endmacro

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

extern irq_handler

irq_common_stub:
  pusha                    ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

  mov ax, ds               ; Lower 16-bits of eax = ds.
  push eax                 ; save the data segment descriptor

  mov ax, 0x10  ; load the kernel data segment descriptor
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  call irq_handler

  pop ebx        ; reload the original data segment descriptor
  mov ds, bx
  mov es, bx
  mov fs, bx
  mov gs, bx

  popa                     ; Pops edi,esi,ebp...
  add esp, 8     ; Cleans up the pushed error code and pushed ISR number
  sti
  iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

global copy_page_physical
copy_page_physical:
  push ebx              ; According to __cdecl, we must preserve the contents of EBX.
  pushf                 ; push EFLAGS, so we can pop it and reenable interrupts
  ; later, if they were enabled anyway.
  cli                   ; Disable interrupts, so we aren't interrupted.
  ; Load these in BEFORE we disable paging!
  mov ebx, [esp+12]     ; Source address
  mov ecx, [esp+16]     ; Destination address

  mov edx, cr0          ; Get the control register...
  and edx, 0x7fffffff   ; and...
  mov cr0, edx          ; Disable paging.

  mov edx, 1024         ; 1024*4bytes = 4096 bytes to copy

  .loop:
  mov eax, [ebx]        ; Get the word at the source address
  mov [ecx], eax        ; Store it at the dest address
  add ebx, 4            ; Source address += sizeof(word)
  add ecx, 4            ; Dest address += sizeof(word)
  dec edx               ; One less word to do
  jnz .loop

  mov edx, cr0          ; Get the control register again
  or  edx, 0x80000000   ; and...
  mov cr0, edx          ; Enable paging.

  popf                  ; Pop EFLAGS back.
  pop ebx               ; Get the original value of EBX back.
  ret

global read_eip
read_eip:
  pop eax
  jmp eax

global do_fucking_jump
do_fucking_jump:

  mov ecx, [esp + 4]
  mov ebp, [esp + 8]
  mov eax, [esp + 12]
  mov ebx, [esp + 16]

  mov esp, eax
  mov eax, 12345h

  mov cr3, ebx

  sti
  jmp ecx

global tss_flush
tss_flush:
  mov ax, 0x2B      ; Load the index of our TSS structure - The index is
  ; 0x28, as it is the 5th selector and each is 8 bytes
  ; long, but we set the bottom two bits (making 0x2B)
  ; so that it has an RPL of 3, not zero.
  ltr ax            ; Load 0x2B into the task state register.
  ret
