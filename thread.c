//
// Created by 안재찬 on 2019-05-28.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
#include "proc.h"
#include "gvar.h"
#include "thread.h"


void thread_init_config(thread_config_t *config) {
  config->sp = 0;
  config->valid = 0;
  config->state = FREE;
}

thread_config_t *thread_alloc_config(struct proc *p) {
  thread_config_t *config = NULL;
  int i;
  acquire(&ptable.lock);
  for (i = 0; i < MAX_THREADS; ++i) {
    if (p->thread_pool[i].state == FREE) {
      p->thread_pool[i].state = WORKING;
      config = &p->thread_pool[i];
      break;
    }
  }
  release(&ptable.lock);
  return config;
}

int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg) {
  struct proc *np;
  struct proc *curproc = myproc();
  uint sz, sp;
  uint ustack[3];
  int i;
  thread_config_t *config;

  if ((np = allocproc()) == 0) {
    return -1;
  }

  // Main thread
  if (curproc->is_thread) {
    curproc = curproc->parent;
  }

  // Allocate stack
  config = thread_alloc_config(curproc);
  if (config == NULL) {
    goto tbad;
  }

  if (!config->valid) {
    sz = PGROUNDUP(curproc->sz);
    if ((sz = allocuvm(curproc->pgdir, sz, sz + 2 * PGSIZE)) == 0)
      goto tbad;
    clearpteu(curproc->pgdir, (char *) (sz - 2 * PGSIZE));

    curproc->sz = config->sp = sp = sz; // Set the process's new size
    config->valid = 1;
  } else {
    sp = config->sp;
  }

  memset((void *) (sp - PGSIZE), 0, PGSIZE);

  // Set up ustack
  ustack[0] = 0xffffffff;
  ustack[1] = 1;
  ustack[2] = (uint) arg;

  sp -= sizeof(ustack);

  if (copyout(curproc->pgdir, sp, ustack, sizeof(ustack)) < 0) {
    goto tbad;
  }

  // Enqueue into thread queue
  if (enqueue(&curproc->threads, np) < 0) {
    goto tbad;
  }
  np->is_thread = 1;

  // Share page directory
  np->pgdir = curproc->pgdir;
  np->parent = curproc;

  // Trapframe
  *np->tf = *curproc->tf;
  np->tf->esp = sp; // Set esp to user stack pointer of the new thread
  np->tf->eip = (uint) start_routine; // Set eip to the function

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  *thread = np->pid;

  return 0;

tbad:
  kfree(np->kstack);
  np->kstack = 0;
  np->state = UNUSED;
  return -1;
}

int thread_join(thread_t thread, void **retval) {
  struct proc *p = NULL;
  int pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;) {
    // Find the thread with tid
    // if thread is killed or thread does not exist, return -1;
    // if it's zombie, clear out its variables and set the thread_config's state to free
    p = queue_fetch(&curproc->threads, thread);

    // No point waiting if we don't have any children.
    if (p == NULL || curproc->killed) {
      release(&ptable.lock);
      return -1;
    }

    if (p->state == ZOMBIE) {
      // Found one.
      pid = p->pid;
      kfree(p->kstack);
      p->kstack = 0;
      freevm(p->pgdir);
      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      p->state = UNUSED;

      p->thread_config->state = FREE;
      p->thread_config = NULL;

      release(&ptable.lock);
      return pid;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
  return 0;
}

void thread_exit(void *retval) {
  struct proc *tp = myproc();
  struct proc *curproc = tp->parent;
  int fd;

  if (!tp->is_thread) {
    exit();
  }

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++) {
    if (tp->ofile[fd]) {
      fileclose(tp->ofile[fd]);
      tp->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(tp->cwd);
  end_op();
  tp->cwd = 0;

  acquire(&ptable.lock);
  queue_delete(&curproc->threads, tp);

  tp->state = ZOMBIE;

  // Main thread might be sleeping in thread_join().
  wakeup(curproc);
  sched();
}