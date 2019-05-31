#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
#include "proc.h"
#include "gvar.h"


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

void
printproc(struct proc *p) {
  if (p == NULL) {
    return;
  }

  if (p->type == STRIDE || p->type == DEFAULT) {
    cprintf("[%s] (%d): pass: %d, stride: %d, state: %d %s\n", p->type == STRIDE ? "STRIDE" : "DEFAULT", p->pid,
            p->stride_config.pass,
            p->stride_config.stride,
            p->state, p == mlfqproc ? "MLFQ" : "");
  } else {
    cprintf("[MLFQ] (%d): level: %d, quantum: %d, allotment: %d\n", p->pid, p->mlfq_config.level,
            p->mlfq_config.quantum, p->mlfq_config.allotment);
  }
}

void printqueue(queue_t *q) {
  int i;
  for (i = 0; i < q->size; ++i) {
    cprintf("(%d - %d) -> ", q->parr[(q->front + i) % QUEUE_SIZE]->pid,
            q->parr[(q->front + i) % QUEUE_SIZE]->parent->pid);
  }
  cprintf("\n");
}

void printmlfq(mlfq_t *mlfq) {
  int i;
  for (i = 0; i < MLFQ_LEVELS; ++i) {
    cprintf("[LEVEL %d]: ", i);
    printqueue(&mlfq->queue[i]);
  }
  cprintf("\n");
}

void printheap(heap_t *heap) {
  int i;

  cprintf("-----Heap(%d)-----\n", heap->sz);
  for (i = 0; i < heap->sz; ++i) {
    cprintf("[%d] ", i);
    printproc(heap->parr[i]);
    if (heap->parr[i]->threads.size != 0) {
      cprintf("\t");
      printqueue(&heap->parr[i]->threads);
    }
  }
  cprintf("\n");
}

void printsched(void) {
  printheap(&pheap);
  printmlfq(&pmlfq);
}

void
pinit(void) {
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void) {
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
struct proc *
allocproc(void) {
  struct proc *p;
  char *sp;
  int i;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *) sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *) sp = (uint) trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *) sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint) forkret;

  // Set scheduling variables
  p->type = DEFAULT;
  stride_init_config(&p->stride_config);
  mlfq_init_config(&p->mlfq_config);

  for (i = 0; i < MAX_THREADS; ++i) {
    thread_init_config(&p->thread_pool[i]);
  }

  p->is_thread = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void) {
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int) _binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}


void strideinit(void) {
  // Init pheap for stride scheduling
  heap_init(&pheap);

  acquire(&ptable.lock);

  heap_push(&pheap, initproc);

  release(&ptable.lock);
}

