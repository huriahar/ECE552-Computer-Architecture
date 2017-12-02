#define TOTAL_COUNT 1000
#define LENGTH 5

int main() {
    short num[LENGTH];
    int j = 0;
    /* 
     * Verification statistics: In "stable" state, we calculate how many
     * mispredictions occur in the inner loop. The actual pattern is NNNNT,
     * and the predicted pattern is NNNNN as calculated using the 2-level predictor.
     */
    for (j = 0; j < TOTAL_COUNT; ++j) {  
        short i = 0;
        // The below x86 code is the comparison for the inner loop
        // cmpw    $4, -6(%rbp)
        // jle .L4
        // addl    $1, -4(%rbp)        
        for (; i < LENGTH; ++i) {   
            //The below x86 code is the implementation of num[i] = i;
            // .L4: movswl  -6(%rbp), %eax
            // cltq
            // movzwl  -6(%rbp), %edx
            // movw    %dx, -16(%rbp,%rax,2)
            // movzwl  -6(%rbp), %eax
            // addl    $1, %eax
            // movw    %ax, -6(%rbp)
            num[i] = i;
        }
    }
    return 0;
}
