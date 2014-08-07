/* Loriah Pope
   Operating Systems Fall 2013 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "hardware.h"
#include "drivers.h"
#include "kernel.h"

typedef enum { RUNNING, READY, BLOCKED , UNINITIALIZED } PROCESS_STATE;

typedef struct process_table_entry {
  PROCESS_STATE state;
  int CPU_time_used;
  int quantum_start_time;
} PROCESS_TABLE_ENTRY;

extern PROCESS_TABLE_ENTRY process_table[];

/* A quantum is 40 ms */
#define QUANTUM 40

int num_processes;

PROCESS_TABLE_ENTRY process_table[MAX_NUMBER_OF_PROCESSES];    

typedef struct ready_queue_elt {
  struct ready_queue_elt *next;
  PID_type pid;
} READY_QUEUE_ELT;


READY_QUEUE_ELT *ready_queue_head = NULL;  /* head of the event queue */
READY_QUEUE_ELT *ready_queue_tail = NULL;


/*places current PID at the end of the ready queue*/
void queue_ready_process(PID_type pid)
{
  READY_QUEUE_ELT *p = (READY_QUEUE_ELT *) malloc(sizeof(READY_QUEUE_ELT));
  p->pid = pid;
  p->next = NULL;
  if (ready_queue_tail == NULL) 
    if (ready_queue_head == NULL) {
      ready_queue_head = ready_queue_tail = p;
      p->next = NULL;
    }
    else {
      printf("Error: ready queue tail is NULL but ready_queue_head is not\n");
      exit(1);
    }
  else {
    ready_queue_tail->next = p;
    ready_queue_tail = p;
  }
}

/*removes and returns the PID at the front of the ready queue*/
PID_type dequeue_ready_process()
{
  if (ready_queue_head == NULL)
    if (ready_queue_tail == NULL)
      return IDLE_PROCESS;        // indicates no active process is ready
    else {
      printf("Error: ready_queue_head is NULL but ready_queue_tail is not\n");
      exit(1);
    }
  else {      
    READY_QUEUE_ELT *p = ready_queue_head;
    ready_queue_head = ready_queue_head->next;
    if (ready_queue_head == NULL)
      ready_queue_tail = NULL;
    return p->pid;
  }
}

/*chooses to run the next non-idle process on the ready queue*/
void choose_next_process(){
  current_pid = dequeue_ready_process();

  if(current_pid != IDLE_PROCESS){
    process_table[current_pid].state = RUNNING;
    process_table[current_pid].quantum_start_time = clock;
    printf("Time: %d: Process %d runs\n", clock, current_pid);
  }
  else
    printf("Time %d: Processor is idle.\n", clock);
}

/*R1 contains information about the type of trap thrown*/
void handle_trap(){
  printf("IN HANDLE_TRAP. Clock = %d Trap = %d\n", clock, R1);
  switch(R1){
  case DISK_READ:
    /*blocks the current process to issue a DISK_READ request, updates the total CPU time of the blocked process, & run the next available process.*/
      process_table[current_pid].state = BLOCKED;
      disk_read_req(current_pid, R2); 
      process_table[current_pid].CPU_time_used += (clock - process_table[current_pid].quantum_start_time);
      choose_next_process();
    break;

    /*issues a non-blocking DISK_WRITE request*/
  case DISK_WRITE:
      disk_write_req(current_pid);
    break;

    /*blocks the current process to issue a KEYBOARD_READ request, updates the total CPU time of the blocked process, & run the next available process.*/
  case KEYBOARD_READ:
      process_table[current_pid].state = BLOCKED;
      keyboard_read_req(current_pid);
      process_table[current_pid].CPU_time_used += (clock - process_table[current_pid].quantum_start_time);
      choose_next_process();
    break;

    /*creates a child process from the PID in R2, adds it to the queue, and increments the total number of processes.*/
  case FORK_PROGRAM:
    if(current_pid != IDLE_PROCESS){
      process_table[R2].state = READY;
      queue_ready_process(R2);
      process_table[R2].CPU_time_used = 0;
      printf("Time %d: Creating process entry for pid %d\n", clock, R2);
      num_processes ++;
    }
    break;

    /*ends the current process, decrements the total processes, and chooses the next process to run. PID of ending process is in R2.*/
  case END_PROGRAM:
      process_table[R2].state = UNINITIALIZED;
      process_table[R2].CPU_time_used += (clock -  process_table[current_pid].quantum_start_time);
      printf("Time %d: Process %d exits. Total CPU time = %d\n", clock, R2, process_table[R2].CPU_time_used);
      num_processes--;
      if(num_processes == 0){
	printf("--No more processes to execute--\n");
	exit(0);
      }
	choose_next_process();
    break;
  }
}

/*if a process has used its quantum and it is not idle, put it back on the ready queue, update its CPU time, & begin running the next available process.*/
handle_clock_interrupt(){
    if(clock - process_table[current_pid].quantum_start_time >= QUANTUM){
      if(current_pid != IDLE_PROCESS){
	process_table[current_pid].state = READY;
	queue_ready_process(current_pid);
	process_table[current_pid].CPU_time_used += (clock -  process_table[current_pid].quantum_start_time);
	choose_next_process();
      }
    }
  }

/*after a DISK_READ is issued, changes the state of the blocked process back to ready and adds it to the queue*/
void handle_disk_interrupt(){
    process_table[R1].state = READY;
    queue_ready_process(R1);
    printf("Time %d: Handled DISK_INTERRUPT for pid %d\n", clock, R1);
    if(current_pid == IDLE_PROCESS){
      choose_next_process();
    }
}

/*after a DISK_READ is issued, changes the state of the blocked process back to ready and adds it to the queue*/
void handle_keyboard_interrupt(){
    process_table[R1].state = READY;
    queue_ready_process(R1);
    printf("Time %d: Handled KEYBOARD_INTERRUPT for pid %d\n", clock, R1);
    if(current_pid == IDLE_PROCESS){
      choose_next_process();
    }
}

/*begin running the first process*/
void initialize_kernel(){
  num_processes = 1;
  process_table[0].state = RUNNING;
  process_table[0].quantum_start_time = 0;
  process_table[0].CPU_time_used = 0;
  int i = 1;
  for(i; i < MAX_NUMBER_OF_PROCESSES-1; i++){
    process_table[i].state = UNINITIALIZED;
  }
  INTERRUPT_TABLE[TRAP] = (FN_TYPE) handle_trap;
  INTERRUPT_TABLE[CLOCK_INTERRUPT] = (FN_TYPE) handle_clock_interrupt;
  INTERRUPT_TABLE[DISK_INTERRUPT] = (FN_TYPE) handle_disk_interrupt;
  INTERRUPT_TABLE[KEYBOARD_INTERRUPT] = (FN_TYPE) handle_keyboard_interrupt;
}


