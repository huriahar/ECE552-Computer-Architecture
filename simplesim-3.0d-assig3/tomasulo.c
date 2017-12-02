
#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

#define INSTR_QUEUE_SIZE         10

#define RESERV_INT_SIZE    4
#define RESERV_FP_SIZE     2
#define FU_INT_SIZE        2
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     4
#define FU_FP_LATENCY      9


/* ECE552 Assignment 3 - BEGIN CODE */
#define NUM_INPUT_REG      3
#define NUM_OUTPUT_REG     2
#define LARGE_NUMBER       0x7FFFFFF
/* ECE552 Assignment 3 - END CODE */

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle)	\
  myfprintf(out, "%d: %s", cycle, str);		\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str);	\
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the last instruction fetched
static int fetch_index = 1;

/* ECE552 Assignment 3 - BEGIN CODE */
static int head_instr_q = 0;
static int tail_instr_q = 0;

static int total_insns = 0;

// Indicate how many FP/INT reservation stations are available
static int reservINTavail = RESERV_INT_SIZE;
static int reservFPavail = RESERV_FP_SIZE;

static int fuINTavail = FU_INT_SIZE;
static int fuFPavail = FU_FP_SIZE;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS STATUS*/
// 0: Waiting for dependency to clear out
// 1: Dependency solved. Waiting to be scheduled
// 2: Scheduled into FU
static short status_resINT[RESERV_INT_SIZE] = {0};
static short status_resFP[RESERV_FP_SIZE] = {0};

/* FUNCTIONAL UNIT STATUS */
// 0: Inst still executing in FU
// 1: Inst done executing. Waiting for CDB
// 2: Scheduled into CDB
static short status_fuINT[FU_INT_SIZE] = {0};
static short status_fuFP[FU_FP_SIZE] = {0};

//Counter for latency 
static int latency_fuINT[FU_INT_SIZE] = {0};
static int latency_fuFP[FU_FP_SIZE] = {0};


void print_stat(int cycle) {
  if (fetch_index >= 1) {

    int i = 0, j = 0;
    printf("Cycle: %d\n", cycle);
    printf("RS int available: %d, RS fp available: %d\n", reservINTavail, reservFPavail);
    printf("FU int available: %d, FU fp available: %d\n", fuINTavail, fuFPavail);

    printf("Head: %d\n", head_instr_q);  
    printf("Tail: %d\n", tail_instr_q);  
    printf("Insn Queue Size: %d\n", instr_queue_size);
    instruction_t *insn = NULL;
    for (; i < INSTR_QUEUE_SIZE; ++i) {
      insn = instr_queue[i];
      if (insn != NULL) {
        printf("%d: ", i);
        md_print_insn(insn->inst, insn->pc, stdout);
        printf("\n");
      }
      else {
        printf("Insn is NULL\n");
      }
    }
    printf("\n");

    printf("\nINT RS\n");
    for(i = 0; i < RESERV_INT_SIZE; ++i){
      insn = reservINT[i];
      if (insn != NULL) {
        printf("%d: ", i);
        print_tom_instr(insn);
        for (j = 0; j < 3; ++j) {
          if (insn->Q[j] != NULL) {
            printf("Q[%d]: ", j);
            md_print_insn(insn->Q[j]->inst, insn->Q[j]->pc, stdout);
            printf("\n"); 
          }
        }
      }
    }
    printf("\n"); 

    printf("\nFP RS\n");
    for (i = 0; i < RESERV_FP_SIZE; ++i) {
      insn = reservFP[i];
      if (insn != NULL) {
        printf("%d: ", i);
        print_tom_instr(insn);
        for (j = 0; j < 3; ++j) {
          if (insn->Q[j] != NULL) {
            printf("Q[%d]: ", j);
            md_print_insn(insn->Q[j]->inst, insn->Q[j]->pc, stdout);
          }
        }
      }
    }
    printf("\n"); 

    printf("\nINT FU\n");
    for (i = 0; i < FU_INT_SIZE; ++i) {
      insn = fuINT[i];
      if (insn != NULL) {
        printf("%d: ", i);
        print_tom_instr(insn);
        printf("Latency of fuINT[%d]: %d\n", i, latency_fuINT[i]);
      }
    }
    printf("\n");

    printf("\nFP FU\n");
    for (i = 0; i < FU_FP_SIZE; ++i) {
      insn = fuFP[i];
      if (insn != NULL) {
        printf("%d: ", i);
        print_tom_instr(insn);
        printf("Latency of fuFP[%d]: %d\n", i, latency_fuFP[i]);
      }
    }

    if (commonDataBus) {
      printf("Broadcasting instruction:\n");
      print_tom_instr(commonDataBus);
    }
    printf("%d %d %d %d %d %d %d\n", fetch_index >= total_insns,  instr_queue_size == 0, 
        reservINTavail == RESERV_INT_SIZE, reservFPavail == RESERV_FP_SIZE, fuINTavail == FU_INT_SIZE,
        fuFPavail == FU_FP_SIZE, commonDataBus == NULL);

    printf("---------------------------------------\n");
  }
}

