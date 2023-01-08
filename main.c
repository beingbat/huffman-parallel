#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define MAX_TREE_HT 10

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 
  

int FREQUENCIES[128];       // how frequent is each character
char* CODES[128];           // bit codes of corresponding ascii characters
int CODES_SIZES[128];       // bit codes sizes (as they are variable)
int MAX_CODE_SIZE; 
int CHUNK_SIZE;             // Total file size / thread count
int FILE_SIZE;              // Total bytes in file
char* INPUT_FILE_NAME;      // Arg Passed
char* OUTPUT_FILE_NAME;     // Arg Passed


// Struct passed to Threads for calculating the character frequencies
struct thread_params
{
    int starting_position;  // Current thread reading position start
    int last;               // If current thread is the last thread (as last thread would need to read till the end irrespective of CHUNK_SIZE)
};

// 1 of 2 structs which are stored in *_encoding.bin, it stores values that allow in parsing the second struct in *encodng.bin
struct storecount
{
    int thread_count;       // Number of threads
    int encodes;
    int code_sizes[128];    // Global variable duplicate for dumping
};

// 2 of 2 structs stored in *_encoding.bin
struct outputmeta
{
    int bit_offset_index[16];   //Each thread's last byte will have some vacant bits, this index will point to such byte so its vacant bits can be shifted. This returns last byte
    int bit_offsets[16];    // This returns thread's last byte's last bit indices
    char codes[128][10];    //Character encodng codes
    int thread_count;       //Number of Threads
};

// Struct passed as argument to the threads which are responsible for converting text to encodings given the tree. 
struct encode_params
{
    int starting_position;  // From where to start encoding
    int last;               // Is current last
    char* output;           // Final output after encoding
    int size;               // Size of output
    int bit_offset;         //Last byte's last bit index
};

// Representing a node of tree, containing the frequency of how much a character has appeared in file including its value.
struct node
{
    int freq;
    char val;
    int lo, ro;
    struct node *left, *right;  // non-leafs Nodes will not be actual characters. They will have combine frequency of their children and value of $
};

// Get the size of the file in bytes
int fsize(FILE *fp){
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz=ftell(fp);
    fseek(fp,prev,SEEK_SET);
    return sz;
}

void* frequencyCounterT(void* arguments) //First thread that calculates the frequency of a character in file
{
    struct thread_params* args;//Creting a pointer to struct 'thread_params' which holds arguments for the thread function
    args = (struct thread_params*)arguments; 
    FILE *ptr;
    ptr = fopen(INPUT_FILE_NAME, "r");
    fseek(ptr, args->starting_position, SEEK_SET); //Setting the file pointer to position from where current thread's file chunk starts
    int remainder = 0;
    if (args->last)
        remainder=FILE_SIZE%CHUNK_SIZE;//since last file would have some extra data i.e. if chunk size was 10 bytes, last one may have 13. So we need to get those extra 3 bytes as well, so we add remainder for that
    for (int i=0; i<CHUNK_SIZE+remainder; i++)//For the complete size of chunk + remainder if any, get current character read from file and convert it to its ascii code, use that ascii code as index of FREQUENCIES array and add 1 to that arrays index value
        FREQUENCIES[(int)fgetc(ptr)] +=1;    
}

void* fileEncoder(void* arguments) //Thread that converts file input into compressed code based on the tree encoding (caed after tree creation is complete)
{
    struct encode_params* args;
    args = (struct encode_params*)arguments; 
    FILE *ptr;
    ptr = fopen(INPUT_FILE_NAME, "r");
    fseek(ptr, args->starting_position, SEEK_SET);//set file pointer to byte number specified in starting_position
    int loop_count = 0;

    if(args->last==1)
        loop_count = CHUNK_SIZE+(FILE_SIZE%CHUNK_SIZE);//finding how many times loop ran i.e. how many bytes were in the chunk so we can create malloc() array of exaact size
    else
        loop_count = CHUNK_SIZE;

    unsigned int xsize = (sizeof(char)*MAX_CODE_SIZE*loop_count)/8;//output size of the encoded file is at max (1*MAX_CODE_SIZE*NUMBER_OF_BYTES_ENCOUNTERED)/SIZE_OF_BYTE, we divide by 8 because each character is converted into a set of bits which can be at max eequal to MAX_CODE_SIZE so we multiply by that and divide by BYTE_SIZE (8)
    if (xsize < 1)
        xsize=1;//If in any case the size becomes less than one than we make it 1 to avoid error
    args->output = malloc(xsize);//allocate memory to output variable

    args->size = 0;
    int c = 0;
    int bit = 0;//what is it used for 
    for (int i=0; i<loop_count; i++)
    {
        int val = (int)fgetc(ptr); //Get character then convert it to its corresponding decimal values
        char* code =  CODES[val];//simply the encoding for encountered character e.g. 001, 110
        for (int d=0; d < CODES_SIZES[val]; d++)//run loop for the length of currently encountered character's encoding
        {   
            char x = code[d];
            sscanf(&x, "%d", &bit);
            c = c << 1;//Bit shift c to left by one
            c = c | bit;//then make last bit either 0 or 1 based on what is present in encoding
            args->size++;//increase output size (size here is in bits)
            if(args->size%8==0)//checking if complete byte is allocated or there is some vacancy left
            {
                args->output[(args->size/8) -1] = (char)c;//Proceed to next bit in output
                c = 0;
            }
            else if(d==CODES_SIZES[val]-1 && i==loop_count-1)//If it is the last character from file, then we would need to store it regardless byte is complete or not
            {
                args->bit_offset = args->size%8;//store how many bits are occupied in final byte
                if (args->bit_offset > 0)
                {

                    c = c << 8-args->bit_offset;//Move the current character's encoding  to the left by the same amount
                    args->output[(args->size/8)] = (char)c;//Then assign the above value to last byte of array
                }
            }
        }
    }

}

