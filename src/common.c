#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>


#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "common.h"

#define FILESIZE 2248

/* ==== SOCKET HELPERS ==== */

int address_parser(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage) {

    if (addrstr == NULL || portstr == NULL) return -1;

    uint16_t port = (uint16_t)atoi(portstr);
    if (port == 0) return -1;
    port = htons(port); 

    struct in_addr inaddr4;
    if (inet_pton(AF_INET, addrstr, &inaddr4)) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;
        return 0;
    }

    struct in6_addr inaddr6; // 128-bit IPv6 address
    if (inet_pton(AF_INET6, addrstr, &inaddr6)) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
        memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));
        return 0;
    }

    return -1;
}

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize) {
    int version;
    char addrstr[INET6_ADDRSTRLEN + 1] = "";
    uint16_t port;

    if (addr->sa_family == AF_INET) {
        version = 4;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        if (!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr,
                       INET6_ADDRSTRLEN + 1)) {
            logexit("ntop");
        }
        port = ntohs(addr4->sin_port); // network to host short
    } else if (addr->sa_family == AF_INET6) {
        version = 6;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        if (!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr,
                       INET6_ADDRSTRLEN + 1)) {
            logexit("ntop");
        }
        port = ntohs(addr6->sin6_port); // network to host short
    } else {
        logexit("unknown protocol family.");
    }
    if (str) {
        snprintf(str, strsize, "IPv%d %s %hu", version, addrstr, port);
    }
}

int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage* storage){

    uint16_t port = (uint16_t)atoi(portstr);

    if(!port) return -1;
    port = htons(port);

    memset(storage, 0, sizeof(*storage));
    if(strcmp(proto, "v4") == 0){
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr.s_addr = INADDR_ANY;
        return 0;
    } else if (strcmp(proto, "v6") == 0){
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
        addr6->sin6_addr = in6addr_any;
        return 0;
    } else {
        return -1;
    }

    return 0;
} 

/* ==== COMMUNICATION HANDLING ==== */
int send_message(char* message, int sockfd){
    size_t count = 0;
    count = send(sockfd, message, strlen(message) + 1, 0);
    if(count != strlen(message) + 1) return -1;
    return 0;
}

int receiveMessage(char* message, int sockfd){
    size_t count = 0;
    count = recv(sockfd, message, FILESIZE-1, 0);
    if(count <= 0) return -1;
    return 0;
}

int broadcast_message(char* message, LinkedList *users, int exception_id){
    Node *current = users->head;
    while(current != NULL){
        if(current->data->id != exception_id) send_message(message, current->data->socket);
        current = current->next;
    }
    return 0;
}

/* ==== ERROR HANDLING ==== */
void logexit(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

/* ==== USER INPUT ==== */
char** parseInput(char* input, int* numTokens){
    char* cpyinput = strcpy(malloc(strlen(input) + 1), input);
    input[strlen(cpyinput) - 1] = '\0';
    char** tokens = malloc(FILESIZE * sizeof(char*));
    char* token;
    int index = 0;

    token = strtok(cpyinput, " ");
    while(token != NULL){
        tokens[index] = token;
        index++;
        token = strtok(NULL, " ");
    }
    tokens[index] = NULL;
    *numTokens = index;

    return tokens;
}

/* ==== LINKED LIST ==== */

void initLinkedList(LinkedList* list) {
    list->head = NULL;
    list->size = 0;
}

void insert(LinkedList* list, client *data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->data = data;
    newNode->next = NULL;

    if (list->head == NULL) {
        list->head = newNode;
    } else {
        Node* current = list->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }   
    list->size++;
}

void deleteById(LinkedList* list, int id) {
    if (list->head == NULL) {
        return;
    }

    Node* current = list->head;
    Node* prev = NULL;

    if (current->data->id == id) {
        list->head = current->next;
        free(current);
        return;
    }

    while (current != NULL && current->data->id != id) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        return;
    }

    prev->next = current->next;
    free(current);
    list->size--;
}

client* getById(LinkedList* list, int id) {
    Node* current = list->head;
    while (current != NULL) {
        if (current->data->id == id) {
            return current->data;
        }
        current = current->next;
    }
    return NULL;
}

client* get_client_by_index(int index, LinkedList* users){
    if(index < 0) index = users -> size + index;
    Node* current = users->head;
    int i = 0;
    while(current != NULL){
        if(i == index) return current->data;
        current = current->next;
        i++;
    }
    return NULL;
}

