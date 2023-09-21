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

#define ADDR_SIZE 128
#define MESSAGE_SIZE 2248


/* ==== THREAD STRUCTS ==== */

typedef struct client_thread_params {
    int socket;
    int current_id;
    LinkedList* clients;
} client_thread_params;

/* ==== AUX FUNCTIONS ==== */

int setup_client(int argc, char* argv[]);
void usage(int argc, char *argv[]);
int connect_to_message_server(int argc, char *argv[], client_thread_params* params);
int handle_input(char* message, int *destiny_id);
void *active_thread (void* arg);
void *passive_thread (void* arg);
void do_active_command_action(int action, int destination_id, char* message, client_thread_params* params);
void do_passive_command_action(int action, char* message, int id1, int id2, client_thread_params* params);
int check_if_is_new_member(char* message);

/* ==== MAIN FUNCTION ==== */

int main(int argc, char *argv[]){

    client_thread_params* params = malloc(sizeof(client_thread_params));
    int client_sock = connect_to_message_server(argc, argv, params);
    if (client_sock < 0) return EXIT_FAILURE;

    pthread_t input_thread;
    pthread_create(&input_thread, NULL, active_thread, (void*) params);

    pthread_t output_thread;
    pthread_create(&output_thread, NULL, passive_thread, (void*) params);

    pthread_join(input_thread, NULL);
    pthread_join(output_thread, NULL);
}

int setup_client(int argc, char* argv[]){
    /* === SETTING UP ADDRESS AND SOCKET === */

    struct sockaddr_storage storage;
    if(address_parser(argv[1], argv[2], &storage)) usage(argc, argv);

    int sockfd = socket(storage.ss_family, SOCK_STREAM, 0);
    if (sockfd < 0) logexit("socket");

    struct sockaddr *address = (struct sockaddr *)(&storage);
    socklen_t address_len = address->sa_family ==  AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

    if(0 != connect(sockfd, address, address_len)) logexit("connect");
    
    char address_string[ADDR_SIZE];
    addrtostr(address, address_string, ADDR_SIZE);
    return sockfd;
}

void usage(int argc, char *argv[]) {
    printf("Usage: %s <server> <port>\n", argv[0]);
    exit(1);
}

int handle_input(char* message, int *destiny_id){
    char cpy_message[MESSAGE_SIZE];
    char command[MESSAGE_SIZE];
    if(!fgets(command, MESSAGE_SIZE, stdin)) return -1;
    strcpy(cpy_message, command);

    int num_tokens = 0;
    char** tokens = parseInput(command, &num_tokens);
    if(strcmp(cpy_message, "close connection\n") == 0) return 1;
    if(strcmp(cpy_message, "list users\n") == 0) return 2;
    if(strcmp(tokens[0], "send") == 0 && strcmp(tokens[1], "all") == 0 && num_tokens >= 3) {
        memset(message, 0, MESSAGE_SIZE);
        if(break_message_under_quotes(cpy_message, message) == 1) return -1;
        return 3;

    } else if (strcmp(tokens[0], "send") == 0 && strcmp(tokens[1], "to") == 0 && num_tokens >= 4) {
        *destiny_id = atoi(tokens[2]);
        memset(message, 0, MESSAGE_SIZE);
        if(break_message_under_quotes(cpy_message, message) == 1) return -1;
        return 4;
    } else {
        printf("Invalid command\n");
       return -1;
    }
    return 0;
}

int connect_to_message_server(int argc, char *argv[], client_thread_params* params){
    int client_socket = setup_client(argc, argv);
    send_message("REQ_ADD", client_socket);

    char response[MESSAGE_SIZE];
    memset(response, 0, MESSAGE_SIZE);
    receiveMessage(response, client_socket);

    if(strcmp(response, "ERROR(01)") == 0) {
        printf("User limit exceeded\n");
        return -1;
    }

    int discarted_id = 0;
    params -> current_id = 0;
    params -> socket = client_socket;
    params -> clients = malloc(sizeof(LinkedList));
    initLinkedList(params -> clients);

    parse_message(response, &params -> current_id, &discarted_id, response, params -> clients); //adiciona ids nas listas
    client *current = get_client_by_index(-1, params -> clients);

    params -> current_id = current -> id;

    printf("User 0%d joined the group!\n", current -> id);

    return client_socket;
}

void *active_thread (void* arg) {
    client_thread_params* params = (client_thread_params*) arg;

    int* destination_id = malloc(sizeof(int)); 

    char message[MESSAGE_SIZE];

    while(1){
        int input = handle_input(message, destination_id);
        if(input == -1) continue;
        do_active_command_action(input, *destination_id, message, params);
    } 

    return NULL;
}

void do_active_command_action(int action, int destination_id, char* message, client_thread_params* params){
    char formatted_message[MESSAGE_SIZE];

    switch(action){
        case 1:
            sprintf(formatted_message, "REQ_REM(%d)", params -> current_id);
            send_message(formatted_message, params -> socket);
            break;
        case 2:
            display(params -> clients);
            break;
        case 3:
            build_message(formatted_message, params -> current_id, -1, message);
            send_message(formatted_message, params -> socket);
            break;
        case 4:
            build_message(formatted_message, params -> current_id, destination_id, message);
            send_message(formatted_message, params -> socket);
            break;
        default:
            printf("Invalid command\n");
            break;
    }
}

void *passive_thread (void* arg) {
    client_thread_params* params = (client_thread_params*) arg;
    char message[MESSAGE_SIZE];
    char raw_data[MESSAGE_SIZE];

    while(1){
        memset(message, 0, MESSAGE_SIZE);
        memset(raw_data, 0, MESSAGE_SIZE);
        receiveMessage(raw_data, params->socket);
        
        
        int id1, id2;
        int command = parse_message(raw_data, &id1, &id2, message, params->clients);

        do_passive_command_action(command, message, id1, id2, params);
    } 

    return NULL;
}

void do_passive_command_action(int action, char* message, int id1, int id2, client_thread_params* params){
   char response[MESSAGE_SIZE];

   if(action == 3){
        if(check_if_is_new_member(message)){
            client* new = malloc(sizeof(client));
            new -> id = id1;
            new -> socket = -1;

            printf("User 0%d joined the group!\n", id1);

            insert(params -> clients, new);
            return;
        }

        formatted_message(response, id1, params -> current_id, id2 == -1, message);
        printf("%s\n", response);
    } else if (action == 5) {
        switch(id1)
        {
        case 1:
            printf("User limit exceeded\n");
            break;
        case 2:
            printf("User not found\n");
            break;
        case 3:
            printf("Receiver not found\n");
            break;
        default:
            break;
        }
    
    } else if (action == 7){
        printf("User 0%d left the group!\n", id1);
        close(params -> socket);
        exit(0);
    } else if(action == 4){
        printf("User 0%d left the group!\n", id1);
        deleteById(params -> clients, id1);
    }
    fflush(stdout);
}

int check_if_is_new_member(char* message){
    regex_t regex;
    regcomp(&regex, ".*joined the group!", REG_EXTENDED);

    int res = regexec(&regex, message, 0, NULL, 0);

    regfree(&regex);

    return res == 0;
}