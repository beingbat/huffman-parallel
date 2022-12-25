#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define MAX_TREE_HT 10

int FREQUENCIES[128];
char* CODES[128]; 
int CODES_SIZES[128]; 
int MAX_CODE_SIZE;
int CHUNK_SIZE;
int FILE_SIZE;
char* INPUT_FILE_NAME;
char* OUTPUT_FILE_NAME;

struct thread_params
{
    int starting_position;
    int last;
};

struct outputmeta
{
    int* bit_offset_index;
    int* bit_offsets;
    char** codes;
    int* codes_sizes;
    int thread_count;
};

struct encode_params
{
    int starting_position;
    int last;
    char* output;
    int size;
    int bit_offset;
};

struct node
{
    int freq;
    char val;
    struct node *left, *right;
};

int fsize(FILE *fp){
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz=ftell(fp);
    fseek(fp,prev,SEEK_SET);
    return sz;
}

void* frequencyCounterT(void* arguments)
{
    struct thread_params* args;
    args = (struct thread_params*)arguments; 
    FILE *ptr;
    ptr = fopen(INPUT_FILE_NAME, "r");
    fseek(ptr, args->starting_position, SEEK_SET);
    int remainder = 0;
    if (args->last)
        remainder=FILE_SIZE%CHUNK_SIZE;
    for (int i=0; i<CHUNK_SIZE+remainder; i++)
        FREQUENCIES[(int)fgetc(ptr)] +=1;    
}

void* fileEncoder(void* arguments)
{
    struct encode_params* args;
    args = (struct encode_params*)arguments; 
    FILE *ptr;
    ptr = fopen(INPUT_FILE_NAME, "r");
    fseek(ptr, args->starting_position, SEEK_SET);
    int loop_count = 0;

    if(args->last==1)
        loop_count = CHUNK_SIZE+(FILE_SIZE%CHUNK_SIZE);
    else
        loop_count = CHUNK_SIZE;

    unsigned int xsize = (sizeof(char)*MAX_CODE_SIZE*loop_count)/8;
    if (xsize < 1)
        xsize=1;
    args->output = malloc(xsize);

    args->size = 0;
    int c = 0;
    int bit = 0;
    for (int i=0; i<loop_count; i++)
    {
        int val = (int)fgetc(ptr); 
        char* code =  CODES[val];
        for (int d=0; d < CODES_SIZES[val]; d++)
        {   
            char x = code[d];
            sscanf(&x, "%d", &bit);
            c = c << 1;
            c = c | bit;
            args->size++;
            if(args->size%8==0)
            {
                args->output[(args->size/8) -1] = (char)c;
                c = 0;
            }
            else if(d==CODES_SIZES[val]-1 && i==loop_count-1)
            {
                args->bit_offset = args->size%8;
                if (args->bit_offset > 0)
                {
                    c << 8-args->bit_offset;
                    args->output[(args->size/8)] = (char)c;
                }
            }
        }
    }

}

void minHeapify(struct node **char_nodes, int i, int count)
{
    // Sorting values by frequency
    // Parent: (i-1)/2 // Left Child (2*i)+1 // Right Child (2*i)+2 //Starting from Last level -1
    int current = i;
    int left_child = 2*i+1;
    int right_child = 2*i+2;
    // set current to left child index
    if (left_child < count && char_nodes[current]->freq > char_nodes[left_child]->freq)
        current = left_child;
    // set current to right child index
    if (right_child < count && char_nodes[current]->freq > char_nodes[right_child]->freq)
        current = right_child;
    // If index changed then swap smaller with ith index (smaller being current)
    if (current != i) 
    {
        // printf("Swapping at %d ", i);
        struct node *t = *&char_nodes[current];
        *&char_nodes[current] = *&char_nodes[i];
        *&char_nodes[i] = t;
        minHeapify(char_nodes, current, count);
    }
}

struct node *extractMin(struct node **minHeap, int count) {
  struct node *temp = minHeap[0];
  minHeap[0] = minHeap[count - 1];
  count--;
  minHeapify(minHeap, 0, count);
  return temp;
}

