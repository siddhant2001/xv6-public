#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "fcntl.h"


struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;


int queues[5][NPROC];
int ptrs[5][2] = {
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0},
  {0, 0}
};

void print_timeVpid(void){
  acquire(&ptable.lock);
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid < 3)
      continue;
    if(p->state == RUNNING){
      cprintf("\nYEEET: %d %d RUNNING ", p->pid, ticks);
    }
    else if(p->state == SLEEPING){
      cprintf("\nYEEET: %d %d SLEEPING ", p->pid, ticks);
    }
    else if(p->state == RUNNABLE){
      cprintf("\nYEEET: %d %d RUNNABLE ", p->pid, ticks);
    }
  }
  release(&ptable.lock);
}

void enqueue(int q_num, int pid){
  queues[q_num][ptrs[q_num][1]] = pid;
  ptrs[q_num][1] = (ptrs[q_num][1]+1)%NPROC;
}

void dequeue(int q_num){
  ptrs[q_num][0] = (ptrs[q_num][0]+1)%NPROC;
}

void demote_queue(int old_q, int new_q, struct proc *p){
  if(new_q>4)
    new_q=4;
  if(new_q<0)
    new_q=0;

  int start_shifting=0, i, q=p->cur_q;

  for(i=ptrs[q][0]; i!=ptrs[q][1]; i = (1+i)%NPROC){
    if(queues[q][i] == p->pid){
      start_shifting=1;
      queues[q][i]=0;
      continue;
    }

    if(start_shifting){
      queues[q][(i-1+NPROC)%NPROC] = queues[q][i];
    }

    // cprintf("infinite loop?\n");
  }

  ptrs[q][1] = (ptrs[q][1]-1+NPROC)%NPROC;

  p->qwtime=0;
  p->ts_rtime=0;
  p->cur_q=new_q;

  int ts=1;

  for(i=0; i<new_q; i++)
    ts *= 2;

  p->time_slice = ts;
  p->qwtime=0;

  if(new_q<=4)
    enqueue(new_q, p->pid);

  if(LOGS && p->pid > 2){
    // cprintf("%d %d\n", p->cur_q, p->time_slice);
    cprintf("\nYEEET: %d %d %d.9 ", p->pid, old_q, ticks-1);
    cprintf("\nYEEET: %d %d %d ", p->pid, p->cur_q, ticks);
  }
  
}

void increment_wait(){
  struct proc *p;

  acquire(&ptable.lock);  

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state==RUNNABLE){
      p->wtime+=1;
      p->stime+=1;
      p->qwtime+=1;

      if(SCHEDULER==MLFQ){
        if(p->qwtime > 50 && p->cur_q != 0){
          // cprintf("Promotion\n");
          demote_queue(p->cur_q, p->cur_q-1, p);
        }
      }
    }
  }

  for (int i = 0; i < ncpu; ++i) {
    p = cpus[i].proc;
    if(p==0 || p->state != RUNNING)
      continue;
    p->rtime++;
    p->ts_rtime++;
    p->qticks[p->cur_q]++;
  }

  release(&ptable.lock);  
}


static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
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
struct proc*
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
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  // nextpid++;
  //ASSIGNMENT TASK 1
  acquire(&tickslock);
  p->ctime = ticks;
  p->rtime = 0;
  p->ts_rtime=0;
  release(&tickslock);

  p->n_run=0;
  p->priority=60;
  p->wtime=0;
  p->qwtime=0;
  p->stime=0;
  p->to_add=1;  
  p->cur_q=0;
  p->time_slice=1;
  for(int i=0; i<5; i++)
    p->qticks[i]=0;

  p->qticks[0]=0;

  release(&ptable.lock);


  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
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

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
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
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
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

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int i;
  int q=curproc->cur_q;
  int fd, qt;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;


  acquire(&tickslock);
  curproc->etime = ticks;
  release(&tickslock);

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.


  // for(qt=0; qt<5; qt++){
  //   cprintf("%d: ", qt);
  //   for(i=ptrs[qt][0]; i!=ptrs[qt][1]; i = (1+i)%NPROC){
  //     cprintf("%d ", queues[qt][i]);
  //   }
  //   cprintf("\n");
  // }


  // REMOVE PROCESS PID FROM QUEUE
  int start_shifting=0;

  for(i=ptrs[q][0]; i!=ptrs[q][1]; i = (1+i)%NPROC){
    if(queues[q][i] == curproc->pid){
      start_shifting=1;
      queues[q][i]=0;
      continue;
    }

    if(start_shifting){
      queues[q][(i-1+NPROC)%NPROC] = queues[q][i];
    }

    // cprintf("infinite loop?\n");
  }

  ptrs[q][1] = (ptrs[q][1]-1+NPROC)%NPROC;

  // cprintf("Exited a process\n");

  // COULD'VE been done using deque too fml


  // ASSIGNMENT TASK 1
  // Assumes curproc->state = ZOMBIE means end.
  curproc->state = ZOMBIE;
  

  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
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

        p->ctime = 0;
        p->rtime = 0;
        p->etime = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}


// ASSIGNMENT TASK 1
int 
waitx(int* wtime, int* rtime){
  
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        *wtime = p->wtime; // change
        *rtime = p->rtime;
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

        p->ctime = 0;
        p->rtime = 0;
        p->etime = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }

}




