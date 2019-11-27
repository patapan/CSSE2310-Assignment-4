#include "2310depot.h"

// Global boolean to check if a SIGHUP has been recieved
bool sighup = false;

// Send error message to stderr and exit code, exitStatus gives the 
// exit code 
void show_message(int exitStatus) {
    const char *messages[] = {"",                                    
            "Usage: 2310depot name {goods qty}\n",            
            "Invalid name(s)\n",         
            "Invalid quantity\n"};
    fprintf(stderr, messages[exitStatus]);
    exit(exitStatus);
}

// Set the sighup boolean to be true, signal is the signal which
// is called 
void handle_sighup(const int signal) { 
    if (signal == SIGHUP) {
        sighup = true;
    }
} 

// main function initially run on startup. argc is number of arguments,
// argv is an array of those arguments
int main(int argc, char *argv[]) {
    // Handle sighup
    struct sigaction sa;
    sa.sa_handler = handle_sighup;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, 0);
 
    check_args(argc, argv);
  
    DepotContents depotContents;   
    setup_depot(&depotContents, argc, argv);
   
    // make a new thread to run the netowrk
    pthread_t threadId;
    pthread_create(&threadId, NULL, handle_server_thread, 
            (void *) &depotContents);
   
    //handle sighup
    while (1) {
        if (sighup) {
            sighup = false;
            printf("Goods:\n");
            print_list(&depotContents, depotContents.numItems, 
                    depotContents.type, 1);
            printf("Neighbours:\n");
            print_list(&depotContents, depotContents.numNeighbours,
                    depotContents.neighbours, 0);
            fflush(stdout);
        }
    }
}

// Wrapper function to create server
// v is a depotContents pointer cast to a void pointer
void *handle_server_thread(void *v) {
    DepotContents *depotContents = (DepotContents *)v;
    run_server(depotContents);
    return NULL;
}

// Create lock. l is the lock we are creating
void init_lock(sem_t *l) {
    sem_init(l, 0, 1);
}

// Take the lock. l is the lock we are locking
void take_lock(sem_t *l) {
    sem_wait(l);
}

// Release the lock. l is the lock we are releasing
void release_lock(sem_t *l) {
    sem_post(l);
}

// Setup depot and initialise memory. depotContents gives current state of
// the depot, argc gives the number of arguments and argv is an array of 
// those arguments
void setup_depot(DepotContents *depotContents, int argc, char *argv[]) {
    int numGoods = (argc - 1) / 2;
    depotContents->numItems = numGoods;
    // allocate mem for names of goods
    depotContents->allocatedGoods = numGoods + 10;
    depotContents->type = malloc((numGoods + 10) * sizeof(char *));
    for (int i = 0; i < numGoods; i++) {
        depotContents->type[i] = malloc(strlen(argv[2 + 2 * i]) * 
                sizeof(char));
    }
    // allocate quantity
    depotContents->quantity = malloc((numGoods + 10) * sizeof(int)); 

    // populate struct
    for (int i = 0; i < numGoods; i++) {
        depotContents->type[i] = argv[2 + 2 * i];
        depotContents->quantity[i] = check_valid_number(argv[3 + 2 * i], 0);
    }

    // setup lock
    init_lock(&depotContents->lock);
   
    depotContents->allocatedConnections = 10;
    depotContents->numConnections = 0;
    depotContents->tid = malloc(10 * sizeof(pthread_t));
    depotContents->rstream = malloc(10 * sizeof(FILE *));
    depotContents->wstream = malloc(10 * sizeof(FILE *));
    depotContents->messageSent = malloc(10 * sizeof(bool));
    depotContents->deferredMessages = malloc(10 * sizeof(DeferredMessage));
    depotContents->allocatedDeferredMessages = 10;
    depotContents->numDeferredMessages = 0;
    depotContents->allocatedNeighbours = 10;
    depotContents->neighbours = malloc(10 * sizeof(char *));
    depotContents->neighbourPorts = malloc(10 * sizeof(int));
    depotContents->numNeighbours = 0;
    depotContents->name = argv[1];
}