void display(LinkedList* list) {
    Node* current = list->head;

    while (current ->next != NULL) {
        printf("0%d ", current->data->id);
        current = current->next;
    }
    printf("0%d\n", current->data->id);
    
}

int parse_message(char* raw, int* id1, int* id2, char* message, LinkedList* users){
    char* cpyraw = strcpy(malloc(strlen(raw) + 1), raw);
    char* cpyraw2 = strcpy(malloc(strlen(raw) + 1), raw);

    /* ==== ZERO ARGUMENTS ==== */
    if(cpyraw == NULL) return 0;
    if(strcmp("REQ_ADD", cpyraw) == 0) return 1;
    if(strcmp("REQ_LIST", cpyraw) == 0) return 2;

    /* ==== HAS ARGUMENTS ====*/
    char* command = strtok(cpyraw, "(");
    char* arguments[FILESIZE];
    int numArguments = break_arguments(cpyraw, arguments);
    if(strcmp("MSG", command) == 0 && numArguments >= 3){
        break_message_under_quotes(cpyraw2, message);
        *id1 = atoi(arguments[0]);
        if(strcmp("NULL", arguments[1]) == 0) *id2 = -1;
        else *id2 = atoi(arguments[1]);

        if(break_message_under_quotes(cpyraw2, message) != 0){
            return -1;
        }
        return 3;

    } else if(strcmp("REQ_REM", command) == 0 && numArguments == 1){
        *id1 = atoi(arguments[0]);
        return 4;

    } else if(strcmp("OK", command) == 0 && numArguments == 1){
        *id1 = atoi(arguments[0]);
        return 7;

    } else if(strcmp("ERROR", command) == 0 && numArguments == 1){
        *id1 = atoi(arguments[0]);
        *id2 = -1;
        return 5;

    } else if(strcmp("RES_LIST", command) == 0){
        *id1 = atoi(arguments[0]);
        
        for(int i = 0; i < numArguments; i++){
            client* temp = malloc(sizeof(client));
            temp->id = atoi(arguments[i]);
            temp->socket = -1;
            
            insert(users, temp);
        }

        return 6;

    } else {
        return -1;
    }
}

int break_arguments(char* raw, char* arguments_vector[]){
    char* token = strtok(NULL, ",");
    int numTokens = 0;

    while (token != NULL) {
        int len = strlen(token);

        if (len >= 2 && token[len - 1] == ')') {
            token[len - 1] = '\0';
        }

        arguments_vector[numTokens] = malloc((len + 1) * sizeof(char));
        strcpy(arguments_vector[numTokens], token);

        token = strtok(NULL, ",");
        numTokens++;
    }

    return numTokens;
}

int break_message_under_quotes(char* raw, char* message){
    regex_t regex;
    regmatch_t match[2];

    // Compile the regular expression pattern
    if (regcomp(&regex, "\"([^\"]+)\"", REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex pattern.\n");
        return 1;
    }

    // Execute the regular expression
    if (regexec(&regex, raw, 2, match, 0) != 0) {
        return 1;
    }

    strncpy(message, raw + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
    message[match[1].rm_eo - match[1].rm_so] = '\0';
    return 0;
}

void format_time(char* formattedTime){

    time_t rawTime;
    struct tm* timeInfo;

    time(&rawTime);  // Get the current time
    timeInfo = localtime(&rawTime);  // Convert to local time

    strftime(formattedTime, sizeof(formattedTime), "%H:%M", timeInfo);  // Format the time
}

void build_message(char* builded_message, int author, int receiver, char* message){
    memset(builded_message, 0, FILESIZE);
    char* formattedTime = malloc(6 * sizeof(char));
    format_time(formattedTime);

    sprintf(builded_message, "MSG(%d,%d,\"[%s]%s\")", author, receiver, formattedTime, message);
}

void formatted_message(char* formatted, int author, int receiver, int broadcast, char* message){
    
    memset(formatted, 0, FILESIZE);
    char time[8];
    for(int i = 0; i < 7; i++) time[i] = message[i]; //copy time
    time[7] = '\0';
    

    char msg[FILESIZE];
    for(int i = 0; i < strlen(message) - 7; i++) msg[i] = message[i + 7]; //copy message
    msg[strlen(message) - 7] = '\0';

    if(author == receiver && broadcast){
        sprintf(formatted, "%s -> all %s", time, msg);
    } else {
        sprintf(formatted, "%s 0%d: %s", time, author, msg);
    }
}