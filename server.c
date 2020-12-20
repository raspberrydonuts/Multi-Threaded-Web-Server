// Implemented by Jared Lim limxx564
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include "util.h"
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

#define MAX_THREADS 100
#define MAX_queue_len 100
#define MAX_CE 100
#define INVALID -1
#define BUFF_SIZE 1024

// structs:
typedef struct request_queue {
   int fd;
   char *request;
} request_t;

typedef struct cache_entry {
    int len;
    char *request;
    char *content;
} cache_entry_t;


// global variables
static volatile sig_atomic_t doneflag = 0;  // for graceful termination
request_t request_buffer[MAX_queue_len];  // request queue
int num_requests = 0;
int insert_idx = 0, remove_idx = 0;
int queue_length;
FILE *log_file_ptr;

// lock variables for changes to requeust_buffer, num_requests, and log_file_ptr
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

/* ******************** Dynamic Pool Code  [Extra Credit A] **********************/
// Extra Credit: This function implements the policy to change the worker thread pool dynamically
// depending on the number of requests
void * dynamic_pool_size_update(void *arg) {
  while(1) {
    // Run at regular intervals
    // Increase / decrease dynamically based on your policy
  }
}
/**********************************************************************************/

/* ************************ Cache Code [Extra Credit B] **************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  /// return the index if the request is present in the cache
  return 1;
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory , int memory_size){
  // It should add the request at an index according to the cache replacement policy
  // Make sure to allocate/free memory when adding or replacing cache entries
}

// clear the memory allocated to the cache
void deleteCache(){
  // De-allocate/free the cache memory
}

// Function to initialize the cache
void initCache(){
  // Allocating memory and initializing the cache array
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char * mybuf) {
  // Should return the content type based on the file type in the request
  // (See Section 5 in Project description for more details)
  if (strstr(".htm", mybuf) != NULL) return "text/html";
  else if (strstr(".jpg", mybuf) != NULL) return "image/jpeg";
  else if (strstr(".gif", mybuf) != NULL) return "image/gif";
  else return "text/plain";
}

// Function to open and read the file from the disk into the memory
// also gets file size
char * readFromDisk(char *path, int *num_bytes) {
  // Open and read the contents of file given the request
  char *request_string;
  struct stat file_stat;

  // attempt to open the file
  FILE *fptr = fopen(path+1, "r");  // add 1 to path account for the first `/`
  if (fptr  == NULL) {
    printf("Could not open file while reading from disk\n");
    return NULL;
  }

  // attempt to get file info
  if (stat(path+1, &file_stat) == -1) {
    printf("Error while calling stat\n");
    return NULL;
  }
  // set num_bytes equal to file size
  *num_bytes = file_stat.st_size;

  // dynamically allocate space for the file into memory
  request_string = malloc(*num_bytes);  // memory will be freed in worker()

  // read the file from the disk into the memory
  if (fread(request_string, *num_bytes, 1, fptr) == 0) {
    printf("Could not perform fread() on file\n");
    return NULL;
  }
  fclose(fptr);
  return request_string;
}

// function to log requests to log file and terminal from worker threads
void logRequest(int threadId, int reqNum, int fd, char *reqStr, int bytesOrError) {
  if (bytesOrError == 0) {
    fprintf (log_file_ptr, "[%d][%d][%d][%s][%s]\n", threadId, reqNum, fd, reqStr, "Requested file not found");
    fprintf (stdout, "[%d][%d][%d][%s][%s]\n", threadId, reqNum, fd, reqStr, "Requested file not found");
  } else {
    fprintf (log_file_ptr, "[%d][%d][%d][%s][%d]\n", threadId, reqNum, fd, reqStr, bytesOrError);
    fprintf (stdout, "[%d][%d][%d][%s][%d]\n", threadId, reqNum, fd, reqStr, bytesOrError);
  }
}
/**********************************************************************************/

// Function to receive the request from the client and add to the queue
void * dispatch(void *arg) {
  int fd;
  request_t request;
  char filename[BUFF_SIZE];

  while (1) {
    // Accept client connection
    fd = accept_connection();

    if (fd >= 0) {  // if connection successful
      pthread_mutex_lock(&lock);  // lock mutex

      // Get request from the client
      if ((get_request(fd, filename)) != 0) {
        printf("Failed to get request from the client\n");
        pthread_mutex_unlock(&lock);
        exit(0);
      }

      // synchronize
      while (num_requests == queue_length) {
        pthread_cond_wait(&not_full, &lock);
      }
      
      // Add the request into the queue
      request.fd = fd;
      request.request = filename;
      request_buffer[insert_idx] = request;
      printf("Request added to queue: %s\n", request.request);

      // update insert_idx
      insert_idx++;
      insert_idx = insert_idx % queue_length;
      num_requests++;
      pthread_cond_signal(&not_empty);
      pthread_mutex_unlock(&lock);
    }
    
  }
  return NULL;
}

/**********************************************************************************/

// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
  request_t request;
  int requests_handled = 0;
  int num_bytes = 0;
  char *content_buffer;

  while (1) {
    requests_handled++;
    pthread_mutex_lock(&lock);

    // synchronize
    while (num_requests == 0) {
      pthread_cond_wait(&not_empty, &lock);
    }

    // Get the request from the queue
    request = request_buffer[remove_idx];

    // update remove_idx
    remove_idx++;
    remove_idx = remove_idx % queue_length;

    // Get the data from the disk or the cache (extra credit B)
    // if we could not read the data from disk, return error
    if ((content_buffer = readFromDisk(request.request, &num_bytes)) == NULL) {
      free(content_buffer);
      if ((return_error(request.fd, "File not found")) != 0) {
        printf("Failed to return error\n");
        exit(0);
      }
    }
    // else, return the result
    else {
      if (return_result(request.fd, getContentType(request.request), content_buffer, num_bytes) != 0) {
        printf("Failed to get request from the client\n");
       exit(0);
      }
    }
    num_requests--;
    // Log the request into the file and terminal
    int thread_id = *(int*)arg;
    logRequest(thread_id, requests_handled, request.fd, request.request, num_bytes);
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&lock);
    free(content_buffer);
  }
  return NULL;
}

// used for graceful termination in main
static void setdoneflag(int signo) {
  doneflag = 1;
}

/**********************************************************************************/

int main(int argc, char **argv) {
  // Error check on number of arguments
  if(argc != 8){
    printf("usage: %s port path num_dispatcher num_workers dynamic_flag queue_length cache_size\n", argv[0]);
    return -1;
  }

  // Get the input args
  int port 	= strtol(argv[1], NULL, 10);
  char *path 	= argv[2];
  int num_dispatch 	= strtol(argv[3], NULL, 10);
  int num_workers 	= strtol(argv[4], NULL, 10);
  int dynamic_flag 	= strtol(argv[5], NULL, 10);
  int q_len 	= strtol(argv[6], NULL, 10);
  int cache_entries 	= strtol(argv[7], NULL, 10);

  // Perform error checks on the input arguments
  if (port < 1025 || port > 65535) {
    printf("Invalid port number\n");
    exit(0);
  }

  if (strlen(path) > BUFF_SIZE) {
    printf("Filename length cannot be greater than 1024\n");
    exit(0);
  }

  if (num_dispatch > MAX_THREADS || num_dispatch < 1) {
    printf("Must have at least one dispatcher and a maximum of 100 dispatchers\n");
    exit(0);
  }

  if (num_workers > MAX_THREADS || num_workers < 1) {
    printf("Must have at least one worker and a maximum of 100 workers\n");
    exit(0);
  }

  if (dynamic_flag != 0 && dynamic_flag != 1) {
    printf("Invalid dynamic_flag number\n");
    exit(0);
  }

  if (q_len > MAX_queue_len || q_len < 1) {
    printf("Must have a queue length of at least 1 and less than 100\n");
    exit(0);
  }

  if (cache_entries < 0 || cache_entries > MAX_CE) {
    printf("Number of cache entries must be between 0 and 100\n");
    exit(0);
  }

  // Change SIGINT action for grace termination
  struct sigaction act;
  act.sa_handler = setdoneflag;
  act.sa_flags = 0;
  if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)) {
    perror("Failed to set SIGINT handler\n");
    exit(0);
  }

  // Open log file
  log_file_ptr = fopen("web_server_log","w");
  if (log_file_ptr == NULL) {
    printf("Failed to open file\n");
  }

  // Change the current working directory to server root directory
  if (chdir(path) == -1) {
    printf("Failed to change directory to path\n");
    exit(0);
  }

  // Initialize cache (extra credit B)

  // Start the server
  fprintf(stdout, "Starting server. Port=%d, #dispatchers=%d, #workers=%d\n", port, num_dispatch, num_workers);
  init(port);
  queue_length = q_len;

  // Create dispatcher and worker threads (all threads should be detachable)
  pthread_t dis_threads[num_dispatch];
  pthread_t work_threads[num_workers];
  int work_thread_ids[num_workers];

  for (int i = 0; i < num_dispatch; i++) {
    int error;
    if ((error = pthread_create(&dis_threads[i], NULL, dispatch, NULL) != 0)) {
      fprintf(stderr, "Failed to create thread: %s\n", strerror(error));
    }
    if ((error = pthread_detach(dis_threads[i]) != 0)) {
      fprintf(stderr, "Failed to detach thread: %s\n", strerror(error));
    }
  }

  for (int i = 0; i < num_workers; i++) {
    int error;
    work_thread_ids[i] = i;
    if ((error = pthread_create(&work_threads[i], NULL, worker, &work_thread_ids[i]) != 0)) {
      fprintf(stderr, "Failed to create thread: %s\n", strerror(error));
    }
    if ((error = pthread_detach(work_threads[i]) != 0)) {
      fprintf(stderr, "Failed to detach thread: %s\n", strerror(error));
    }
  }

  // Create dynamic pool manager thread (extra credit A)

  // Terminate server gracefully
  while (!doneflag) {
  }
  printf("Program terminating. Gracefully...\n");
  // Print the number of pending requests in the request queue
  printf("Remaining requests in request queue: %d\n", num_requests);
  // close log file
  fclose(log_file_ptr);
  // Remove cache (extra credit B)

  return 0;
}
