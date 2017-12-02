#define TOTAL_SIZE 1000
#define LARGE_NUM 10000
int main() {
    int array[TOTAL_SIZE] = {0};
    int i, j;
    // Access every 128th element i.e. every 512th byte
    // The cache line is 64 bytes long. So, we are accessing a value in every 8th line.
    // The Next line prefetcher failed for this microbenchmark and our stride prefetcher
    // had a low miss rate. We tried with different increments of j and the miss rate 
    // did not change.
    for (i = 0; i < LARGE_NUM; ++i) {
        for (j = 0; j < TOTAL_SIZE; j=j+128) {
            array[j] = j;
        }
    }
}