void
scheduler_rr(void){
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

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
    }
    release(&ptable.lock);

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
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  uint min_ctime;
  uint min_priori;
  c->proc = 0;
  int qnum, sel_pid, to_break, qt, i;
  int ticks_done=-1;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    switch(SCHEDULER){
      case FCFS:
        // ALL PROCESSES MIGHT BE RUNNING ON CPU 1
        min_ctime = ticks + 100;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->state != RUNNABLE)
            continue;
          
          if(p->ctime < min_ctime){
            min_ctime = p->ctime;
          }
        }

        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->state != RUNNABLE)
            continue;
          
          if(p->ctime == min_ctime){
            c->proc = p;
            switchuvm(p);
            p->state = RUNNING;
            p->n_run+=1;
            swtch(&(c->scheduler), p->context);
            switchkvm();
            c->proc = 0;
            break;
          }
        }
        break;

      case PBS:
        min_priori = 100;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->state != RUNNABLE)
            continue;
          
          if(p->priority < min_priori){
            min_priori = p->priority;
          }
        }

        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->state != RUNNABLE)
            continue;
          
          if(p->priority == min_priori){
            c->proc = p;
            switchuvm(p);
            p->state = RUNNING;
            p->n_run+=1;
            swtch(&(c->scheduler), p->context);
            switchkvm();
            c->proc = 0;
            break;
          }
        }
        break;
      
      case MLFQ:
        // Add new processes to queue
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->to_add){
            enqueue(0, p->pid);
            if(LOGS && SCHEDULER==MLFQ && p->pid>2){
              cprintf("\nYEEET: %d %d %d ", p->pid, p->cur_q, ticks);
            }
            p->to_add=0;
          }
        }

        // select highest priority process
        to_break=0;
        sel_pid=-1;
        for(qnum=0; qnum<5; qnum++){
          for(int i=ptrs[qnum][0]; i!=ptrs[qnum][1]; i=(i+1)%NPROC){
            sel_pid=queues[qnum][i];

            for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
              if(p->pid==sel_pid && p->state == RUNNABLE){
                to_break=1;
                break;
              }
            }

            if(to_break)
              break; 

          }

          if(to_break)
            break;          
        }


        // if(sel_pid==3){
        //   cprintf("sel pid: %d\n", sel_pid);      
        // }

        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->state != RUNNABLE || p->pid != sel_pid)
            continue;
          c->proc = p;
          switchuvm(p);
          p->state = RUNNING;
          p->ts_rtime = 0;
          p->n_run+=1;
          swtch(&(c->scheduler), p->context);
          switchkvm();
          c->proc = 0;
        }
        break;

      case RR:
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->state != RUNNABLE)
            continue;
          c->proc = p;
          switchuvm(p);
          p->state = RUNNING;
          p->n_run+=1;
          swtch(&(c->scheduler), p->context);
          switchkvm();
          c->proc = 0;
        }
        break;
      
      default:
        break;
    }


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
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  // cprintf("Switching to scheduler\n");

  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  // cprintf("%s Yield hua hai!\n", myproc()->name);
  if(SCHEDULER==MLFQ)
    demote_queue(myproc()->cur_q, myproc()->cur_q+1, myproc());
  sched();
  release(&ptable.lock);
}


int
set_priority(int new_priority, int pid){
  
  int to_yield=0, old_priority=-1;
  struct proc* p;

  acquire(&ptable.lock);  //DOC: yieldlock
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->state != UNUSED){
      // dnf=0;
      old_priority = p->priority;
      p->priority = new_priority;

      if(new_priority < old_priority)
        to_yield=1;
    }
  }
  release(&ptable.lock);
  if(to_yield==1){
    yield();
    // cprintf("Yielded\n");
  }
  return old_priority;
}


// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
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
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  if(SCHEDULER==MLFQ)
    demote_queue(myproc()->cur_q, myproc()->cur_q, myproc());

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
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
procdump(void)
{
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void
ps(void){
  static char *states[] = {
  [UNUSED]    " unused ",
  [EMBRYO]    " embryo ",
  [SLEEPING]  "sleeping",
  [RUNNABLE]  "runnable",
  [RUNNING]   "running ",
  [ZOMBIE]    " zombie "
  };
  struct proc *p;
  char *state;

  cprintf("PID\tPRIORITY\tSTATE\t\trtime\twtime\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");
  int x=1;
  for(int i=0; i<=1000000000; i++){
    // cprintf("\t|");
    x *= ticks; 
  }
  cprintf("%d\n", x);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d\t%d\t\t%s\t%d\t%d\t%d\t%d", p->pid, p->priority, state, p->rtime, p->qwtime, p->n_run, p->cur_q);
    for(int i=0; i<5; i++){
      cprintf("\t%d", p->qticks[i]);
    }
    cprintf("\n");
  }
}


void
ps1(void){
  static char *states[] = {
  [UNUSED]    " unused ",
  [EMBRYO]    " embryo ",
  [SLEEPING]  "sleeping",
  [RUNNABLE]  "runnable",
  [RUNNING]   "running ",
  [ZOMBIE]    " zombie "
  };
  struct proc *p;
  char *state;

  cprintf("PID\tPRIORITY\tSTATE\t\trtime\twtime\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");
  int x=1;
  for(int i=0; i<=1000000000; i++){
    // cprintf("\t|");
    x *= ticks; 
  }
  cprintf("%d\n", x);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d\t%d\t\t%s\t%d\t%d\t%d\t%d", p->pid, p->priority, state, p->rtime, p->qwtime, p->n_run, p->cur_q);
    for(int i=0; i<5; i++){
      cprintf("\t%d", p->qticks[i]);
    }
    cprintf("\n");
  }
}