// Check to see the given arguments are valid, argc is the number of
// command line arguments and argv is an array of those arguments
void check_args(int argc, char *argv[]) {
    if ((argc < 2) || (argc % 2 == 1)) {
        show_message(1);
    }    
     
    if (!valid_name(argv[1])) {
        show_message(2);
    }
    for (int i = 2; i < argc; i = i + 2) {
        if (!valid_name(argv[i])) {
            show_message(2);
        }
    }
   
    for (int i = 3; i < argc; i = i + 2) {
        if (check_valid_number(argv[i], 0) == -1) {
            show_message(3);
        }
    }
}

// check if given name is valid and return true if so, else return false
// name is the name we are checking
bool valid_name(char *name) {
    char bannedChars[4] = {' ', '\n', '\r', ':'};
    for (int j = 0; j < 4; j++) { 
        if (char_in_sequence(name, bannedChars[j])) {
            return false;
        }
    }   
    return true;
}

// Check to see char is in sequence. Return true if so. Else return false
// sequence is the string we are checking, and input is the char we are looking
// for
bool char_in_sequence(char *sequence, char input) { 
    for (int i = 0; sequence[i] != '\0'; i++) {
	if (input == sequence[i]) {
            return true;
        }
    }
    return false;
}

// See if the given input is a valid number
// Type = 1 when after number there should be a :
// Type = 0 when there should not be any characters after number
int check_valid_number(char *input, int type) {
    char *invalidInput;
    if (!strcmp(input, "\0") || char_in_sequence(input, ' ') || 
            char_in_sequence(input, '\n') || char_in_sequence(input, '\r')) {
        return -1;
    }

    int inputValue = strtol(input, &invalidInput, 10);	
    errno = 0;
    if ((inputValue < 0) || (*invalidInput != '\0') || 
            (errno == ERANGE && (inputValue == LONG_MAX || 
            inputValue == LONG_MIN)) || 
            (errno != 0 && inputValue == 0)) {
        if (type == 0) {
            return -1;
        }
        if (invalidInput[0] != ':') {
            return -1;
        }
        return inputValue;
    }
    
    if (type) { 
        return -1;
    }
    return inputValue;
} 

// If the good is in the depot, return its index, else return -1
int good_at_depot(DepotContents *depotContents, char *name) {
    for (int i = 0; i < depotContents->numItems; i++) {
        if (!strcmp(name, depotContents->type[i])) {
            return i;
        }
    }
    return -1;
}

// Add a given amount of goods to the appropriate good type
void add_goods(DepotContents *depotContents, char *name, int quantity) {
    // If not already in list
    int index = good_at_depot(depotContents, name);
    if (index != -1) {
        take_lock(&depotContents->lock);
        depotContents->quantity[index] += quantity;
        release_lock(&depotContents->lock);
        return;
    }
    //remalloc every 10
    take_lock(&depotContents->lock);
    depotContents->numItems++;
    if (depotContents->numItems == depotContents->allocatedGoods) {
        // remalloc 
        depotContents->allocatedGoods += 10;
        int memSize = depotContents->allocatedGoods;
        char **tempType = realloc(depotContents->type, 
                memSize * sizeof(char *));
        int *tempQuantity = realloc(depotContents->quantity, 
                memSize * sizeof(int));
        if ((tempType == NULL) || (tempQuantity == NULL)) {
            //memory failure
            exit(99);
        } else {
            depotContents->type = tempType;
            depotContents->quantity = tempQuantity;
        }
    }
    
    depotContents->type[depotContents->numItems - 1] = malloc(strlen(name) 
            * sizeof(char));
    depotContents->type[depotContents->numItems - 1] = name;
    depotContents->quantity[depotContents->numItems - 1] = quantity;
    release_lock(&depotContents->lock);
}

// check if the given integer is in the given list
// listLength gives lenth of list, list is the list itself and
// item is the item we are checking is present in the list
// return true if list contains item, else false
bool list_contains(int listLength, int *list, int item) {
    for (int i = 0; i < listLength; i++) {
        if (list[i] == item) {
            return true;
        }
    }
    return false;
}

