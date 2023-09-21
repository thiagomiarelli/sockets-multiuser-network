#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "common.h"

int setup_server(int argc, char* argv[]);

/* ==== CONSTANTS ==== */

#define ADDR_SIZE 128
#define MESSAGE_SIZE 2248
#define MAX_CLIENTS 15

/* ==== AUX FUNCTIONS ==== */
int connect_client(int server_socket);
void *client_handler(void *arg);
void usage(int argc, char *argv[]);
int create_connection(thread_params* params);
int acknolege_new_member(client* new_member, LinkedList* users);
void generate_users_list(char* string_list, LinkedList* users);
void delete_client(int client_id, int origin_id, thread_params* params);
void do_server_actions(int action, char* message, int origin, int destination, thread_params* params);

/* ==== MAIN FUNCTION ==== */
int main(int argc, char *argv[]){

    int server_socket = setup_server(argc, argv);
    int active_clients_count = 0;
    int current_id = 0;

    LinkedList* clients = malloc(sizeof(LinkedList));
    initLinkedList(clients);

    while(1){
        int client_socket = connect_client(server_socket);

        thread_params* params = malloc(sizeof(thread_params));
        params -> current_client_socket = client_socket;
        params -> current_id = &current_id;
        params -> active_clients_count = &active_clients_count;
        params -> clients = clients;

        create_connection(params);
    }

    return 0;
}



void usage(int argc, char *argv[]) {
    printf("Usage: %s <v4|v6> <server port>\n", argv[0]);
    exit(1);
}

int setup_server(int argc, char* argv[]){
    if(argc < 3) usage(argc, argv);
    const char *protocol = argv[1];
    /* ====== SETTING UP ADDRESS AND SOCKET ====== */

    struct sockaddr_storage storage;

    if(server_sockaddr_init(argv[1], argv[2], &storage) != 0) usage(argc, argv);

    int sockfd = socket(storage.ss_family, SOCK_STREAM, 0);

    if (sockfd < 0) logexit("socket");
    int enable = 1;
    if (0 != setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) logexit("setsockopt");

	struct sockaddr *address = (struct sockaddr *)(&storage);
    socklen_t address_len = !strcmp(protocol, "v4") ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

    if(bind(sockfd, address, address_len) != 0) logexit("bind");
    if(listen(sockfd, 1) != 0) logexit("listen");

    char address_string[ADDR_SIZE];
    addrtostr(address, address_string, ADDR_SIZE);
    return sockfd;
}

int connect_client(int server_socket){
    struct sockaddr_storage client;
    struct sockaddr *clientAddress = (struct sockaddr *) &client;
    socklen_t clientAddressLen = sizeof(client);

    int clientfd = accept(server_socket, clientAddress, &clientAddressLen);
    if(clientfd == -1) logexit("accept");

    return clientfd;
}

void *client_handler(void* arg) {
    thread_params* params = (thread_params*) arg;

    int client_socket = params -> current_client_socket;
    int id = *params -> current_id;
    pthread_t *thread = params -> last_thread;

    client* client = malloc(sizeof(client));
    client -> id = id;
    client -> socket = client_socket;
    client -> thread = thread;


    insert(params -> clients, client);
    acknolege_new_member(client, params -> clients);

    char message[MESSAGE_SIZE];
    int origin = -1;
    int destination = -1;
    char raw_message[MESSAGE_SIZE];



    while(1){
        receiveMessage(raw_message, client_socket);
        int command = parse_message(raw_message, &origin, &destination, message, params ->clients);
        do_server_actions(command, raw_message, origin, destination, params);
    }


}

int create_connection(thread_params* params){

    int client_socket = params -> current_client_socket;
    int active_clients = *params -> active_clients_count;

    char message[MESSAGE_SIZE];
    receiveMessage(message, client_socket);

    if(strcmp(message, "REQ_ADD") != 0){
        send_message("ERROR(01)", client_socket);
        close(client_socket); 
        return -1;
    }


    if(active_clients >= MAX_CLIENTS){
        send_message("ERROR(01)", client_socket);
        close(client_socket); 
        return -1;
    }

    (*params -> active_clients_count)++;
    (*params -> current_id)++;
    
    printf("Client %d connected\n", *params -> current_id);


    pthread_t *thread = malloc(sizeof(pthread_t)); // allocate memory for pthread_t object
    params -> last_thread = thread;

    pthread_create(params -> last_thread, NULL, client_handler, params);
    return 0;
}

int acknolege_new_member(client* new_member, LinkedList* users){
    char acknolege_message[MESSAGE_SIZE];
    generate_users_list(acknolege_message, users);
    send_message(acknolege_message, new_member -> socket);

    char broadcast_message_content[MESSAGE_SIZE];
    sprintf(broadcast_message_content, "MSG(%d,NULL,\"User %d joined the group!\")", new_member -> id, new_member -> id);
    broadcast_message(broadcast_message_content, users, new_member -> id);

    return 0;
}

void generate_users_list(char* string_list, LinkedList* users){
    strcat(string_list, "RES_LIST(");

    Node* current = users -> head;
    int id;
    while(current -> next != NULL){
        id = current -> data -> id;
        char current_client_id[MESSAGE_SIZE];
        sprintf(current_client_id, "%d", id);
        strcat(string_list, current_client_id);
        strcat(string_list, ",");
           current = current -> next;
    }
    id = current -> data -> id;
    char current_client_id[MESSAGE_SIZE];
    sprintf(current_client_id, "%d", id);
    strcat(string_list, current_client_id);
    strcat(string_list, ")");
}

void do_server_actions(int action, char* message, int origin, int destination, thread_params* params){
    if(action == 3){
        if(destination == -1){
            broadcast_message(message, params -> clients, -1);
        }else{
            client* destination_client = getById(params -> clients, destination);
            if(destination_client == NULL){
                client* origin_client = getById(params -> clients, origin);
                send_message("ERROR(03)", origin_client -> socket);
                return;
            }
            send_message(message, destination_client -> socket);
        }
    } else if(action == 4){
        delete_client(origin, origin, params);
    
    }
}

void delete_client(int client_id, int origin_id, thread_params* params){

    client* client_to_delete = getById(params -> clients, client_id);
    if(client_to_delete == NULL){
        send_message("ERROR(02)", origin_id);  
        return;  
    }
    printf("User 0%d removed\n", client_id);
    (*params -> active_clients_count)--;
    char ok_message[10];
    sprintf(ok_message, "OK(%d)", client_id);
    send_message(ok_message, client_to_delete -> socket);
    char broadcast_message_content[MESSAGE_SIZE];
    sprintf(broadcast_message_content, "REQ_REM(%d)", client_id);
    broadcast_message(broadcast_message_content, params -> clients, origin_id);
    close(client_to_delete -> socket);
    deleteById(params -> clients, client_id);
    fflush(stdout);
    pthread_cancel(*(client_to_delete -> thread));
}