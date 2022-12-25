#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define MAX_TREE_HT 10


int fsize(FILE *fp){
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz=ftell(fp);
    fseek(fp,prev,SEEK_SET);
    return sz;
}

int FREQUENCIES[128];
int CHUNK_SIZE = 10;
char* INPUT_FILE_NAME;

struct thread_params
{
    int starting_position;
    int last;
};

struct node
{
    int freq;
    char val;
    struct node *left, *right;
};

void* frequencyCounterT(void* arguments)
{
    struct thread_params* args;
    args = (struct thread_params*)arguments; 
    FILE *ptr;
    ptr = fopen(INPUT_FILE_NAME, "r");
    fseek(ptr, args->starting_position, SEEK_SET);
    
    for (int i=0; i<CHUNK_SIZE; i++)
        FREQUENCIES[(int)fgetc(ptr)] +=1;    
}

void minHeapify(struct node **char_nodes, int i, int count)
{
    // Sorting values by frequency
    // Parent: (i-1)/2
    // Left Child (2*i)+1
    // Right Child (2*i)+2
    //Starting from Last level -1
    int current = i;
    // printf("Index (%d)\n", current);
    int left_child = 2*i+1;
    int right_child = 2*i+1;
    // set current to left child index
    if (left_child < count && char_nodes[current]->freq > char_nodes[left_child]->freq)
        current = left_child;
    // set current to right child index
    if (right_child < count && char_nodes[current]->freq > char_nodes[right_child]->freq)
        current = right_child;
    // If index changed then swap smaller with ith index (smaller being current)
    if (current != i) 
    {
        printf("Swapping at %d ", i);
        struct node *t = *&char_nodes[current];
        *&char_nodes[current] = *&char_nodes[i];
        *&char_nodes[i] = t;
        minHeapify(char_nodes, current, count);
        printf("'%c'[%d] with current '%c'[%d]\n", char_nodes[i]->val, char_nodes[i]->freq, char_nodes[current]->val, char_nodes[current]->freq);
    }
}

// struct node *extractMin(struct node *minHeap, int count) {
//   struct node *temp = &minHeap[0];
//   minHeap[0] = minHeap[count - 1];
//   minHeapify(minHeap, 0, count);
//   return temp;
// }

// // Insertion function
// void insertMinHeap(struct node *char_nodes, struct node *new_node, int count) {
//   int i = count;
//   while (i && new_node->freq < char_nodes[(i - 1) / 2].freq) {
//     char_nodes[i] = char_nodes[(i - 1) / 2];
//     i = (i - 1) / 2;
//   }
//   char_nodes[i] = new_node;
// }

struct node** findEncodings(struct node** char_nodes, int count)
{
    for (int i = (count - 1); i >= 0; --i)
        minHeapify(char_nodes, i, count);
    // char_nodes is now convertesd into heap array
    for(int i =0; i<count; i++)
        printf("%d\n", char_nodes[i]->freq);
    fflush(stdout);

    return char_nodes;
    // struct node *left, *right, *top;
    // while (count>1) 
    // {
    //     left = extractMin(char_nodes, count);
    //     count--;
    //     right = extractMin(char_nodes, count);
    //     count--;        
    //     struct node *top = (struct node *)malloc(sizeof(struct node));
    //     top->val = '$';
    //     top->freq = left->freq + right->freq;
    //     top->left = left;
    //     top->right = right;
    //     count++;
    //     insertMinHeap(char_nodes, top, count);
    // }
    // return char_nodes[0];
}


// // Print the array
// void printArray(int arr[], int n) {
//   int i;
//   for (i = 0; i < n; ++i)
//     printf("%d", arr[i]);

//   printf("\n");
// }

// void printHCodes(struct node *root, int arr[], int top) {
//   if (root.left) {
//     arr[top] = 0;
//     printHCodes(root.left, arr, top + 1);
//   }
//   if (root.right) {
//     arr[top] = 1;
//     printHCodes(root.right, arr, top + 1);
//   }
//   if (root.right==NULL && root.left==NULL) {
//     printf("  %c   | ", root.val);
//     printArray(arr, top);
//   }
// }

int main(int argc, char** argv)
{
    int t_count = 1;
    if (argc >= 3)
    {
        INPUT_FILE_NAME = argv[1];
        sscanf(argv[2], "%d", &t_count);
    }
    FILE* ptr;
    ptr = fopen(INPUT_FILE_NAME, "r");
    if (NULL == ptr)
    {    printf("file can't be opened \n");
        exit(EXIT_FAILURE); 
    }
    else
    {
        for(int i=0; i<128; i++)
            FREQUENCIES[i]=0;

        int file_size = fsize(ptr);
        CHUNK_SIZE = (int)file_size/t_count;
        fclose(ptr);
        // printf("File size is: %d\n", file_size);
        // printf("Chunk size for each thread: %d\n", CHUNK_SIZE);
        // char ch;
        pthread_t tids[t_count];
        struct thread_params args[t_count];
        for(int i=0; i<t_count; i++)
        {
            args[i].starting_position=i*CHUNK_SIZE;
            if (i == t_count-1)
                args[i].last = 1;
            else
                args[i].last=0;

            pthread_create(&tids[i], NULL, frequencyCounterT, (void *) &args[i]);
        }

        for (int i = 0; i < t_count; i++) 
            pthread_join(tids[i], NULL); 

        // ==============================================

        // printf("Frequencies of all Characters:\n");
        int char_count = 0;
        for(int i=0; i<128; i++)
        {
            // printf("%d: %d\n", i, FREQUENCIES[i]);
            if (FREQUENCIES[i] > 0)
                char_count++;
        }    
        struct node** nodes;
        nodes = (struct node **)malloc(char_count*sizeof(struct node*));
        int populated = 0;
        for (int i=0; i<128; i++)
        {
            if (FREQUENCIES[i] > 0)
            {
                nodes[populated] = (struct node*)malloc(sizeof(struct node));
                nodes[populated]->val = (char)i; 
                nodes[populated]->freq = FREQUENCIES[i]; 
                nodes[populated]->right = NULL;
                nodes[populated]->left = NULL;
                // printf("%d '%c': %d\n", populated, nodes[populated]->val, nodes[populated]->freq);
                populated++;
            }
        }
        printf("Nodes Populated: %d\n", populated);
        struct node** encoding_tree = findEncodings(nodes, populated);

        // printf("Root Val Freq %d, Item %c", encoding_tree[0]->freq, encoding_tree[0]->val);

        // int arr[MAX_TREE_HT], top = 0;
        // printHCodes(encoding_tree, arr, top);


        // ===================================================

    }

    return 0;
}