void minHeapify(struct node **char_nodes, int i, int count) // Helper function used in creating tree. To convert a simple array into minHeap (PriorityQueue)
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

struct node *extractMin(struct node **minHeap, int count) // Extracting minimum elem i.e. root of the tree in minheap
{
  struct node *temp = minHeap[0];
  minHeap[0] = minHeap[count - 1];
  count--;
  minHeapify(minHeap, 0, count);
  return temp;
}

// Insertion function
void insertMinHeap(struct node **minHeap, struct node *minHeapNode, int count) //Inserting new val in minheap 
{
    int i = count-1;
    while (i && minHeapNode->freq < minHeap[(i - 1) / 2]->freq) {
        minHeap[i] = minHeap[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap[i] = minHeapNode;
}

// Function which finds encodings by converting array of nodes into minheap (tree) and then removing from tree and combining two least frequent characters in tree and then putting back their sum into it. Until only one node i.e. root is left.
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
        top->lo = 1;
        top->ro = 1;
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


void savetree(struct node* root, FILE* p)
{
    if(root->left != NULL)
    {    
        root->lo=1;

    }
    else
        root->ro=0; 

    if (root->right != NULL)
    {   
        root->ro=1; 

    }
    else
        root->ro=0;

    fwrite(root, sizeof(struct node), 1, p);
    if(root->left != NULL)
        savetree(root->left, p);
    if(root->right != NULL)
        savetree(root->right, p);
 
}



//Method that is called when zip argument is passed
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
            FREQUENCIES[i]=0;//why are you doing this

        printf("Number of threads: %d\n", t_count);
        
        FILE_SIZE = fsize(ptr);
        CHUNK_SIZE = (int)FILE_SIZE/t_count;
        printf("Dividing data among threads\n");
        printf("Data Chunk Size: %d\n", CHUNK_SIZE);//Chunk size is => FILE_SIE_IN_BYTES/threads_count
        fclose(ptr);


        //Each thread is passed its starting position and size so it only reads the data starting from start position to start position + size
        // Both of these variables are defined in thread_params struct        
        printf("Reading data and calculating frequencies...\n");
        pthread_t tids[t_count];
        struct thread_params args[t_count];
        for(int i=0; i<t_count; i++)
        {
            args[i].starting_position=i*CHUNK_SIZE;//Starting position of each thread should be CHUNKSIZE*THREAD_NUMBER
            if (i == t_count-1)
                args[i].last = 1;
            else
                args[i].last=0;
            pthread_create(&tids[i], NULL, frequencyCounterT, (void *) &args[i]);
        }


        // Waiting for all thread to return
        for (int i = 0; i < t_count; i++) 
            pthread_join(tids[i], NULL); 

        // ======================== CREATING TREE ======================

        printf("\nCreating tree using the frequencies calculated...\n");
        
        // count total unique characters found in file, Basic ASCII has 128 characters so we use 128 here
        int char_count = 0;
        for(int i=0; i<128; i++)
        {
            if (FREQUENCIES[i] > 0)
                char_count++;
        }    

        //Convert each unique character into node with val and frequencies set
        struct node** nodes;
        nodes = (struct node **)malloc(char_count*sizeof(struct node*));
        int populated = 0;
        for (int i=0; i<128; i++)
        {
            if (FREQUENCIES[i] > 0)
            {
                nodes[populated] = (struct node*)malloc(sizeof(struct node));//If a character is found in file, then create struct (node) for it (allocate memory to it) and then fill its values [We represent each occurin character with struct datastructure so we can store some other useful information with its frequency, this will be useful later in creating tree].
                nodes[populated]->val = (char)i; //The value of  occuring character is i, typecasting it back to char datatype
                nodes[populated]->freq = FREQUENCIES[i]; //Freq variable in struct is simply frequency of character in file
                nodes[populated]->right = NULL;//This struct is represeting node of tree so it can have child with same type 'node' which are null initially
                nodes[populated]->left = NULL;
                // printf("%d '%c': %d\n", populated, nodes[populated]->val, nodes[populated]->freq);
                populated++;
            }
        }

        printf("Unique characters in file: %d\n", populated);
        printf("Building minheap, executing combining algorithm and calculating encodings...\n");
        struct node* encoding_tree_root  = findEncodings(nodes, populated);//This metho will create tree using the nodes created above and give us the huffman encoding sutiable for this data 

        int arr[MAX_TREE_HT], top = 0;
        printf("\nCharacters and their encodings:\n");
        printHCodes(encoding_tree_root, arr, top);


        // ============================== Getting encoded values =====================

        // Get encodings from input text using tree
        pthread_t encode_tids[t_count];
        struct encode_params argz[t_count];
        for(int i=0; i<t_count; i++)
        {
            argz[i].starting_position=i*CHUNK_SIZE;//Already explained above
            if (i == t_count-1)
                argz[i].last = 1;
            else
                argz[i].last=0;
            pthread_create(&encode_tids[i], NULL, fileEncoder, (void *) &argz[i]);
        }

        for (int i = 0; i < t_count; i++) 
            pthread_join(encode_tids[i], NULL); 

        // =========================== saving to file ===================

        // Save encoding to output file and save scorecount an outputmeta structs in encoding.bin file.

        int total_file_size = 0;
        for(int i=0; i<t_count; i++)
            total_file_size+= argz[i].size;
        
        char* output_array = malloc(sizeof(char)*(total_file_size+7)/8);//output encoding file size can be at max equal to the sum of all bytes in input file divided by 8, but since doing so may floor the resulting integer so we add 7 to make sure that we always get memory which is equal or greater than total file size.

        struct outputmeta meta;

        int index = 0;
        int offsets_count = 0;
        for (int i=0; i<t_count; i++)
        {
            int j;
            for(j=0; j<argz[i].size/8; j++)
            {   output_array[index] = argz[i].output[j];//Combining the outputs from different threads (encodings) into single output_array 
                index += 1;
            }
            if(argz[i].bit_offset > 0)
            {
                output_array[index] = argz[i].output[j];
                index += 1;
                offsets_count +=1;
            }
            meta.bit_offset_index[i] = index; 
            meta.bit_offsets[i] = argz[i].bit_offset;//collecting bit offsets of all threads in a single array (for storing in .bin file)
            // printf("\n");
        }


        for(int i=0; i<128; i++)
        {
            int j;
            for(j=0; j<CODES_SIZES[i]; j++)//Storing encoding codes for each character in struct arry so it can be dumped to bin file
                meta.codes[i][j]=CODES[i][j];//copying character by character
            meta.codes[i][j] = '\0';
        }
        meta.thread_count = t_count;

        FILE *fptr;
        char* out_name_enc = malloc(sizeof(char)*strlen(OUTPUT_FILE_NAME));
        strcpy(out_name_enc, OUTPUT_FILE_NAME);//out_name_enc is char* in which we want to copy the output file name provied as argment
        char* out_name = strcat(OUTPUT_FILE_NAME,".txt");
        out_name_enc = strcat(out_name_enc, "_encoding.bin");

        fptr = fopen(out_name,"w");

        if(fptr == NULL)
        {
            printf("Error!\n");   
            exit(1);             
        }
        
        printf("\nEncoded Data\n");
        for(int i=0; i<index; i++)
        {
            printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(output_array[i]));//This is just for printing bytes as bits instead of chars or ints on terminal
            // int return_val = 
            fputc(output_array[i], fptr);
            printf(" ");

        }
        printf("\n\n");
        // for (int i =0; i<)
         
        fclose(fptr);

        FILE *p;
        if ((p = fopen(out_name_enc,"wb")) == NULL){
            printf("Error! opening file\n");
            exit(1);
        }
        struct storecount count;
        count.thread_count = t_count;
        count.encodes = index;
        count.code_sizes;
        for (int i=0;  i<128; i++)
         {   count.code_sizes[i] = CODES_SIZES[i];
         }   
        // Storing two structures, basically they can be combined but for testing I was trying to save space by storing meta data in first and then allocating dynamic memory to second so it wont waste memory but that did not work.

        fwrite(&count, sizeof(struct storecount), 1, p); //dumping struct data in bin file, 1 means total number of struct storecounts, fwrite expects array so we give pointer to count using &count and also speicify its size which is 1, p here is file pointer
        fwrite(&meta, sizeof(struct outputmeta), 1, p); 
        
        savetree(encoding_tree_root, p);
        fclose(p); 
    }

}