void mlfqinit(void) {
  mlfqproc = allocproc(); // Stays at the state EMBRYO so that it's occupied.
  mlfqproc->parent = initproc;
  safestrcpy(mlfqproc->name, "mlfqproc", sizeof(mlfqproc->name));

  pheap.share = MLFQ_SHARE;
  stride_set_share(mlfqproc, MLFQ_SHARE);

  mlfq_init(&pmlfq);

  acquire(&ptable.lock);

  heap_push(&pheap, mlfqproc);
  stride_rearrange(&pheap);

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n) {
  uint sz;
  struct proc *curproc = myproc();

  if (curproc->is_thread) {
    curproc = curproc->parent;
  }

  sz = curproc->sz;
  if (n > 0) {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if (n < 0) {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;



  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void) {
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0) {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

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

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void) {
  struct proc *curproc = myproc();
  struct proc *p;
  int fd, i;

  if (curproc == initproc)
    panic("init exiting");

  if (curproc->is_thread) {
    curproc = curproc->parent;
  }

  // Close all open files in threads.
  for (i = 0; i < curproc->threads.size; ++i) {
    p = curproc->threads.parr[(curproc->threads.front + i) % QUEUE_SIZE];
    for (fd = 0; fd < NOFILE; fd++) {
      if (p->ofile[fd]) {
        fileclose(p->ofile[fd]);
        p->ofile[fd] = 0;
      }
    }

    begin_op();
    iput(p->cwd);
    end_op();
    p->cwd = 0;
  }

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++) {
    if (curproc->ofile[fd]) {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  while ((p = queue_front(&curproc->threads)) != NULL) {
    thread_clear(p);

    // Delete from parent thread queue
    dequeue(&curproc->threads);

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
  }

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == curproc) {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  if (curproc->type == MLFQ) {
    if (mlfq_delete(&pmlfq, curproc) == 0) {
      mlfq_init_config(&curproc->mlfq_config);
    }
  } else {
    heap_delete(&pheap, curproc);

    // Rearrange tickets due to total ticket count change
    pheap.share -= curproc->stride_config.share; // DEFAULT has 0 for share
    curproc->stride_config.share = 0;
    stride_rearrange(&pheap);
  }

  queue_clear(&curproc->threads);

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void) {
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;) {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->parent != curproc)
        continue;
      havekids = 1;
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
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed) {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;) {
    sti();

    acquire(&ptable.lock);

    p = heap_peek(&pheap);

    if (p == NULL) {
      goto release;
    }

    if (p == mlfqproc) {
      // Increment pass by stride.
      heap_set_pass(&pheap, 0, p->stride_config.pass + p->stride_config.stride);

      // Dequeue one from the pmlfq
      p = mlfq_pop(&pmlfq);

      // MLFQ is empty
      if (p == NULL) {
        goto release;
      }

      if (p->state != RUNNABLE) {
        mlfq_downlevel(&pmlfq, p);
        goto release;
      }

      if (p->mlfq_config.allotment >= pmlfq.queue[p->mlfq_config.level].allotment) {
        // Go down level
        mlfq_downlevel(&pmlfq, p);
      } else {
        // Round Robin
        mlfq_round_robin(&pmlfq, p);
      }
    } else {
      if (p->state != RUNNABLE) {
        goto release;
      }

      // Increment pass by stride.
      heap_set_pass(&pheap, 0, p->stride_config.pass + p->stride_config.stride);
    }

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;

    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
release:
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void) {
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void) {
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void) {
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock) {  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  if (p == mlfqproc) {
    panic("MLFQ  sleeping");
  }

  if (p->type != MLFQ) {
    heap_set_pass(&pheap, heap_search(&pheap, p), INFINITE);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock) {  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
void
wakeup1(void *chan) {
  struct proc *p;
  int idx;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan) {
      // Set pass to match current runnable processes
      if ((idx = heap_search(&pheap, p)) != -1) {
        heap_set_pass(&pheap, idx, heap_peek_pass(&pheap));
      }

      p->state = RUNNABLE;
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid) {
  struct proc *p;
  int idx;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING) {
        // Set pass before re-entering the heap
        idx = heap_search(&pheap, p);
        if (idx != -1) {
          heap_set_pass(&pheap, idx, heap_peek_pass(&pheap));
        }

        p->state = RUNNABLE;
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void) {
  static char *states[] = {
      [UNUSED]    "unused",
      [EMBRYO]    "embryo",
      [SLEEPING]  "sleep ",
      [RUNNABLE]  "runble",
      [RUNNING]   "run   ",
      [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING) {
      getcallerpcs((uint *) p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int cpu_share(int x) {
  struct proc *p = myproc();
  int share;

  if (x < 0) {
    return -1;
  }

  share = pheap.share;

  switch (p->type) {
    case STRIDE:
      // x includes itself
      x -= p->stride_config.share;
      break;
    case MLFQ:
      // Delete process from MLFQ
      if (mlfq_delete(&pmlfq, p) == 0) {
        mlfq_init_config(&p->mlfq_config);
      }

      // Insert process to STRIDE scheduler
      p->stride_config.pass = INFINITE;
      heap_push(&pheap, p);
      break;
    case DEFAULT:
      break;
    default:
      return -1;
  }

  if (share + x > MAX_STRIDE_SHARE + MLFQ_SHARE) {
    return -1;
  }

  acquire(&ptable.lock);

  pheap.share = share + x;
  stride_set_share(p, x);
  stride_rearrange(&pheap);

  release(&ptable.lock);

  return 0;
}

int run_MLFQ(void) {
  struct proc *p = myproc();

  if (p == mlfqproc) {
    return -1;
  }

  acquire(&ptable.lock);

  switch (p->type) {
    case DEFAULT:
    case STRIDE:
      // Delete process from STRIDE scheduler
      heap_delete(&pheap, p);

      // Rearrange tickets due to total ticket count change
      pheap.share -= p->stride_config.share; // DEFAULT has 0 for share
      p->stride_config.share = 0;
      stride_rearrange(&pheap);

      // Insert process to MLFQ
      mlfq_init_config(&p->mlfq_config);
      mlfq_push(&pmlfq, p);
    case MLFQ: // Do nothing
      break;
    default:
      release(&ptable.lock);
      return -1;
  }
  release(&ptable.lock);

  return 0;
}

int getlev(void) {
  return myproc()->type == MLFQ ? myproc()->mlfq_config.level : -1;
}

void swapproc(struct proc **p1, struct proc **p2) {
  struct proc *tmp = *p1;
  *p1 = *p2;
  *p2 = tmp;
}
