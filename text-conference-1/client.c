#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "message.h"

char client_id[MAX_NAME];
int client_socket;

void remove_newline(char *str) {
    str[strcspn(str, "\n")] = '\0';
}

// void login() {

// }


int main() {



    char user_input[USER_INPUT_MAX_SIZE];

    printf(">>> ");

    while (fgets(user_input, USER_INPUT_MAX_SIZE, stdin)) {
        remove_newline(user_input);
        // printf("User inputted: %s\n", user_input);
        if (strncmp(user_input, "/login", 6) == 0) {
            char server_ip[INET_ADDRSTRLEN];
            int server_port;
            char password[MAX_DATA];
            
            if (sscanf(user_input, "/login %s %s %s %d", client_id, password, server_ip, &server_port) != 4) {
                printf("Please make sure format is: /login <client-id> <passwd> <server-ip> <server-port>\n");
                printf(">>> ");
                continue;
            }
            client_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (client_socket < 0) {
                perror("socket");
                exit(1);
            }
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(server_port);
            inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
            
            if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("connect");
                exit(1);
            }
            
            printf("Connected to server.\n");
            
            // send login message 
            printf("Sending login message...\n");

            send_login(client_socket, client_id, password);
            
            // receive the login ack, and report the result to user
            struct message response;
            memset(&response, 0, sizeof(response));
            if (receive_message(client_socket, &response) <= 0) {
                fprintf(stderr, "Failed to receive login ack.\n");
                exit(1);
            }

            print_message(&response);

        } else if (strncmp(user_input, "/logout", 7) == 0) {
            // send_message(EXIT, "");
            // close(client_socket);
            // printf("Logged out.\n");
        } else if (strncmp(user_input, "/joinsession", 12) == 0) {
            // char session_id[MAX_NAME];
            // sscanf(command, "/joinsession %s", session_id);
            // send_message(JOIN, session_id);
        } else if (strncmp(user_input, "/leavesession", 13) == 0) {
            // send_message(LEAVE_SESS, "");
        } else if (strncmp(user_input, "/createsession", 14) == 0) {
            // char session_id[MAX_NAME];
            // sscanf(command, "/createsession %s", session_id);
            // send_message(NEW_SESS, session_id);
        } else if (strncmp(user_input, "/list", 5) == 0) {
            // send_message(QUERY, "");
        } else if (strncmp(user_input, "/quit", 5) == 0) {
            // send_message(EXIT, "");
            // close(client_socket);
            // printf("Client exiting.\n");
            // exit(0);
        } else {
            // send_message(MESSAGE, command);
        }
        printf(">>> ");
    }

    return 0;
}