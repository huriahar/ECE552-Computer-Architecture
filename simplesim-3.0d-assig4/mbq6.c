#define TOTAL_SIZE 100000

//Created a linked list with an int and a next pointer. 
//Dynamically allocate and link TOTAL_SIZES of Nodes together
//Access the values of the linked list using the next pointer.
//As the the nodes might not be allocated consecutively, the stride 
//prefetcher would not be able to prefetch values 
//The open ended prefetcher with the additional stream buffers 
//it keeps track of the pattern of accesses. We can use
//this pattern to prefetch the next address and get a smaller miss rate
struct Node {
    int val;
    struct Node *next;
};
typedef struct Node Node;

Node *head = NULL;

int main() {

    int i;
    Node *cur = head;
    for (i = 0; i < TOTAL_SIZE; ++i) {
        if (i == 0) {
            head = (Node *)malloc(sizeof(Node));
            cur = head;
        }
        assert(cur);
        cur->next = (Node *)malloc(sizeof(Node));
        cur->val = i;
        cur = cur->next;
    }
    cur->next = NULL;

    int array[TOTAL_SIZE];
    i = 0;
    cur = head;
    while (cur != NULL) {
        array[i] = cur->val;
        i++;
        cur = cur->next;
    }
}