void add_insn_to_iq(instruction_t *insn) {
  // Add a new instruction at the tail
  instr_queue[tail_instr_q] = insn;
  // Wrap around
  tail_instr_q = (tail_instr_q + 1) % INSTR_QUEUE_SIZE;
  instr_queue_size++;
}

void remove_head_of_iq() {
  head_instr_q = (head_instr_q + 1) % INSTR_QUEUE_SIZE;
  instr_queue_size--;
}

instruction_t *get_insn_at_head_iq() {
  return instr_queue[head_instr_q];
}

int get_available_idx(instruction_t **table, int size) {
  // Find first available idx for RS and FU
  int i = 0, ans = 0;
  for (; i < size; ++i) {
    if (table[i] == NULL) {
      ans = i;
      break;
    }
  }
  return ans;
}

/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      and if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {

  if (fetch_index > sim_insn && instr_queue_size == 0 && reservINTavail == RESERV_INT_SIZE &&
        reservFPavail == RESERV_FP_SIZE && fuINTavail == FU_INT_SIZE && fuFPavail == FU_FP_SIZE &&
        commonDataBus == NULL)
    return true;
  else
    return false;
}

// If CDB is available, updates it with current inst and sets CDB cycle. 
// else, CDB update was unsuccessful
bool update_cdb(instruction_t *insn, int current_cycle) {
  if(commonDataBus == NULL && insn->tom_cdb_cycle == 0){
    commonDataBus = insn;
    insn->tom_cdb_cycle = current_cycle;
    return true;
  }
  return false;
}


