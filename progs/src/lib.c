#include "lib.h"


void *syscall_malloc(u_int32 size)
{
  void *ret;
  asm volatile(" \
mov $4, %%eax;                                  \
mov %0, %%ebx;                                  \
int $0x80;                                      \
mov %%eax, %0                                   \
" : "=r" (ret) : "r" (size) : "eax", "ebx");
  return ret;
}

void syscall_free(void *return_value)
{
  asm volatile ("\
mov $5, %%eax;                                  \
mov %0, %%ebx;                                  \
int $0x80;                                      \
" : : "r" (return_value) : "eax", "ebx");
}

fd syscall_open(string path, u_int8 oflag, u_int16 fperm)
{
  fd ret;
  
  asm volatile("\
mov $15, %%eax;                                         \
mov %0, %%ebx;                                          \
mov %1, %%ecx;                                          \
int $0x80;                                              \
mov %%eax, %0;                                          \
" : "=r" (ret) : "r" (path), "r" (oflag<<16 | fperm) \
               : "eax", "ebx", "ecx");

  return ret;
}

void syscall_close(fd f)
{
  asm volatile("\
mov $16, %%eax; \
mov %0, %%ebx;  \
int $0x80;      \
" : : "r" (f) : "eax", "ebx");
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
u_int32 syscall_read(fd f, u_int8* buffer, u_int32 offset, u_int32 length)
{
  asm volatile("\
mov $17, %%eax;                                                    \
mov 8(%%ebp), %%ebx;                                               \
mov 12(%%ebp), %%ecx;                                              \
mov 16(%%ebp), %%edx;                                              \
mov 20(%%ebp), %%edi;                                              \
int $0x80;                                                         \
ret" : : :);
  return 0xdead; // dead code
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic ignored "-Wunused-parameter"
u_int32 syscall_write(fd f, u_int8* buffer, u_int32 offset, u_int32 length)
{
  asm volatile("\
mov $18, %%eax;                                                    \
mov 8(%%ebp), %%ebx;                                               \
mov 12(%%ebp), %%ecx;                                              \
mov 16(%%ebp), %%edx;                                              \
mov 20(%%ebp), %%edi;                                              \
int $0x80;                                                         \
ret" : : :);
  return 0xdead; // dead code
}
#pragma GCC diagnostic pop

u_int32 syscall_fork(u_int32 priority, u_int32 *pid)
{
  u_int32 ret;
  asm volatile ("\
mov $1, %%eax;                                      \
mov %0, %%ebx;                                      \
int $0x80;                                          \
mov %%eax, %0                                       \
" : "=r" (ret) : "r" (priority) : "eax", "ebx");

  if (ret) {
    asm volatile ("mov %%ebx, %0" : "=r" (*pid) : : "ebx");
  }
  return ret;
}

void syscall_exit(u_int32 return_value)
{
  asm volatile ("\
mov $0, %%eax;                                  \
mov %0, %%ebx;                                  \
int $0x80;                                      \
" : : "r" (return_value) : "eax", "ebx");
}

bool syscall_wait(u_int32 *pid, u_int32 *return_value)
{
  u_int32 ret;
  asm volatile ("\
mov $2, %eax;    \
int $0x80;       \
");
  
  asm volatile ("mov %%eax, %0" : "=r" (ret) : : "eax");
  if (ret) {
    asm volatile ("\
mov %%ebx, %0;                                              \
mov %%ecx, %1;                                              \
" : "=r" (*pid), "=r" (*return_value) : : "ebx", "ecx");
  }
  return ret;
}


void syscall_printf(string s, ...)
{
  asm volatile ("\
mov $3, %%eax;                                  \
mov %0, %%ebx;                                  \
int $0x80;                                      \
" : : "r" (s) : "eax", "ebx");
}


void syscall_hlt()
{
  asm volatile ("\
mov $11, %eax;   \
int $0x80;       \
");
}