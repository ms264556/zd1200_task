#include <linux/oprofile.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/stacktrace.h>
#include <linux/stacktrace.h>
#include <linux/kernel.h>
#include <asm/sections.h>

struct stackframe {
  unsigned long sp;
  unsigned long pc;
  unsigned long ra;
};

int userspace;

#define GET_USERSPACE_VALUE(val, addr) \
{ \
if (!access_ok(VERIFY_READ, addr, sizeof(val))) \
  return -5; \
__get_user(val, addr); \
}

#define GET_VALUE(val, addr) \
{ \
if (userspace) \
  GET_USERSPACE_VALUE(val, addr) \
else { \
  if (!kstack_end(addr)/*((unsigned long)addr < (unsigned long)_end &&*/ \
      /*(unsigned long)addr > (unsigned long)_stext)*/) { \
  val=*addr; \
  } else { \
  return -6; \
  } } \
}

int notrace unwind_frame_and_report(struct stackframe *old_frame) {
  struct stackframe new_frame = *old_frame;
  size_t ra_offset = 0;
  size_t stack_size = 0;
  unsigned long * addr;

  unsigned int max_instr_check = 1024;

  if (old_frame->pc == 0 || old_frame->sp == 0 || old_frame->ra == 0)
    return -9;

  if (!userspace) {
    max_instr_check = 128;
  }

  for (addr = (unsigned long *) new_frame.pc; ((unsigned long) addr
      + max_instr_check > new_frame.pc) && (!ra_offset || !stack_size); --addr) {
    unsigned long var = 0;

    GET_VALUE(var, addr);

    {
      unsigned short instr = var >> 16;
      short imm = var & 0xffff;

      switch (instr) {
      case 0x27bd: // addiu sp, imm
        stack_size = abs(imm);
        break;
      case 0xafbf: // sw ra, imm
        ra_offset = imm;
        break;
      case 0x3c1c: // lui gp
      case 0x03e0: // jr ra
        goto __out_of_loop;
      default:
        break;
      }
    }
  }

  __out_of_loop:

  if (ra_offset) {
    new_frame.ra = old_frame->sp + ra_offset;
    GET_VALUE(new_frame.ra, (unsigned long*) new_frame.ra);
  }

  if (stack_size) {
    new_frame.sp = old_frame->sp + stack_size;
    GET_VALUE(new_frame.sp, (unsigned long*) new_frame.sp);
  }

  if (!ra_offset || !stack_size) {
    return -1;
  }

  if (new_frame.sp > old_frame->sp) {
    return -2;
  }

  new_frame.pc = old_frame->ra;
  *old_frame = new_frame;

  oprofile_add_trace(new_frame.ra);

  return 0;
}

#ifdef OP_NICE_HACK
struct oprofile_cpu_buffer {
  unsigned long buffer_size;
  struct task_struct *last_task;
  int last_is_kernel;
  int tracing;
  unsigned long sample_received;
  unsigned long sample_lost_overflow;
  unsigned long backtrace_aborted;
  unsigned long sample_invalid_eip;
  int cpu;
  struct delayed_work work;
};
#endif

void notrace op_mips_backtrace(struct pt_regs * const regs, unsigned int depth) {
  struct stackframe frame;
  unsigned long high, low;

#ifdef OP_NICE_HACK
  struct oprofile_cpu_buffer *cpu_buf =
  (struct oprofile_cpu_buffer *) op_custom;
  if (!(cpu_buf->last_task) || cpu_buf->last_task->prio == 120)
  return;
#endif

  frame.sp = regs->regs[29];
  frame.ra = regs->regs[31];
  frame.pc = regs->cp0_epc;

  userspace = user_mode(regs);
  low = frame.sp;
  high = ALIGN(low, THREAD_SIZE) + THREAD_SIZE;

  while (depth-- && unwind_frame_and_report(&frame) == 0) {
    if (frame.sp < low || frame.sp > high)
      break;
  }
}