void clear_insn(instruction_t *insn) {
  // Clear map table and RS for given instruction
  int i = 0;

  for (; i < NUM_OUTPUT_REG; ++i) {
    if (insn->r_out[i] != DNA && map_table[insn->r_out[i]] == insn) {
      map_table[insn->r_out[i]] = NULL;
    }
  }

  for (i = 0; i < RESERV_INT_SIZE; ++i) {
    if (reservINT[i] == insn) {
      reservINT[i] = NULL;
      status_resINT[i] = 0;
      reservINTavail++;
      return;
    }
  }

  for (i = 0; i < RESERV_FP_SIZE; ++i) {
    if (reservFP[i] == insn) {
      reservFP[i] = NULL;
      status_resFP[i] = 0;
      reservFPavail++;
      return;
    }
  }
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {

  if(commonDataBus != NULL) {
    commonDataBus = NULL;
  }
}

/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 *  Update the CDB cycle if CDB available
 *  Clear FU, RS and maptable once inst finishes executing
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  int i = 0;
  instruction_t *insn, *insn_old_int = NULL, *insn_old_fp = NULL;
  int min_insn_index = LARGE_NUMBER, oldest_insn_idx_int = 0, oldest_insn_idx_fp = 0;

  /* FUNCTIONAL UNIT STATUS */
  // 0: Inst still executing in FU
  // 1: Inst done executing. Waiting for CDB
  // 2: Scheduled into CDB

  // Check if any of the executing instructions are done
  // If the finished instruction is a store, retire it from RS & FU
  for(i = 0; i < FU_INT_SIZE; ++i) {
    insn = fuINT[i];
    if(insn && latency_fuINT[i] != 0 && current_cycle >= latency_fuINT[i]) {
      if (!WRITES_CDB(insn->op)) { 
        // i.e. Done inst is a STORE, so clear RS  
        status_fuINT[i] = 2;
        clear_insn(insn);
      }
      else {
        // Instruction is done but it needs to wait for CDB availability
        status_fuINT[i] = 1;
        if (insn->index < min_insn_index) {
          min_insn_index = insn->index;
          insn_old_int = insn;
          oldest_insn_idx_int = i;
        }
      }
    }
  }

  for(i = 0; i < FU_FP_SIZE; ++i) {
    insn = fuFP[i];
    if(insn && latency_fuFP[i] != 0 && current_cycle >= latency_fuFP[i]) {
      if (IS_STORE(insn->op)) {
        status_fuFP[i] = 2;
        clear_insn(insn);
      }
      else {
        status_fuFP[i] = 1;
        if (insn->index < min_insn_index) {
          min_insn_index = insn->index;
          insn_old_fp = insn;
          oldest_insn_idx_fp = i;
        }
      }
    }
  }

  // Now check which instruction out of FP or INT is the oldest and broadcast that to CDB
  if (insn_old_int != NULL && insn_old_fp == NULL) {
    if (update_cdb(insn_old_int, current_cycle)){
      status_fuINT[oldest_insn_idx_int] = 2;
      clear_insn(insn_old_int);
    }
  }
  else if (insn_old_int == NULL && insn_old_fp != NULL) {
    if (update_cdb(insn_old_fp, current_cycle)){
      status_fuFP[oldest_insn_idx_fp] = 2;
      clear_insn(insn_old_fp);
    }
  }
  else if (insn_old_int != NULL && insn_old_fp != NULL) {
    if (insn_old_int->index < insn_old_fp->index) {
      if (update_cdb(insn_old_int, current_cycle)){
        status_fuINT[oldest_insn_idx_int] = 2;
        clear_insn(insn_old_int);
      }
    }
    else {
      if (update_cdb(insn_old_fp, current_cycle)) {
        status_fuFP[oldest_insn_idx_fp] = 2;
        clear_insn(insn_old_fp);
      }
    }
  }

  // Clear the FU if it has been broadcasted
  for (i = 0; i < FU_INT_SIZE; ++i) {
    if (status_fuINT[i] == 2) {
      status_fuINT[i] = 0;
      fuINT[i] = NULL;
      fuINTavail++;
      latency_fuINT[i] = 0;
    }
  }
  for (i = 0; i < FU_FP_SIZE; ++i) {
    if (status_fuFP[i] == 2) {
      status_fuFP[i] = 0;
      fuFP[i] = NULL;
      fuFPavail++;
      latency_fuFP[i] = 0;
    }
  }
}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 *      Also clear dependencies for instruction if CDB is broadcasting 
 *      Update the execute cycle if scheduled into FU
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void issue_To_execute(int current_cycle) {

  int i = 0, j = 0;
  int min_insn_index = LARGE_NUMBER;
  int oldest_insn_idx;
  int fu_idx;
  bool found_ready_insn = false;

  /* RESERVATION STATIONS STATUS*/
  // 0: Waiting for dependency to clear out
  // 1: Dependency solved. Waiting to be scheduled
  // 2: Scheduled into FU  

  // If there are no dependencies, this instruction is ready to execute
  for (i = 0; i < RESERV_INT_SIZE; ++i) {
    instruction_t *insn = reservINT[i];
    if (insn != NULL && status_resINT[i] == 0) {
      if(insn->Q[0] == NULL && insn->Q[1] == NULL && insn->Q[2] == NULL) {
        status_resINT[i] = 1;
      }
    }
  }

  for (i = 0; i < RESERV_FP_SIZE; ++i) {
    instruction_t *insn = reservFP[i];
    if (insn != NULL && status_resFP[i] == 0) {
      if(insn->Q[0] == NULL && insn->Q[1] == NULL && insn->Q[2] == NULL) {
        status_resFP[i] = 1;
      }
    }
  }

  // Check if FU is available, and if the instruction is ready to execute, execute it
  while(fuINTavail > 0) {
    found_ready_insn = false;
    min_insn_index = LARGE_NUMBER;
    for(i = 0; i < RESERV_INT_SIZE; ++i) {
      if(status_resINT[i] == 1) {
        if(reservINT[i]->index < min_insn_index) {
          min_insn_index  = reservINT[i]->index;
          oldest_insn_idx = i;
          found_ready_insn = true;
        }
      }
    }
    if (found_ready_insn) {
      fuINTavail--;
      fu_idx = get_available_idx(fuINT, FU_INT_SIZE);
      fuINT[fu_idx] = reservINT[oldest_insn_idx];
      status_resINT[oldest_insn_idx] = 2;
      fuINT[fu_idx]->tom_execute_cycle = current_cycle;
      latency_fuINT[fu_idx] = current_cycle + FU_INT_LATENCY;      
    }
    else {
      break;
    }
  }

  while(fuFPavail > 0) {

    found_ready_insn = false;
    min_insn_index = LARGE_NUMBER;

    for(i = 0; i < RESERV_FP_SIZE; ++i) {
      if(status_resFP[i] == 1) {
        if(reservFP[i]->index < min_insn_index) {
          min_insn_index  = reservFP[i]->index;
          oldest_insn_idx = i;
          found_ready_insn = true;
        }
      }
    }
    if (found_ready_insn) {
      fuFPavail--;
      fu_idx = get_available_idx(fuFP, FU_FP_SIZE);
      fuFP[fu_idx] = reservFP[oldest_insn_idx];
      status_resFP[oldest_insn_idx] = 2;
      fuFP[fu_idx]->tom_execute_cycle = current_cycle;
      latency_fuFP[fu_idx] = current_cycle + FU_FP_LATENCY;

    }
    else {
      break;
    }
  }

    // Check CDB dependencies
  if (commonDataBus != NULL) {
    // Check for all INT & FP RS if any of the dependent values are now available 
    // to be used in the next cycle. If so, clear dependency in Q of insn. 
    for(i = 0; i < RESERV_INT_SIZE; ++i) {
      if (reservINT[i] != NULL) {
        for (j = 0; j < NUM_INPUT_REG; ++j) {
          if (reservINT[i]->Q[j] != NULL && reservINT[i]->Q[j] == commonDataBus) {
            reservINT[i]->Q[j] = NULL;
          }
        }
      }
    }

    for(i = 0; i < RESERV_FP_SIZE; ++i) {
      if (reservFP[i] != NULL) {
        for (j = 0; j < NUM_INPUT_REG; ++j) {
          if (reservFP[i]->Q[j] != NULL && reservFP[i]->Q[j] == commonDataBus) {
            reservFP[i]->Q[j] = NULL;
          }
        }
      }
    }
  }
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 *  Checks if instruction at head of IFQ is branch -> do nothing
 *  Else, check if reservation station is available and update issue cycle
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {

  if (instr_queue_size > 0) {
    instruction_t *cur_insn = get_insn_at_head_iq();
    enum md_opcode opc = cur_insn->op;
  
    //Check whether unconditional or conditional branch, if so, remove from head
    if(IS_UNCOND_CTRL(opc) || IS_COND_CTRL(opc)) {
      remove_head_of_iq();
    }
    else {
      // Need to check for if a RS is available. If not available, keep stalling
      //RAW dependencies: check if the maptable has an entry at the input reg's indices. 
      //each input reg contains the register it will be reading from
      bool updated = false;
      if (USES_INT_FU(opc) && reservINTavail > 0) {
        int int_idx = get_available_idx(reservINT, RESERV_INT_SIZE);
        cur_insn->tom_issue_cycle = current_cycle;
        reservINT[int_idx] = cur_insn;
        reservINTavail--;
        updated = true;
      }


      if (USES_FP_FU(opc) && reservFPavail > 0) {
        int fp_idx = get_available_idx(reservFP, RESERV_FP_SIZE);
        cur_insn->tom_issue_cycle = current_cycle;
        reservFP[fp_idx] = cur_insn;
        reservFPavail--;
        updated = true;
      }

      if (updated) {
        remove_head_of_iq();
        // RAW dependencies: check if the maptable has an entry at the input reg's indices. 
        //each input reg contains the register it will be reading from
        int i = 0;
        for(; i < NUM_INPUT_REG; ++i) {
          int r_in = cur_insn->r_in[i];
          // Set dependent instruction in Q
          if(r_in != DNA && map_table[r_in] != NULL) {
            cur_insn->Q[i]= map_table[r_in];
          }
        }

        //once you are done checking raw dependencies, update the maptable based on your output reg  
        for(i = 0; i < NUM_OUTPUT_REG; ++i) { 
          int r_out = cur_insn->r_out[i];
          if(r_out != DNA) {
            map_table[r_out] = cur_insn;
          }      
        }
      }
    }
  }  
}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 *  Updates the dispatch cycle when the instruction enters the IFQ
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 *      current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch(instruction_trace_t* trace, int current_cycle) {

  /* ECE552: YOUR CODE GOES HERE */
  if (instr_queue_size == INSTR_QUEUE_SIZE)
    return;

  instruction_t *insn = NULL;
  bool valid = false;
  while(fetch_index <= sim_num_insn && !valid) {
    insn = get_instr(trace, fetch_index);
    valid = (!insn || !insn->op || IS_TRAP(insn->op)) ? false : true;
    fetch_index++;
  }

  if (valid) {
    insn->tom_dispatch_cycle = current_cycle;
    add_insn_to_iq(insn);
  }

}

/* 
 * Description: 
 * 	Calls fetch
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {

  fetch(trace, current_cycle);  
}

/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr_queue[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
      reservINT[i] = NULL;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
      reservFP[i] = NULL;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i] = NULL;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = NULL;
  }


  int cycle = 1;
  while (true) {

    /* ECE552 Assignment 3 - BEGIN CODE */
    
    CDB_To_retire(cycle);
    execute_To_CDB(cycle);
    issue_To_execute(cycle);    
    dispatch_To_issue(cycle);
    fetch_To_dispatch(trace, cycle);

    /* ECE552 Assignment 3 - END CODE */

    cycle++;

     if (is_simulation_done(sim_num_insn)) {
        break;
     }
  }


  return cycle;
}
