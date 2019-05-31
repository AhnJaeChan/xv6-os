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

int tcreate(thread_t *thread, void *(*start_routine)(void *), void *arg) {
  struct proc *np;
  struct proc *curproc = myproc();
  uint sp;
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
    curproc->sz = PGROUNDUP(curproc->sz);
    if ((curproc->sz = allocuvm(curproc->pgdir, curproc->sz, curproc->sz + 2 * PGSIZE)) == 0)
      goto tbad;
    clearpteu(curproc->pgdir, (char *) (curproc->sz - 2 * PGSIZE));

    config->sp = sp = curproc->sz; // Set the process's new size
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
  np->sz = curproc->sz;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  *thread = np->pid;

  acquire(&ptable.lock);

  if (curproc->type == MLFQ) {
    mlfq_push(&pmlfq, np);
  } else {
    np->stride_config.pass = heap_peek_pass(&pheap);
    heap_push(&pheap, np);

    // Rearrange tickets for shared portions.
    if (pheap.share != 0) {
      stride_rearrange(&pheap);
    }
  }

  np->state = RUNNABLE;

  release(&ptable.lock);

  return 0;

tbad:
  kfree(np->kstack);
  np->kstack = 0;
  np->state = UNUSED;
  return -1;
}

int tjoin(thread_t thread, void **retval) {
  struct proc *p = NULL;
  int pid;
  struct proc *parent = myproc();

  if (parent->is_thread) {
    parent = parent->parent;
  }

  acquire(&ptable.lock);
  for (;;) {
    // Find the thread with tid
    // if thread is killed or thread does not exist, return -1;
    // if it's zombie, clear out its variables and set the thread_config's state to free
    p = queue_fetch(&parent->threads, thread);

    // Error, we don't have any matching thread.
    if (p == NULL) {
      release(&ptable.lock);
      return -1;
    }

    if (p->state == ZOMBIE) {
      // Found one.
      pid = thread_clear(p);

      release(&ptable.lock);
      return pid;
    }

    if (parent->killed) {
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(parent, &ptable.lock);  //DOC: wait-sleep
  }
}

void texit(void *retval) {
  struct proc *thread = myproc();
  struct proc *parent;
  int fd;

  cprintf("t");

  if (!thread->is_thread) {
    exit();
  }

  parent = thread->parent;

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++) {
    if (thread->ofile[fd]) {
      fileclose(thread->ofile[fd]);
      thread->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(thread->cwd);
  end_op();
  thread->cwd = 0;

  acquire(&ptable.lock);

  // Delete from parent thread queue
  queue_delete(&parent->threads, thread);

  if (thread->type == MLFQ) {
    if (mlfq_delete(&pmlfq, thread) == 0) {
      mlfq_init_config(&thread->mlfq_config);
    }
  } else {
    heap_delete(&pheap, thread);

    // Rearrange tickets due to total ticket count change
    pheap.share -= thread->stride_config.share; // DEFAULT has 0 for share
    thread->stride_config.share = 0;
    stride_rearrange(&pheap);
  }

  stride_init_config(&thread->stride_config);
  mlfq_init_config(&thread->mlfq_config);

  thread->state = ZOMBIE;

  // Main thread might be sleeping in thread_join().
  wakeup1(parent);

  sched();
}

int thread_clear(struct proc *thread) {
  int pid;

  // Found one.
  pid = thread->pid;
  kfree(thread->kstack);
  thread->kstack = 0;
  thread->pid = 0;
  thread->parent = 0;
  thread->name[0] = 0;
  thread->killed = 0;
  thread->state = UNUSED;

  stride_init_config(&thread->stride_config);
  mlfq_init_config(&thread->mlfq_config);

  thread->thread_config->state = FREE;
  thread->thread_config = NULL;

  return pid;
}
