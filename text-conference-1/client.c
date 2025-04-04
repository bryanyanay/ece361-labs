#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "message.h"

#include <pthread.h>

char client_id[MAX_NAME];
char session_id[MAX_NAME];
int client_socket;
int logged_in = 0;
int in_session = 0;

void remove_newline(char *str) {
    str[strcspn(str, "\n")] = '\0';
}

pthread_mutex_t lock;
int polling_active = 0;
pthread_t poll_thread;

void *poll_messages(/*void *arg*/) {
    while (polling_active) {
        pthread_mutex_lock(&lock);
        if (in_session) {
            send_getmsg(client_socket, client_id);
            struct message response;
            memset(&response, 0, sizeof(response));
            if (receive_message(client_socket, &response) > 0) {
                if (response.type == MESSAGE) {
                    if (strlen((char *)response.data) > 0) {
                        printf("\nReceived message: %s\n>>> ", response.data);
                        fflush(stdout);
                    }
                }
            }
        }
        pthread_mutex_unlock(&lock);
        sleep(3);
    }
    return NULL;
}

void start_polling() {
    polling_active = 1;
    pthread_create(&poll_thread, NULL, poll_messages, NULL);
}

void stop_polling() {
    polling_active = 0;
    pthread_join(poll_thread, NULL);
}

int main() {

    char user_input[USER_INPUT_MAX_SIZE];
    logged_in = 0;

    pthread_mutex_init(&lock, NULL);

    printf(">>> ");

    while (fgets(user_input, USER_INPUT_MAX_SIZE, stdin)) {
        remove_newline(user_input);
        pthread_mutex_lock(&lock);

        if (strncmp(user_input, "/login", 6) == 0) {
            char server_ip[INET_ADDRSTRLEN];
            int server_port;
            char password[MAX_DATA];
            
            // check that they aren't already logged in
            if (logged_in) {
                printf("You are already logged in to a server as %s, please log out first.\n", client_id);
                printf(">>> ");
                continue;
            }
            // check if format is right
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

            if (response.type == LO_ACK) {
                logged_in = 1;
                printf("Successfully logged in as %s.\n", client_id);
            } else if (response.type == LO_NAK) {
                printf("Error: %s\n", (char *) response.data);
            } else {
                printf("Received incorrect response type:\n");
                print_message(&response);
            }
        } else if (strncmp(user_input, "/logout", 7) == 0) {
            if (!logged_in) {
                printf("You are not logged in yet.\n");
                printf(">>> ");
                continue;
            }
            send_exit(client_socket, client_id);
            printf("Logged out of server.\n");
            close(client_socket);
            logged_in = 0;
            in_session = 0;
            if (in_session) {
                pthread_mutex_unlock(&lock);
                stop_polling();
            }
        } else if (strncmp(user_input, "/joinsession", 12) == 0) {
            if (!logged_in) {
                printf("You must be logged in to join a session.\n");
                printf(">>> ");
                continue;
            }
            if (in_session) {
                printf("Already in session %s, leave it first.\n", session_id);
                printf(">>> ");
                continue;
            }
            if (sscanf(user_input, "/joinsession %s", session_id) != 1) {
                printf("Please make sure format is: /joinsession <session-id>\n");
                printf(">>> ");
                continue;
            }

            // printf("before join");
            send_join(client_socket, client_id, session_id);
            // printf("after join");

            struct message response;
            memset(&response, 0, sizeof(response));
            if (receive_message(client_socket, &response) <= 0) {
                fprintf(stderr, "Failed to receive session join response.\n");
                exit(1);
            }

            if (response.type == JN_ACK) {
                in_session = 1;
                printf("Successfully joined session: %s\n", session_id);
                start_polling();
            } else if (response.type == JN_NAK) {
                printf("Error joining session: %s\n", response.data);
            } else {
                printf("Unexpected response type:\n");
                print_message(&response);
            }
        } else if (strncmp(user_input, "/leavesession", 13) == 0) {
            if (!in_session) {
                printf("You are not in a session yet.\n");
                printf(">>> ");
                continue;
            }
            send_leavesess(client_socket, client_id);
            in_session = 0;
            pthread_mutex_unlock(&lock);
            stop_polling();
        } else if (strncmp(user_input, "/createsession", 14) == 0) {
            if (!logged_in) {
                printf("You are not logged in yet.\n");
                printf(">>> ");
                continue;
            }
            if (in_session) {
                printf("Already in session %s, leave it first.\n", session_id);
                printf(">>> ");
                continue;
            }

            if (sscanf(user_input, "/createsession %s", session_id) != 1) {
                printf("Please make sure format is: /createsession <session-id>\n");
                printf(">>> ");
                continue;
            }

            // if session already exists, just join it
            send_newsess(client_socket, client_id, session_id);

            struct message response;
            memset(&response, 0, sizeof(response));
            if (receive_message(client_socket, &response) <= 0) {
                fprintf(stderr, "Failed to receive login ack.\n");
                exit(1);
            }

            if (response.type == NS_ACK) {
                in_session = 1;
                printf("Successfully (created and) joined session %s.\n", session_id);
                start_polling();
            } else {
                printf("Received incorrect response type:\n");
                print_message(&response);
            }
        } else if (strncmp(user_input, "/list", 5) == 0) {
            if (!logged_in) {
                printf("You must be logged in to use this command.\n");
                printf(">>> ");
                continue;
            }

            send_query(client_socket, client_id);

            struct message response;
            memset(&response, 0, sizeof(response));
            if (receive_message(client_socket, &response) <= 0) {
                fprintf(stderr, "Failed to receive list response.\n");
                exit(1);
            }

            if (response.type == QU_ACK) {
                printf("%s\n", response.data);
            } else {
                printf("Received incorrect response type:\n");
                print_message(&response);
            }
        } else if (strncmp(user_input, "/quit", 5) == 0) {
            if (logged_in) {
                send_exit(client_socket, client_id);
                printf("Logged out of server.\n");
                close(client_socket);
                logged_in = 0;
                in_session = 0;
            }
            if (in_session) {
                pthread_mutex_unlock(&lock);
                stop_polling();
            }
            exit(0);
        } else {
            if (!logged_in) {
                printf("You must be logged in to use this command.\n");
                printf(">>> ");
                continue;
            }
            if (!in_session) {
                printf("Not in session yet.\n");
                printf(">>> ");
                continue;
            }
            send_usermsg(client_socket, client_id, user_input);
        }
        printf(">>> ");
        pthread_mutex_unlock(&lock);
    }

    return 0;
}