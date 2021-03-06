#include "process.h"
#include "paging.h"
#include "error.h"
#include "malloc.h"
#include "logging.h"
#include "gdt.h"


process_t new_process(pid parent_id, priority prio, bool create_page_dir)
{
  /* kloug(100, "Creating new process\n"); */

  process_t proc;
  proc.state = Runnable;
  proc.parent_id = parent_id;
  proc.prio = prio;

  context_t ctx;
  if (create_page_dir) {
    ctx.page_dir = new_page_dir(&ctx.first_free_block, &ctx.unallocated_mem);
  } else {
    ctx.page_dir = NULL;
  }
  /* kloug(100, "Malloc state: %x %x\n", ctx.first_free_block, ctx.unallocated_mem); */

  regs_t *regs = (regs_t *)mem_alloc(sizeof(regs_t));
  /* The data and general purpose segment registers are set to the user data segment */
  regs->ds = regs->es = regs->fs = regs->gs = USER_DATA_SEGMENT;
  /* All general purpose registers are set to 0 */
  regs->eax = regs->ebx = regs->ecx = regs->edx = regs->esi = regs->edi = regs->esp = regs->ebp = 0;
  /* We don't care about the error codes */
  regs->int_no = regs->err_code= 0;
  /* There comes the interesting stuff */
  regs->eip = NULL;  /* Will be set during code loading */
  regs->cs = USER_CODE_SEGMENT;
  regs->eflags = 0x200;  /* Interruptions */
  regs->useresp = START_OF_USER_STACK;
  regs->ss = USER_STACK_SEGMENT;

  ctx.regs = regs;
  proc.context = ctx;

  return proc;
}