// Order and print the given list in lexographic order
// depotContents gives current state of the depot, listLength tells us the 
// length of the list and list is the list we are to print out, type 
// checks if we are printing neighbours or goods (1 = goods, 0 = neighbours)
void print_list(DepotContents *depotContents, int listLength, char **list, 
        int type) {
    take_lock(&depotContents->lock);
    //need to lexographically order the items
    char **orderedType = malloc(listLength * sizeof(char *));
    int *orderedQuantity = malloc(listLength * sizeof(int));
    int *alreadyOrdered = malloc(listLength * sizeof(int));
   
    for (int i = 0; i < listLength; i++) {
        alreadyOrdered[i] = -1;
    }  
    for (int i = 0; i < listLength; i++) {
        int minIndex;
        // Find first element in list which hasnt already been added
        for (int index = 0; index < listLength; index++) {
            if (!list_contains(listLength, alreadyOrdered, 
                    index)) {
                minIndex = index;
                break;
            }
        } 
        // Compare this element to every other in list, to find minimum
        for (int j = 0; j < listLength; j++) {
            if (minIndex != j && !list_contains(listLength, 
                    alreadyOrdered, j)) {
                int comparison = strcmp(list[minIndex], list[j]);
                if (comparison >= 0) {
                    minIndex = j;
                }  
            }
        }
        alreadyOrdered[i] = minIndex;
        orderedType[i] = list[minIndex];
        if (type) {
            orderedQuantity[i] = depotContents->quantity[minIndex];
        }
    }  
    for (int i = 0; i < listLength; i++) {
        if (type && orderedQuantity[i] == 0) {
            continue;
        } 
        printf("%s", orderedType[i]);
        if (type) {
            printf(" %d", orderedQuantity[i]);
        }
        printf("\n");
    }
    free(orderedType);
    free(orderedQuantity);
    free(alreadyOrdered);
    release_lock(&depotContents->lock);
}

// Run the depot server which can be connected to
// This function will be continuously running to check for new connections
// depotContents gives the current state of the depot
void run_server(DepotContents *depotContents) {
    struct addrinfo *ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;        // IPv6  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;  // Because we want to bind with it    
    int err;
    if ((err = getaddrinfo("127.0.0.1", 0, &hints, &ai))) {
        freeaddrinfo(ai);
        return;   // could not work out the address
    }   
    // create a socket and bind it to a port
    int serv = socket(AF_INET, SOCK_STREAM, 0); // 0 == use default protocol
    if (bind(serv, (struct sockaddr *)ai->ai_addr, sizeof(struct sockaddr))) {
        return;
    }
             
    // Which port did we get?
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(serv, (struct sockaddr *)&ad, &len)) {
        return;
    }
      
    take_lock(&depotContents->lock);
    printf("%u\n", ntohs(ad.sin_port));
    fflush(stdout);  
    depotContents->port = ntohs(ad.sin_port);          
    release_lock(&depotContents->lock); 

    if (listen(serv, 10)) {     // allow up to 10 connection requests to queue
        perror("Listen");
        exit(4);
    }                                                            
    int connFd; 
    while (connFd = accept(serv, 0, 0), connFd >= 0) {
        take_lock(&depotContents->lock);
        int connFdDup = dup(connFd);
        depotContents->rstream[depotContents->numConnections] = 
                fdopen(connFd, "r");
        depotContents->wstream[depotContents->numConnections] =
                fdopen(connFdDup, "w");
        depotContents->numConnections++;
        release_lock(&depotContents->lock);
        pthread_create(&depotContents->tid[depotContents->numConnections - 1],
                NULL, handle_connection, (void *)depotContents);
    }
}   