struct node* readtree(struct node* root, FILE* p)
{
    root = malloc(sizeof(struct node));
    fread(root, sizeof(struct node), 1, p);
    if(root->lo==1)
        root->left = readtree(root->left, p);
    if(root->ro==1)
        root->right = readtree(root->left, p);
    return root;
}


void unzip(int t_count)
{
    char* bin_file;
    bin_file = malloc(strlen(INPUT_FILE_NAME)+1+13); //why 1+13: INPUT_FILE_NAME + "_encoding.txt" [13] + '\0' [1] 
    strcpy(bin_file, INPUT_FILE_NAME);
    strcat(bin_file, "_encoding.bin");
    printf("Reading bin file...%s\n", bin_file);

    FILE *fptr;
    if ((fptr = fopen(bin_file, "rb")) == NULL){
        printf("Error! opening file\n");
        exit(1);
    }

    struct storecount count;
    fread(&count, sizeof(struct storecount), 1, fptr);  //Reading from bin file just like we stored   
    
    printf("Encoding Thread Count: %d\n", count.thread_count);  //count struct had info about how many threads were used in encoding, here we are printing that

    // printf("Codes sizes: \n");
    // for(int i=0; i<128; i++)
    //     printf("%d\n", count.code_sizes[i]);
    
    struct outputmeta info;
    info.bit_offset_index;// = malloc(sizeof(int)*count.thread_count); //This line is to be commented completely, as i have metnioned above, Initial idea was to use dynamic allocation which did not work as I was not able to read data back from bin file when dynamic array was being dumped to it.

    info.bit_offsets;// = malloc(sizeof(int)*count.thread_count);
    info.codes;
    fread(&info, sizeof(struct outputmeta), 1, fptr);     
    
    printf("\nBit offset [index, val]\n");
    for(int i=0; i<count.thread_count; i++)
         printf("[%d: %d]\n", info.bit_offset_index[i], info.bit_offsets[i]);

    // printf("\nCodes for ASCII Characters\n");
    // for(int i=0;i<128; i++)
    // {
    //     if(count.code_sizes[i] > 0)
    //         printf("%c: %s\n", i, info.codes[i]);
    // }

    struct node* root;
    // READTREE
    root = readtree(root, fptr);
    
    int arr[MAX_TREE_HT], top = 0;
    printf("\nCharacters and their encodings:\n");
    printHCodes(root, arr, top);
    // printf("%c\n", root->val);
    fclose(fptr);
    
    // ENCODING BIN FILE READING END
    char* inp_file;
    inp_file = malloc(strlen(INPUT_FILE_NAME)+1+4); //why 1+13: INPUT_FILE_NAME + "_encoding.txt" [13] + '\0' [1] 
    strcpy(inp_file, INPUT_FILE_NAME);
    strcat(inp_file, ".txt");
    printf("\nReading encoded file...%s\n", inp_file);

    // FILE *fptr;
    if ((fptr = fopen(inp_file, "r")) == NULL){
        printf("Error! opening file\n");
        exit(1);
    }

    char *ch = malloc(sizeof(char)*(count.encodes+1));
    int i=0;
    do {
        ch[i] = fgetc(fptr);
        printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(ch[i]));
        printf(" ");
        i++;
    } while (i < count.encodes);
    ch[i] = EOF;
 
    printf("\n");
    fclose(fptr);

    char *genout = malloc(sizeof(char)*500);
    int genoutindex = 0;
    int offset_counts = 0;
    struct node* croot = root;
    for(int i=0; i<count.encodes; i++)
    {
        for (int j=0; j<8; j++)
        {
            int bit = ((ch[i] >> 7-j)  & 0x01);
            if(bit==0)
                croot = croot->left;
            else
                croot = croot->right;

            if(croot->left==NULL && croot->right==NULL)
            {
                genout[genoutindex] = croot->val;
                genoutindex++;
                croot = root;
            }

            if(i==info.bit_offset_index[offset_counts]-1 && j+1==info.bit_offsets[offset_counts])
            {
                offset_counts++;
                break;
            }
    }
        }
    genout[genoutindex] = '\0';

    printf("\nOutput Reconstructed: %s\n", genout);

    FILE *genp;
    if ((genp = fopen(OUTPUT_FILE_NAME,"w")) == NULL){
        printf("Error! opening gen file\n");
        exit(1);
    }
    fputs(genout, genp);
    printf("\nFile saved %s\n", OUTPUT_FILE_NAME);

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