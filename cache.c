
#include "csapp.h"
#include "http_parser.h"
#include "cache.h"
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)
#define MAX_BLOCKS (MAX_CACHE_SIZE/MAX_OBJECT_SIZE)

static block_t *start = NULL;
static block_t *end = NULL; //the last valid block
static int blocks = 0; //total number of blocks stored in the cache
static int lru_num = 0; //the total number of GET requests
pthread_mutex_t mutex;

//@param: the URI of the request->key; the requested content->value
//malloc a new block, set its key, value, and lru_num
//Its next would point to NULL as a new block is always added to the end 
//@return: the newly built block
void free_block(block_t *block) {
    block_t *prev_block = (block_t *)(block->prev);
    block_t *next_block = (block_t *)(block->next);
    if ((prev_block == NULL) && (next_block == NULL)) {
        start = NULL;
        end = NULL;
        free(block);
    }
    else if ((prev_block == NULL) && (next_block != NULL)) {
        start = next_block;
        free(block);
    }
    else if ((prev_block != NULL) && (next_block == NULL)) {
        end = prev_block;
        free(block);
    }
    else {
        prev_block->next = next_block;
        next_block->prev = prev_block;
        free(block);
    }
    return;
}

block_t *build_new_block (char key[MAXLINE], char value[MAX_OBJECT_SIZE]) {
    block_t *new_block = malloc(sizeof(block_t));
    memcpy(new_block->key, key, MAXLINE);
    memcpy(new_block->value, value, MAX_OBJECT_SIZE);
    new_block->lru_num = lru_num;
    //add to the end of the cache list
    return new_block;
}

//@param: start of the list (cache)
//find the block in the list with the least lru number
//return the block
block_t *find_lru(block_t *start) {
    int min_lru = -1;
    block_t *current = NULL;
    block_t *lru_block = NULL;
    for (current = start; current != NULL; current = current->next) {
        if ((min_lru == -1) || (current->lru_num < min_lru)) {
            min_lru = current->lru_num;
            lru_block = current;
        }
    }
    return lru_block;
}

//@param: key and value of the new block
//add it to the end of the cache
//Special Cases: 1. If there are already a max number of blocks, then evict
//2. If the list was originally empty, both the start and end point to this new
//block
void add_new_block(char key[MAXLINE], char value[MAX_OBJECT_SIZE])
{   
    fprintf(stderr, "----adding new blocks-----\n");
    fprintf(stderr, "------blocks before: %d------\n", blocks);
    pthread_mutex_lock(&mutex);
    if (blocks == MAX_BLOCKS) {
        //if the cache is already full, then we need to evict
        //but we are not adding more blocks to the cache
        block_t *evict_block = find_lru(start);
        if (evict_block->refcnt == 0) {
            //only evict if refcnt is 0 
           free(evict_block);
        }
    }
    block_t *new_block = build_new_block(key, value);
    if ((start == NULL) && (end == NULL)) {
        //when the list is empty
        start = new_block;
        end = new_block;
        new_block->prev = NULL;
        new_block->next = NULL;
    }
    else {
        end->next = new_block;
        new_block->prev = end;
        new_block->next = NULL;
        end = new_block;
    }
    blocks += 1;
    pthread_mutex_unlock(&mutex);
    fprintf(stderr, "------blocks after: %d------\n", blocks);
    return;
}

//@param: key, uri of a request
//find a block with the same key if possible
//@return the block if found, NULL otherwise
block_t* find_block(char key[MAXLINE]) {
    block_t *current;
    for (current = start; current != NULL; current = current->next) {
        if (!strncmp(current->key, key, strlen(current->key))) {
            //note strncmp() returns 0 if the two inputs match
            //if we've found an idential key
            current->lru_num = lru_num;
            return current;
        }
    }
    return NULL;
}

//@param: the key (URI)
//search the whole list of blocks, increment the lru_num
//@return the block if one with the same key is found. Otherwise return NULL
block_t* cache(char key[MAXLINE]) {
    pthread_mutex_lock(&mutex);
    lru_num += 1;
    //everytime that cache() gets called, we are having a new GET request
    block_t *block;
    if ((block = find_block(key)) != NULL) {
        //if a block gets hit, increment its refcnt by 1
        return block;
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}