// Thread wraper function to handle incoming connection
// v is a depotContents pointer cast to a void pointer
void *handle_connection(void *v) {
    DepotContents *depotContents = (DepotContents *)v;
    take_lock(&depotContents->lock);
    if (depotContents->numConnections == depotContents->allocatedConnections) {
        depotContents->allocatedConnections += 10;
        depotContents->rstream = realloc(depotContents->rstream,
                depotContents->allocatedConnections * sizeof(FILE *));
        depotContents->tid = realloc(depotContents->tid, 
                depotContents->allocatedConnections * sizeof(pthread_t));
        depotContents->wstream = realloc(depotContents->wstream,
                depotContents->allocatedConnections * sizeof(FILE *));
        depotContents->messageSent = realloc(depotContents->messageSent,
                depotContents->allocatedConnections * sizeof(bool));
    }    
    release_lock(&depotContents->lock);
    read_from_stream(depotContents, depotContents->
            rstream[get_tid(depotContents)], true);
    take_lock(&depotContents->lock);
    fclose(depotContents->rstream[get_tid(depotContents)]);
    fclose(depotContents->wstream[get_tid(depotContents)]);
    release_lock(&depotContents->lock);
    return NULL;
}

// Get the index ID of the current thread
// depotContents gives current state of the depot
int get_tid(DepotContents *depotContents) {
    for (int i = 0; i < 10; i++) { 
        if (pthread_self() == depotContents->tid[i]) {
            return i;
        }
    }
    return -1;
}

// Read a line from the given stream (a message to the depot)
// depotContents gives current state of depot, stream is the stream we
// wish to read from and initial tells us if this is a new connection
void read_from_stream(DepotContents *depotContents, FILE *stream, 
        bool initial) {
    int c = EOF;
    int currentSize = 100;
    char *message = malloc(100 * sizeof(char));   
    int i = 0;  
    while (c = fgetc(stream), c != '\n') {
        message[i++] = (char)c; 
        if (i == currentSize) {
            currentSize += 100;
            message = realloc(message, currentSize * sizeof(char));
        }
    } 
    fflush(stream);
    message[i] = '\0';
    interpret_message(depotContents, message, initial);
 
    if (c != EOF) {
        read_from_stream(depotContents, stream, false);
    }
}

// Interpret a given message and send it to currect function
// depotContent gives current state, message is the message we are
// interpretting and initial tells us if this is the first message
// from a new connection
void interpret_message(DepotContents *depotContents, char *message, 
        bool initial) {
    if (initial && strncmp(message, "IM:", 3)) {   
        return;
    }    
    if (!strncmp(message, "Connect:", 8)) { 
        message += 8;
        connect_depots(depotContents, message);
    } else if (!strncmp(message, "IM:", 3)) {        
        if (!initial) {
            return;
        }
        message += 3;
        add_neighbour(depotContents, message);
    } else if (!strncmp(message, "Deliver:", 8)) {
        message += 8;
        move_items(depotContents, message, 1);
    } else if (!strncmp(message, "Withdraw:", 9)) {
        message += 9;
        move_items(depotContents, message, -1); 
    } else if (!strncmp(message, "Transfer:", 9)) {
        message += 9;
        transfer(depotContents, message);
    } else if (!strncmp(message, "Defer:", 6)) {
        message += 6;
        defer_message(depotContents, message);
    } else if (!strncmp(message, "Execute:", 8)) {
        message += 8;
        execute_message(depotContents, message);
    }
} 

// A function to handle moving items, type: 1 = addition, -1 = removal
// depotContents gives current state of depot and message is the received
// info from another depot
void move_items(DepotContents *depotContents, char *message, int type) {
    int quantity = check_valid_number(message, 1);
    if (quantity < 0) {
        return;
    }
    if (quantity != -1) {
        while (message[0] != ':') {
            message++;
        }
        //check valid name of good
        message++;
        if (valid_name(message)) {
            if (type == 1) {
                // Deliver
                add_goods(depotContents, message, quantity);
            } else if (type == -1) {
                // Withdraw
                add_goods(depotContents, message, 0 - quantity);  
            }
        }
    }
}

// Check to see if the given port is a new connection or not
// depotContents gives current state of depot and port is the
// port which we are checking
bool new_port(DepotContents *depotContents, int port) {
    for (int i = 0; i < depotContents->numNeighbours; i++) {
        if (depotContents->neighbourPorts[i] == port) {
            return false;
        }
    } 
    return true;
}

