#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

typedef struct DeferredMessage {
    char **messages;
    int key;
    int numMessages;
    int allocatedMessages;
    int currentIndex;
} DeferredMessage;

typedef struct DepotContents {
    char *name;
    int port;

    char **type;
    int *quantity;
    int numItems;
    size_t allocatedGoods;

    char **neighbours;
    int *neighbourPorts;
    volatile int numNeighbours;
    size_t allocatedNeighbours;
    bool *messageSent;
    
    pthread_t *tid;
    FILE **rstream;
    FILE **wstream;
    int numConnections;
    size_t allocatedConnections;
    sem_t lock;
    
    DeferredMessage *deferredMessages;
    int numDeferredMessages;
    size_t allocatedDeferredMessages;

} DepotContents;

void init_lock(sem_t *);
void take_lock(sem_t *);
void release_lock(sem_t *);
void show_message(int);
void check_args(int, char *[]);
bool char_in_sequence(char *, char); 
int check_valid_number(char *, int);
bool valid_name(char *);
void print_list(DepotContents *, int, char **, int);
void setup_depot(DepotContents *, int, char *[]);
int goods_at_depot(DepotContents *, char *);
void add_goods(DepotContents *, char *, int);
void run_server(DepotContents *);
void *handle_connection(void *);
int get_tid(DepotContents *);
void *handle_server_thread(void *);
void read_from_stream(DepotContents *, FILE *, bool);
void interpret_message(DepotContents *, char *, bool);
void move_items(DepotContents *, char *, int);
void defer_message(DepotContents *, char *);
void execute_message(DepotContents *, char *);
void add_neighbour(DepotContents *, char *);
void connect_depots(DepotContents *, char *);
void transfer(DepotContents *, char *);
