#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>

/* ==== STRUCTS ==== */

typedef struct client {
    int id;
    int socket;
    pthread_t *thread;
} client;

typedef struct Node {
    client* data;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    int size;
} LinkedList;

typedef struct thread_params {
    int current_client_socket;
    pthread_t *last_thread;
    int* current_id;
    int* active_clients_count;
    LinkedList* clients;
} thread_params;

/* ==== SOCKET HELPERS ==== */
int address_parser(const char* addressString, const char* portString, struct sockaddr_storage* storage);
void addrtostr(const struct sockaddr* addr, char* str, size_t strsize);
int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage* storage);

/* ==== COMMUNICATION HANDLING ==== */
int send_message(char* message, int sockfd);
int receiveMessage(char* message, int sockfd);
int broadcast_message(char* message, LinkedList* users, int exception_id);

/* ==== ERROR HANDLING ==== */
void logexit(char *msg);

/* ==== INPUT HANDLING ==== */
char** parseInput(char* input, int* numTokens);
int break_arguments(char* raw, char* arguments_vector[]);
int parse_message(char* raw, int* id1, int* id2, char* message, LinkedList* users);
int break_message_under_quotes(char* raw, char* message);

/* ==== LINKED LIST ==== */
void initLinkedList(LinkedList* list);
void insert(LinkedList* list, client *data);
void deleteById(LinkedList* list, int id);
client* getById(LinkedList* list, int id);
client* get_client_by_index(int index, LinkedList* users);
void display(LinkedList* list);


/* ==== UTILS ==== */
void format_time(char* formattedTime);
void build_message(char* builded_message, int author, int receiver, char* message);
void formatted_message(char* formatted, int author, int receiver, int broadcast, char* message);

#endif