// Check if the given key is new (i.e hasn't been used before)
// newKey is variable we set to true if it is new, and keyIndex
// gives the index of the key if it is not a new key
void new_key(DepotContents *depotContents, bool *newKey, int *keyIndex, 
        int key) {
    for (int i = 0; i < depotContents->numDeferredMessages; i++) {
        if (depotContents->deferredMessages[i].key == key) {
            *newKey = false;
            *keyIndex = i;
            break;
        }
    }
}

// A function to store deferred messages
// depotContents gives current state of depot and message is the recieved
// info from another depot
void defer_message(DepotContents *depotContents, char *message) {
    int key = check_valid_number(message, 1);
    if (key < 0) {
        return;
    }
    while (message[0] != ':') {
        message++;
    }    
    message++;
    bool newKey = true;
    int keyIndex = 0;
    for (int i = 0; i < depotContents->numDeferredMessages; i++) {
        if (depotContents->deferredMessages[i].key == key) {
            newKey = false;
            keyIndex = i;
            break;
        }
    }
    take_lock(&depotContents->lock);
    if (newKey) {       
        depotContents->numDeferredMessages++;
        if (depotContents->numDeferredMessages == depotContents->
                allocatedDeferredMessages) {
            depotContents->allocatedDeferredMessages += 10;
            depotContents->deferredMessages = 
                    realloc(depotContents->deferredMessages, 
                    depotContents->allocatedDeferredMessages * sizeof(char *));
        }
        int messageIndex = depotContents->numDeferredMessages - 1;
        depotContents->deferredMessages[messageIndex].allocatedMessages = 10; 
        depotContents->deferredMessages[messageIndex].messages = malloc(10 *
                sizeof(char *));
        depotContents->deferredMessages[messageIndex].currentIndex = 0;
        depotContents->deferredMessages[messageIndex].messages[0] = message;
        depotContents->deferredMessages[messageIndex].key = key;
        depotContents->deferredMessages[messageIndex].numMessages = 1;
    } else {
        if (depotContents->deferredMessages[keyIndex].numMessages + 1 == 
                depotContents->deferredMessages[keyIndex].allocatedMessages) {
            depotContents->deferredMessages[keyIndex].allocatedMessages += 10;
            depotContents->deferredMessages[keyIndex].messages = realloc(
                    depotContents->deferredMessages[keyIndex].messages,
                    depotContents->deferredMessages[keyIndex].allocatedMessages
                    * sizeof(char *));
        }
        depotContents->deferredMessages[keyIndex].messages[depotContents->
                deferredMessages[keyIndex].numMessages++] = message;
    }
    release_lock(&depotContents->lock); 
}

// Function to execute a message
// depotContents gives current state of depot and message is the recieved
// info from another depot
void execute_message(DepotContents *depotContents, char *message) {
    int key = check_valid_number(message, 0);
    if (key < 0) {
        return;
    } 
    for (int i = 0; i < depotContents->numDeferredMessages; i++) {
        if (depotContents->deferredMessages[i].key == key) {
            // we have found the deferredMessage to execute
            if (depotContents->deferredMessages[i].numMessages == 
                    depotContents->deferredMessages[i].currentIndex) {
                return;            
            }  
            for (int j = depotContents->deferredMessages[i].currentIndex; 
                    j < depotContents->deferredMessages[i].numMessages; j++) {
                interpret_message(depotContents, 
                        depotContents->deferredMessages[i].messages[j], false);
            }
            take_lock(&depotContents->lock);
            depotContents->deferredMessages[i].currentIndex += 
                    depotContents->deferredMessages[i].numMessages - 
                    depotContents->deferredMessages[i].currentIndex;
            release_lock(&depotContents->lock);
            break;
        }
    }
}

