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
  config->thread = NULL;
  config->valid = 0;
  config->state = FREE;
  config->retval = NULL;
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

struct proc *thread_fetch(struct proc *parent, thread_t thread) {
  int i;

  for (i = 0; i < MAX_THREADS; ++i) {
    if (parent->thread_pool[i].thread->pid == thread) {
      return parent->thread_pool[i].thread;
    }
  }

  return NULL;
}

int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg) {
  struct proc *np;
  struct proc *parent = myproc();
  uint sz, sp;
  uint ustack[3];
  int i;
  thread_config_t *config;

  if ((np = allocproc()) == 0) {
    printproc(np);
    return -1;
  }

  // Main thread
  if (parent->is_thread) {
    parent = parent->parent;
  }

  // Set thread config
  config = thread_alloc_config(parent);
  if (config == NULL) {
    goto tbad;
  }

  // Allocate memory if there is no valid area in thread pool
  sz = parent->sz;
  if (!config->valid) {
    sz = PGROUNDUP(sz);
    if ((sz = allocuvm(parent->pgdir, sz, sz + 2 * PGSIZE)) == 0) {
      thread_init_config(config);
      goto tbad;
    }
    clearpteu(parent->pgdir, (char *) (parent->sz - 2 * PGSIZE));

    config->sp = sp = parent->sz; // Set the process's new size
    config->valid = 1;
  } else {
    sp = config->sp;
  }
  config->thread = np;
  np->thread_config = config;

  // Set up ustack
  ustack[0] = 0xffffffff;
  ustack[1] = (uint) arg;
  ustack[2] = 0;

  sp -= sizeof(ustack);

  if (copyout(parent->pgdir, sp, ustack, sizeof(ustack)) < 0) {
    goto tbad;
  }

  // Set up thread's property
  np->pgdir = parent->pgdir;
  np->parent = parent;
  np->is_thread = 1;
  np->sz = parent->sz = sz;

  // Trapframe
  *np->tf = *parent->tf;
  np->tf->esp = sp; // Set esp to user stack pointer of the new thread
  np->tf->eip = (uint) start_routine; // Set eip to the function
  np->sz = parent->sz;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (parent->ofile[i])
      np->ofile[i] = filedup(parent->ofile[i]);
  np->cwd = idup(parent->cwd);

  safestrcpy(np->name, parent->name, sizeof(parent->name));

  *thread = np->pid;

  acquire(&ptable.lock);

  // Schedule the thread like a process
  if (parent->type == MLFQ) {
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
  return -1;
}

int thread_join(thread_t thread, void **retval) {
  struct proc *p = NULL;
  struct proc *parent = myproc();

  if (parent->is_thread) {
    parent = parent->parent;
  }

  acquire(&ptable.lock);
  for (;;) {
    // Find the thread with tid
    // if thread is killed or thread does not exist, return -1;
    // if it's zombie, clear out its variables and set the thread_config's state to free
    p = thread_fetch(parent, thread);

    // Error, we don't have any matching thread.
    if (p == NULL) {
      release(&ptable.lock);
      return -1;
    }

    if (p->state == ZOMBIE) {
      if (retval != NULL) {
        *retval = p->thread_config->retval;
      }

      // Deschedule thread
      if (p->type == MLFQ) {
        if (mlfq_delete(&pmlfq, p) == 0) {
          mlfq_init_config(&p->mlfq_config);
        }
      } else {
        heap_delete(&pheap, p);

        // Rearrange tickets due to total ticket count change
        pheap.share -= p->stride_config.share; // DEFAULT has 0 for share
        p->stride_config.share = 0;
        stride_rearrange(&pheap);
      }

      stride_init_config(&p->stride_config);
      mlfq_init_config(&p->mlfq_config);

      // Clear thread
      thread_clear(p);

      release(&ptable.lock);
      return 0;
    }

    if (parent->killed) {
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(parent, &ptable.lock);  //DOC: wait-sleep
  }
}

void thread_exit(void *retval) {
  struct proc *thread = myproc();
  int fd;

  if (!thread->is_thread) {
    exit();
  }

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++) {
    if (thread->ofile[fd]) {
      fileclose(thread->ofile[fd]);
      thread->ofile[fd] = 0;
    }
  }

  printproc(thread);
  begin_op();
  iput(thread->cwd);
  end_op();
  thread->cwd = 0;

  thread->thread_config->retval = retval;

  acquire(&ptable.lock);

  thread->state = ZOMBIE;

  // Main thread might be sleeping in thread_join().
  wakeup1(thread->parent);

  sched();
}

int thread_clear(struct proc *thread) {
  int pid;

  thread->thread_config->thread = NULL;
  thread->thread_config->state = FREE;

  // Found one.
  pid = thread->pid;
  kfree(thread->kstack);
  thread->kstack = 0;
  thread->pid = 0;
  thread->parent = 0;
  thread->name[0] = 0;
  thread->killed = 0;
  thread->state = UNUSED;

  return pid;
}