// Insertion function
void insertMinHeap(struct node **minHeap, struct node *minHeapNode, int count) {
    int i = count-1;
    while (i && minHeapNode->freq < minHeap[(i - 1) / 2]->freq) {
        minHeap[i] = minHeap[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap[i] = minHeapNode;
}

struct node* findEncodings(struct node** char_nodes, int count)
{
    for (int i = (count - 1); i >= 0; --i)
        minHeapify(char_nodes, i, count);
    // char_nodes is now convertesd into heap array
    
    struct node *left, *right, *top;
    while (count>1) 
    {
        left = extractMin(char_nodes, count);
        count--;
        right = extractMin(char_nodes, count);
        count--;        
        struct node *top = (struct node *)malloc(sizeof(struct node));
        top->val = '$';
        top->freq = left->freq + right->freq;
        top->left = left;
        top->right = right;
        count++;
        insertMinHeap(char_nodes, top, count);
    }
    return char_nodes[0];
}

// Print the array
void printArray(int arr[], int n, char val) {
    int i;
    char* code = malloc(sizeof(char)*n);
    for (i = 0; i < n; ++i)
    {
        printf("%d", arr[i]);
        sprintf(&code[i], "%d", arr[i]);
    }
    CODES[(int)val] = code;
    CODES_SIZES[(int)val] = n;
    if (MAX_CODE_SIZE < n)
        MAX_CODE_SIZE = n;
  printf("\n");
}

void printHCodes(struct node *root, int arr[], int top) {
  if (root->left) {
    arr[top] = 0;
    printHCodes(root->left, arr, top + 1);
  }
  if (root->right) {
    arr[top] = 1;
    printHCodes(root->right, arr, top + 1);
  }
  if (root->right==NULL && root->left==NULL) {
    printf("  %c   | ", root->val);
    printArray(arr, top, root->val);
  }
}

void zip(int t_count)
{
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

        FILE_SIZE = fsize(ptr);
        CHUNK_SIZE = (int)FILE_SIZE/t_count;
        printf("Chunk size is: %d\n", CHUNK_SIZE);
        fclose(ptr);
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

        // ======================== CREATING TREE ======================

        int char_count = 0;
        for(int i=0; i<128; i++)
        {
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
                printf("%d '%c': %d\n", populated, nodes[populated]->val, nodes[populated]->freq);
                populated++;
            }
        }
        printf("Nodes Populated: %d\n", populated);
        struct node* encoding_tree_root  = findEncodings(nodes, populated);

        int arr[MAX_TREE_HT], top = 0;
        printHCodes(encoding_tree_root, arr, top);


        // ============================== Getting encoded values =====================


        pthread_t encode_tids[t_count];
        struct encode_params argz[t_count];
        for(int i=0; i<t_count; i++)
        {
            argz[i].starting_position=i*CHUNK_SIZE;
            if (i == t_count-1)
                argz[i].last = 1;
            else
                argz[i].last=0;
            pthread_create(&encode_tids[i], NULL, fileEncoder, (void *) &argz[i]);
        }

        for (int i = 0; i < t_count; i++) 
            pthread_join(encode_tids[i], NULL); 

        // =========================== saving to file ===================

        int total_file_size = 0;
        for(int i=0; i<t_count; i++)
            total_file_size+= argz[i].size;
        
        char* output_array = malloc(sizeof(char)*(total_file_size+7)/8);

        struct outputmeta meta;
        meta.codes = CODES;
        meta.codes_sizes = CODES_SIZES;
        meta.bit_offset_index = malloc(sizeof(int)*t_count); 
        meta.bit_offsets = malloc(sizeof(int)*t_count);

        int index = 0;
        int offsets_count = 0;
        for (int i=0; i<t_count; i++)
        {
            int j;
            for(j=0; j<argz[i].size/8; j++)
            {   output_array[index] = argz[i].output[j];
                index += 1;
            }
            if(argz[i].bit_offset > 0)
            {
                output_array[index] = argz[i].output[j];
                index += 1;
                offsets_count +=1;
            }
            meta.bit_offset_index[i] = index; 
            meta.bit_offsets[i] = argz[i].bit_offset;
            printf("\n");
        }

        FILE *fptr;
        char* out_name_enc = malloc(sizeof(char)*strlen(OUTPUT_FILE_NAME));
        strcpy(out_name_enc, OUTPUT_FILE_NAME);
        char* out_name = strcat(OUTPUT_FILE_NAME,".txt");
        out_name_enc = strcat(out_name_enc, "_encoding.bin");

        fptr = fopen(out_name,"w");

        if(fptr == NULL)
        {
            printf("Error!\n");   
            exit(1);             
        }
        for(int i=0; i<index; i++)
            printf("%d\t", output_array[i]);

        int return_val = fputs(output_array, fptr);
         
        fclose(fptr);

        FILE *p;
        if ((p = fopen(out_name_enc,"wb")) == NULL){
            printf("Error! opening file");
            exit(1);
        }
        
        fwrite(&meta, sizeof(struct outputmeta), 1, fptr); 
        fclose(p); 
    }

}

void unzip(int t_count)
{
    struct outputmeta info;
    FILE *fptr;

    if ((fptr = fopen(INPUT_FILE_NAME, "rb")) == NULL){
        printf("Error! opening file");
        exit(1);
    }

    fread(&info, sizeof(struct outputmeta), 1, fptr); 
    fclose(fptr);
    // for(int i=0; i<1; i++)
    //     printf("%d\t", info.bit_offset_index[i]);
    // int i = info.bit_offset_index[0];
    // printf("%d", i);
    // printf("%d\t", info.bit_offset_index[0]); 
    // printf("%d\t", info.bit_offsets[0]); 
    // printf("%c\t", info.codes[0][0]); 
    // printf("%d\t", info.codes_sizes[0]); 
}

int main(int argc, char** argv)
{
    MAX_CODE_SIZE = 0;
    int t_count = 1;
    if (argc >= 5)
    {
        INPUT_FILE_NAME = argv[2];
        OUTPUT_FILE_NAME = argv[4];
        sscanf(argv[3], "%d", &t_count);
    }
    else
    {
        printf("Not all arguments given.\n");
        exit(EXIT_FAILURE); 
    }

    if (strcmp(argv[1], "zip") == 0) 
    {
            zip(t_count); 
    }
    else if (strcmp(argv[1], "unzip") == 0)
    {
        unzip(t_count);
    }
    else
    {
        printf("First argument is invalid\n");
        exit(EXIT_FAILURE);
    }
    
    return 0;
}