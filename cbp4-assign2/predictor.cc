#include "predictor.h"
#include <iostream>

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////
#define PT_TWOB_SIZE 4096
#define TWO_BIT_SAT_MASK 0x0FFF

uint8_t pt_two_bit[PT_TWOB_SIZE];

void InitPredictor_2bitsat() {
    //initialize the PHT with weakly biased (01)
    std::fill_n(pt_two_bit, PT_TWOB_SIZE, 1);
}

bool GetPrediction_2bitsat(UINT32 PC) {
    //using the last 12 bits of PC as the index into the PHT, find the prediction
    UINT32 idx = (PC & TWO_BIT_SAT_MASK);
    uint8_t val = pt_two_bit[idx];    
    if (val == 0 || val == 1)
        return NOT_TAKEN;
    else if (val == 2 || val == 3)
        return TAKEN;
    else {
        printf("ERROR: Value in PHT not 0,1,2,3\n");
        return TAKEN;
    }
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    UINT32 idx = (PC & TWO_BIT_SAT_MASK);
    uint8_t val = pt_two_bit[idx];
    // Branch taken
    if (resolveDir == true && val < 3)
        pt_two_bit[idx]++;
    // Branch not taken
    else if (resolveDir == false && val > 0)
        pt_two_bit[idx]--;
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////
#define BHT_MASK 0x0FF8
#define PHT_MASK 0x0007
#define BHR_MASK 0x003F
#define PT_TWOL_NUM 8
#define PT_TWOL_DEPTH 64
#define BHT_TWOL_DEPTH 512

uint8_t pt_two_level[PT_TWOL_NUM][PT_TWOL_DEPTH];
uint8_t bht[BHT_TWOL_DEPTH];

void InitPredictor_2level() {
    //Initialize the PHT tables with weakly biased (01) and all BHRs with 0s
    for (auto i = 0; i < PT_TWOL_NUM; ++i) {
        std::fill_n(pt_two_level[i], PT_TWOL_DEPTH, 1);
    }
    std::fill_n(bht, BHT_TWOL_DEPTH, 0);
}

bool GetPrediction_2level(UINT32 PC) {
    //Using the last 3 bits of PC to index into the PHT and the next 9 bits of PC to index into the BHT.
    //Using 6 bits in the indexed BHR, find the index in the determined PHT, giving us the prediction 
    UINT32 bht_idx = (PC & BHT_MASK)  >> 3;                 // 9 bits
    uint8_t pht_idx = (PC & PHT_MASK);                      // 3 bits
    uint8_t val = pt_two_level[pht_idx][bht[bht_idx]];      // bht[bht_idx] = 6 bits
    if (val == 0 || val == 1)
        return NOT_TAKEN;
    else if (val == 2 || val == 3)
        return TAKEN;
    else {
        printf("ERROR: Value in PHT not 0,1,2,3\n");
        return TAKEN;
    }
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

    UINT32 bht_idx = (PC & BHT_MASK)  >> 3;
    uint8_t pht_idx = (PC & PHT_MASK);
    uint8_t val = pt_two_level[pht_idx][bht[bht_idx]];
    // Branch taken
    if (resolveDir == true && val < 3)
        pt_two_level[pht_idx][bht[bht_idx]]++;
    // Branch not taken
    else if (resolveDir == false && val > 0)
        pt_two_level[pht_idx][bht[bht_idx]]--;

    uint8_t cur_res;
    //Update the BHR with the actual result. Appended to the right 
    cur_res = (resolveDir) ? TAKEN : NOT_TAKEN;
    bht[bht_idx] = ((bht[bht_idx] << 1) | cur_res) & BHR_MASK;
    if (bht[bht_idx] < 0 || bht[bht_idx] >= 64)
        printf("ERROR: invalid BHR\n");
}

/////////////////////////////////////////////////////////////
// openend - PERCEPTRONS
/////////////////////////////////////////////////////////////

#define GBHR_N 85
#define NUM_PERCEP 90
#define LARGE_NUM 65536     // 2^16

//calculation of weight saturation threshold 
static const int theta = static_cast<int>(1.93*GBHR_N + 14);

short perceptrons[NUM_PERCEP][GBHR_N];  // each element is 16 bits. So, it has 16*85*90 = 122400 bits
short perceptrons_bias[NUM_PERCEP];     // 16 bits. So it has 16*90 = 1440 bits
int8_t gbhr[GBHR_N];                    // 8 bits. So it has 8*85 = 680 bits -> Implemented as a circular buffer
int gbhr_start = 0;                     // variable points to current index in the GBHR 

void InitPredictor_openend() {
    //Initialize the perceptron table with 0, perceptron bias table with 1, 
    //and the global history register with -1 (not taken)
    //except for history[0] which is initialized to 1 (taken)
    for (auto i = 0; i < NUM_PERCEP; ++i) {
        std::fill_n(perceptrons[i], GBHR_N, 0);
    }
    std::fill_n(gbhr, GBHR_N, -1);
    std::fill_n(perceptrons_bias, NUM_PERCEP, 1);
    gbhr[0] = 1;
}

bool GetPrediction_openend(UINT32 PC) {
    uint8_t pc_idx = (PC % GBHR_N);
    // Bias constitutes a major portion of the sum (strongly weighted)
    int sum = perceptrons_bias[pc_idx];
    // For each of the weight values in perceptrons, reduce the 
    // product of weight with global branch history
    for (auto i = 0; i < GBHR_N; ++i) {
        sum += perceptrons[pc_idx][i]*gbhr[(gbhr_start+i)%GBHR_N];
    }
    // Predict TAKEN if sum is non-negative
    return (sum >= 0) ? TAKEN : NOT_TAKEN;
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    //Calculate the dot product of weight and history again for the perceptron
    uint8_t pc_idx = (PC % GBHR_N);
    int sum = perceptrons_bias[pc_idx];
    short t = (resolveDir) ? 1 : -1;
    for (auto i = 0; i < GBHR_N; ++i) {
        sum += perceptrons[pc_idx][i]*gbhr[(gbhr_start+i)%GBHR_N];
    }

    //Training Algorithm
    //If the predicted value is different and the sum is not saturated, update the weights
    if ((abs(sum) <= theta) || (resolveDir != predDir)) {
    
        //Update the bias value        
        if (abs(perceptrons_bias[pc_idx] + t) < LARGE_NUM) {
            perceptrons_bias[pc_idx] += t;
        }
        //Update the weights array
        for (auto i = 0; i < GBHR_N; ++i) {
            int w = perceptrons[pc_idx][i] + t*gbhr[(gbhr_start + i)%GBHR_N];
            if (abs(w) < LARGE_NUM) {
                perceptrons[pc_idx][i] = w;
            }
        }
    }
    //Update the global history register and move starting pointer forward
    gbhr[gbhr_start] = t;
    gbhr_start = (1+gbhr_start)%GBHR_N;
}
