#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "message.h"

struct user_cred {
    char client_id[MAX_NAME];
    char password[MAX_DATA];
};

struct user_cred user_list[] = {
    {"bryan", "hello123"},
    {"fu", "pass123"},
    {"bob", "mysecret"},
};

int authenticate_user(const char *client_id, const char *password) {
    int num_clients = sizeof(user_list) / sizeof(user_list[0]);
    
    for (int i = 0; i < num_clients; i++) {
        if (strcmp(user_list[i].client_id, client_id) == 0 && 
            strcmp(user_list[i].password, password) == 0) {
            return 1; // Authentication successful
        }
    }
    return 0; // Authentication failed
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // bind the TCP port
    int listener_fd, new_socket;
    struct addrinfo hints, *ai_head, *ai_curr;

    int port = atoi(argv[1]);
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv;
    if ((rv = getaddrinfo(NULL, port_str, &hints, &ai_head)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (ai_curr = ai_head; ai_curr != NULL; ai_curr = ai_curr->ai_next) {
        listener_fd = socket(ai_curr->ai_family, ai_curr->ai_socktype, ai_curr->ai_protocol);
        if (listener_fd < 0) { // if socket call fails, try the next one
            continue;
        }

        // deal with address already in use error
        int yes = 1;
        setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener_fd, ai_curr->ai_addr, ai_curr->ai_addrlen) < 0) { // if bind fails, try next one
            close(listener_fd);
            continue;
        }
        break; // if we successfully socket() and bind()
    }
    if (ai_curr == NULL) {
        fprintf(stderr, "socket or bind failed\n");
        exit(1);
    }

    freeaddrinfo(ai_head);

    if (listen(listener_fd, 10) == -1) {
        perror("listen");
        exit(1);
    }

    printf(">>> Server now listening on port %d\n", port);

    while (1) {
        if ((new_socket = accept(listener_fd, NULL, NULL)) == -1) {
            perror("accept");
            exit(1);
        }
        printf("New client connected.\n");

        struct message login_msg;
        memset(&login_msg, 0, sizeof(login_msg));
        receive_message(new_socket, &login_msg);
        print_message(&login_msg);

        if (authenticate_user((char *)login_msg.source, (char *)login_msg.data)) {
            send_loack(new_socket, (char *) login_msg.source);
        } else {
            send_lonak(new_socket, (char *) login_msg.source);
        }

    }

    return 0;
}
