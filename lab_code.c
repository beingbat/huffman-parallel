#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <pthread.h> 
#include <semaphore.h> 
#include <unistd.h> 

// Structure of a node of Huffman tree 
struct HuffmanNode { 
	char data; 
	unsigned freq; 
	struct HuffmanNode* left, *right; 
}; 

// Structure for threads 
struct thread_data { 
	int thread_id; 
	char* filename; 
	int nThreads; 
	char* outputFile; 
}; 

// Function to allocate a new Huffman node 
struct HuffmanNode* newNode(char data, unsigned freq) 
{ 
	struct HuffmanNode* temp = 
		(struct HuffmanNode*)malloc(sizeof(struct HuffmanNode)); 

	temp->left = temp->right = NULL; 
	temp->data = data; 
	temp->freq = freq; 

	return temp; 
} 

// Function to swap two nodes of min heap 
void swap(struct HuffmanNode** a, struct HuffmanNode** b) 
{ 
	struct HuffmanNode* t = *a; 
	*a = *b; 
	*b = t; 
} 

// Function to build the Huffman tree and get codes 
void* huffmanCodes(void* threadarg) 
{ 
	// Extract the thread arguments 
	struct thread_data* my_data; 
	my_data = (struct thread_data*)threadarg; 

	// Get the filename and number of threads 
	char* filename = my_data->filename; 
	int nThreads = my_data->nThreads; 

	// Get the output file name 
	char* outputfile = my_data->outputFile; 

	// Open the text file 
	FILE* fp = fopen(filename, "r"); 
	if (fp == NULL) { 
		printf("File not found\n"); 
		exit(0); 
	} 

	// Get the character and its frequency 
	int n; 
	char c; 
	fscanf(fp, "%d", &n); 
	struct HuffmanNode* arr[n]; 
	for (int i = 0; i < n; i++) { 
		fscanf(fp, "%c%d", &c, &arr[i]->freq); 
		arr[i]->data = c; 
	} 

	// Create the Huffman tree 
	for (int i = 0; i < n - 1; i++) { 
		struct HuffmanNode* left = arr[i]; 
		struct HuffmanNode* right = arr[i + 1]; 

		struct HuffmanNode* top = 
			(struct HuffmanNode*)malloc(sizeof(struct HuffmanNode)); 

		top->data = '$'; 
		top->freq = left->freq + right->freq; 
		top->left = left; 
		top->right = right; 

		arr[i + 1] = top; 
		i++; 
	} 

	// Create the output file 
	FILE* f = fopen(outputfile, "w"); 
	if (f == NULL) { 
		printf("Error creating file!\n"); 
		exit(1); 
	} 

	// Store the codes in output file 
	char codes[n]; 
	int top = n - 1; 
	recur(arr[top], codes, 0, f); 
	fclose(f); 
} 

// Recursive function to print codes 
void recur(struct HuffmanNode* root, char codes[], 
			int top, FILE* f) 
{ 
	// Assign 0 to left edge and recuse 
	if (root->left) { 

		codes[top] = '0'; 
		recur(root->left, codes, top + 1, f); 
	} 

	// Assign 1 to right edge and recuse 
	if (root->right) { 

		codes[top] = '1'; 
		recur(root->right, codes, top + 1, f); 
	} 

	// If this is a leaf node, then it contains one of the input 
	// characters, print the character and its code from arr[] 
	if (isLeaf(root)) { 
		fprintf(f, "%c: %s\n", root->data, codes); 
	} 
} 

// Returns true if the node is a leaf node 
int isLeaf(struct HuffmanNode* root) 
{ 
	return !(root->left) && !(root->right); 
} 

// Main function to zip/unzip the file 
int main(int argc, char* argv[]) 
{ 
	// Store the command line arguments 
	char* command = argv[1]; 
	char* filename = argv[2]; 
	int nThreads = atoi(argv[3]); 
	char* outputFile = argv[4]; 

	// Check if the command is zip or unzip 
	if (strcmp(command, "zip") == 0) { 
		// Create threads 
		pthread_t threads[nThreads]; 
		int rc; 
		struct thread_data td[nThreads]; 

		// Create nThreads threads 
		for (int i = 0; i < nThreads; i++) { 
			td[i].thread_id = i; 
			td[i].filename = filename; 
			td[i].nThreads = nThreads; 
			td[i].outputFile = outputFile; 
			rc = pthread_create(&threads[i], NULL, 
							huffmanCodes, (void*)&td[i]); 
			if (rc) { 
				printf("Error; return code from pthread_create() is %d\n", rc); 
				exit(EXIT_FAILURE); 
			} 
		} 

		// Wait for all threads to complete 
		for (int i = 0; i < nThreads; i++) 
			pthread_join(threads[i], NULL); 

		printf("Compression done successfully\n"); 
	} 
	else if (strcmp(command, "unzip") == 0) { 
		// TODO: Unzip code 
		printf("Unzip code\n"); 
	} 
	else { 
		printf("Invalid command\n"); 
	} 

	return 0; 
}