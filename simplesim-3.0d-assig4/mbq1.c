#define TOTAL_SIZE 10000
#define LARGE_NUM 1000000

int main() {
    int array[TOTAL_SIZE] = {0};
    int i, j;
    // Access every 16th element i.e. every 64th byte
    // The cache line is 64 bytes long. The prefetcher will always get the next line
    // which will be a hit when an index in that next line is accessed.
    // This prefetcher will have a much lower miss rate than no prefetcher
    // When incrementing j with a higher value than 16, the miss_rate increased.
    for (i = 0; i < LARGE_NUM; ++i) {
        for (j = 0; j < TOTAL_SIZE; j=j+16) {
            array[j] = j;
        }
    }
}