// add a given neighbour to the list of known ports
// depotContents gives current state of depot and message is the recieved
// info from another depot
void add_neighbour(DepotContents *depotContents, char *message) {
    int port = check_valid_number(message, 1);
    take_lock(&depotContents->lock);
    if (port < 0 || !new_port(depotContents, port)) {
        release_lock(&depotContents->lock);
        return;
    }
    while (message[0] != ':') {
        message++;
    }    
    message++; 
    depotContents->numNeighbours++;
    if (depotContents->numNeighbours == depotContents->allocatedNeighbours) {
        depotContents->allocatedNeighbours += 10;
        depotContents->neighbours = realloc(depotContents->neighbours, 
                depotContents->allocatedNeighbours * sizeof(char *));
        depotContents->neighbourPorts = realloc(depotContents->neighbourPorts,
                depotContents->allocatedNeighbours * sizeof(int));
    }
    depotContents->neighbours[depotContents->numNeighbours - 1] = message;
    depotContents->neighbourPorts[depotContents->numNeighbours - 1] = port;
        
    //send IM back
    if (!depotContents->messageSent[get_tid(depotContents)]) {
        fprintf(depotContents->wstream[get_tid(depotContents)], "IM:%d:%s\n", 
                depotContents->port, depotContents->name);
        fflush(depotContents->wstream[get_tid(depotContents)]);
    }
    release_lock(&depotContents->lock);
}

// We have recieved a CONNECT message and must try to connect to new depot
// depotContents gives current state of depot and message is the recieved
// info from another depot
void connect_depots(DepotContents *depotContents, char *message) {
    //client code -> try and connect to server and wait for IM message    
    const char *port = message;
    int numPort = check_valid_number(message, 0);
    take_lock(&depotContents->lock);
    if (numPort < 0 || !new_port(depotContents, numPort)) {
        release_lock(&depotContents->lock);
        return;
    }
    release_lock(&depotContents->lock);

    struct addrinfo *ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;        // IPv6  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    int err;
    if ((err = getaddrinfo("127.0.0.1", port, &hints, &ai))) {
        freeaddrinfo(ai);
        return;   // could not work out the address
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0); // 0 == use default protocol
    if (connect(fd, (struct sockaddr *)ai->ai_addr, sizeof(struct sockaddr))) {
        return;
    }
    // fd is now connected
    // we want separate streams (which we can close independently)
    take_lock(&depotContents->lock);
    int fdDup = dup(fd);
    depotContents->rstream[depotContents->numConnections] = fdopen(fd, "r");
    depotContents->wstream[depotContents->numConnections] = fdopen(fdDup, "w");
    depotContents->messageSent[depotContents->numConnections] = true;
    fprintf(depotContents->wstream[depotContents->numConnections], 
            "IM:%d:%s\n", depotContents->port, depotContents->name);
    fflush(depotContents->wstream[depotContents->numConnections]);
    
    depotContents->numConnections++;
    release_lock(&depotContents->lock);
    pthread_create(&depotContents->tid[depotContents->numConnections - 1],
            NULL, handle_connection, (void *)depotContents);
}

// Transfer given goods from 1 depot to another
// depotContents is struct storing current depotContents
// Message contains goods, quantity and location to be transferred to in format
// of quantity:goods:location
void transfer(DepotContents *depotContents, char *message) {
    char *info = malloc(strlen(message) * sizeof(message));
    int index = 0;
    int counter = 0;
    while (counter < 2) {       
        if (message[index] == ':') {
            counter++;
        }
        info[index] = message[index]; 
        index++;     
    }
    // check valid number
    int quantity = check_valid_number(info, 1);
    if (quantity < 0) {
        return;
    }
    message += index;
    info[index - 1] = '\0';
    take_lock(&depotContents->lock);

    for (int i = 0; i < depotContents->numNeighbours; i++) {
        if (!strcmp(message, depotContents->neighbours[i])) {
            release_lock(&depotContents->lock);
            move_items(depotContents, info, -1);
            char *send = malloc(strlen(info) * sizeof(char));
            strcpy(send, info);
            send[index - 1] = '\n';
            send[index] = '\0';
            take_lock(&depotContents->lock);
            fprintf(depotContents->wstream[i], "Deliver:%s", send);
            fflush(depotContents->wstream[i]);
        }
    }
    release_lock(&depotContents->lock);
}
