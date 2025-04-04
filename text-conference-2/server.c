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
    struct user_cred *next;
};

/*
struct user_cred user_cred_list[] = { // THIS MAY NEED TO BECOME A LINKED LIST LATER ON
    {"bryan", "hello123"},
    {"fu", "pass123"},
    {"bob", "mysecret"},
};
*/

struct user_cred *user_list = NULL;

int username_exists(const char *client_id) {
    struct user_cred *current = user_list;
    while (current) {
        if (strcmp(current->client_id, client_id) == 0) {
            return 1;  
        }
        current = current->next;
    }
    return 0;  
}

void add_user(const char *client_id, const char *password) {
    // adds to head of linked list
    if (username_exists(client_id)) {
        printf("Error: Client ID '%s' already registered.\n", client_id);
        exit(1);
    }

    struct user_cred *new_user = (struct user_cred *) malloc(sizeof(struct user_cred));
    if (!new_user) {
        perror("Failed to allocate memory for new user.\n");
        exit(1);
    }

    strncpy(new_user->client_id, client_id, MAX_NAME - 1);
    new_user->client_id[MAX_NAME - 1] = '\0';

    strncpy(new_user->password, password, MAX_DATA - 1);
    new_user->password[MAX_DATA - 1] = '\0';

    new_user->next = user_list;
    user_list = new_user;
}

void print_users() {
    struct user_cred *current = user_list;
    printf("User List:\n");
    while (current) {
        printf("Client ID: %s, Password: %s\n", current->client_id, current->password);
        current = current->next;
    }
}

void free_user_list() {
    struct user_cred *current = user_list;
    while (current) {
        struct user_cred *temp = current;
        current = current->next;
        free(temp);
    }
    user_list = NULL;
}

#define CREDENTIALS_FILE "user_credentials.txt"

void write_credentials_to_file() {
    FILE *fp = fopen(CREDENTIALS_FILE, "w");
    if (!fp) {
        perror("Failed to open credentials file for writing");
        exit(1);
    }

    struct user_cred *current = user_list;
    while (current) {
        fprintf(fp, "%s:%s\n", current->client_id, current->password);
        current = current->next;
    }

    fclose(fp);
}

void read_credentials_from_file() {
    FILE *fp = fopen(CREDENTIALS_FILE, "r");
    if (!fp) {
        // file doesn't exist, create with default users
        fp = fopen(CREDENTIALS_FILE, "w");
        if (!fp) {
            perror("Failed to create credentials file");
            exit(1);
        }
        fprintf(fp, "bryan:hello123\n");
        fprintf(fp, "fu:pass123\n");
        fprintf(fp, "bob:mysecret\n");
        fclose(fp);
        
        // read from file just created
        fp = fopen(CREDENTIALS_FILE, "r");
        if (!fp) {
            perror("Failed to open credentials file for reading");
            exit(1);
        }
    }

    char line[MAX_NAME + MAX_DATA + 2]; // +2 for colon and newline
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        
        char *username = strtok(line, ":");
        char *password = strtok(NULL, ":");
        
        if (username && password) {
            add_user(username, password);
        }
    }

    fclose(fp);
}

void initialize_user_list() {
    read_credentials_from_file();
}

struct client_info {
    int client_socket;
    char client_id[MAX_NAME];
    struct sockaddr_in client_address;  // IP and port
    char session_id[MAX_NAME];          // note that we use MAX_NAME for session_id too
    struct client_info *next;           // linked list
    char backlog[MAX_DATA];
};

struct client_info *client_list = NULL;

void add_client(int client_socket, const char *client_id, struct sockaddr_in *client_address, const char *session_id) {
    // dynamically allocate new client
    struct client_info *new_client = malloc(sizeof(struct client_info));
    if (new_client == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for new client_info\n");
        exit(1);
    }

    new_client->client_socket = client_socket;
    strcpy(new_client->client_id, client_id);
    memcpy(&new_client->client_address, client_address, sizeof(struct sockaddr_in));
    strcpy(new_client->session_id, session_id);
    new_client->next = NULL;
    new_client->backlog[0] = '\0';

    // add to linked list
    if (client_list == NULL) { // first client in the list
        client_list = new_client;  
    } else {
        struct client_info *current = client_list;
        while (current->next != NULL) {
            current = current->next;  
        }
        current->next = new_client;  
    }
}

int remove_client(int client_socket) {
    // returns 1 if client was removed, 0 otherwise
    struct client_info *current = client_list;
    struct client_info *previous = NULL;

    while (current != NULL) {
        if (current->client_socket == client_socket) {
            if (previous == NULL) { // removing the head
                client_list = current->next;  
            } else {
                previous->next = current->next;  
            }
            free(current);  
            return 1;
        }
        previous = current;
        current = current->next;
    }

    // if we reach end
    // fprintf(stderr, "Error: client to remove not found\n");
    // exit(1);
    return 0;
}

void print_client_list() {
    struct client_info *current = client_list;  
    
    if (current == NULL) {
        printf("No clients connected.\n");
        return;
    }

    printf("--------------------------\n");
    printf("CURRENT CLIENTS:\n");
    printf("--------------------------\n");

    while (current != NULL) {
        char ip_address[INET_ADDRSTRLEN];  
        
        inet_ntop(AF_INET, &current->client_address.sin_addr, ip_address, INET_ADDRSTRLEN);
        
        printf("Socket FD: %d\n", current->client_socket);
        printf("Client ID: %s\n", current->client_id);
        printf("Session ID: %s\n", current->session_id);
        printf("IP Address: %s\n", ip_address);
        printf("Port: %d\n", ntohs(current->client_address.sin_port));  
        printf("--------------------------\n");
        
        current = current->next;  
    }
}

int session_exists(const char *session_id) { 
    struct client_info *current = client_list;
    while (current != NULL) {
        if (strcmp(current->session_id, session_id) == 0) { // session not empty
            return 1;  
        }
        current = current->next;
    }
    return 0;  // session empty
}

int client_exists(const char *client_id) {
    struct client_info *current = client_list;

    while (current != NULL) {
        if (strcmp(current->client_id, client_id) == 0) {
            return 1; 
        }
        current = current->next;
    }
    
    return 0; // No match found
}

void set_client_id(int client_socket, const char *client_id) {
    struct client_info *current = client_list;
    
    while (current != NULL) {
        if (current->client_socket == client_socket) {
            strncpy(current->client_id, client_id, sizeof(current->client_id) - 1);
            current->client_id[sizeof(current->client_id) - 1] = '\0'; // Ensure null termination
            return;
        }
        current = current->next;
    }

    fprintf(stderr, "Error: Client with socket %d not found, in set_client_id.\n", client_socket);
    exit(1);
}

int authenticate_user(const char *client_id, const char *password) {
    struct user_cred *current = user_list;

    while (current) {
        if (strcmp(current->client_id, client_id) == 0 && 
            strcmp(current->password, password) == 0) {
            return 1; // Authentication successful
        }
        current = current->next;
    }
    return 0; // Authentication failed
}

void remove_conn(int i, fd_set *master_ptr) {
    close(i);
    FD_CLR(i, master_ptr);

    int removed = remove_client(i); // rmb client list doesn't only store authenticated clients, stores all active connections
    if (removed) {
        printf("Client removed from list.\n");
    }
    print_client_list();
}

void handle_login(int i, const struct message *msg, fd_set *master_ptr) {
    // check if client_id already exists 
    if (client_exists((char *) msg->source)) {
        send_lonak(i, (char *) msg->source, "This client id is already logged in.");
        remove_conn(i, master_ptr); 
        return;
    }
    
    if (authenticate_user((char *)msg->source, (char *)msg->data)) {
        send_loack(i, (char *) msg->source);
        // set it's client id 
        set_client_id(i, (char *) msg->source);
        print_client_list();
    } else {
        send_lonak(i, (char *) msg->source, "Either user does not exist or password incorrect.");
        remove_conn(i, master_ptr);
    }
}

void handle_signup(int i, const struct message *msg, fd_set *master_ptr) {
    // check if this user already exists
    // if so, send THIS USER ALREADY EXISTS, TRY LOGGING IN INSTEAD
    if (username_exists((char *) msg->source)) {
        send_sunak(i, (char *) msg->source, "This client id already registered, try logging in instead.");
        remove_conn(i, master_ptr); 
        return;
    }
    
    // Add the new user
    add_user((char *)msg->source, (char *)msg->data);
    
    // Write updated credentials to file
    write_credentials_to_file();
    
    // Send success response
    send_suack(i, (char *)msg->source);
    set_client_id(i, (char *)msg->source);
    print_client_list();
}

void set_client_session(int client_socket, const char *session_id) {
    struct client_info *current = client_list;

    while (current != NULL) {
        if (current->client_socket == client_socket) {
            strncpy(current->session_id, session_id, sizeof(current->session_id) - 1);
            current->session_id[sizeof(current->session_id) - 1] = '\0'; 
            return;
        }
        current = current->next;
    }

    fprintf(stderr, "Error: Client with socket %d not found, in set_client_session.\n", client_socket);
    exit(1);
}

void send_to_session(const char *session_id, int sender_sock, const char *msgdata) {
    struct client_info *current = client_list;

    while (current != NULL) {
        if (strcmp(current->session_id, session_id) == 0 && current->client_socket != sender_sock) { 
            strcat(current->backlog, msgdata);
            strcat(current->backlog, "\n");
        }
        current = current->next;
    }
}

void send_to_user(const char *dest_user, const char *msgdata) {
    struct client_info *current = client_list;

    while (current != NULL) {
        if (strcmp(current->client_id, dest_user) == 0) { 
            strcat(current->backlog, msgdata);
            strcat(current->backlog, "\n");
            return;
        }
        current = current->next;
    }

    printf("Private message error: Client with name %s not found.\n", dest_user);
}

const char *get_client_session(int client_socket) {
    struct client_info *current = client_list;

    while (current != NULL) {
        if (current->client_socket == client_socket) {
            return current->session_id;
        }
        current = current->next;
    }

    fprintf(stderr, "Error: Client with socket %d not found, in get_client_session.\n", client_socket);
    exit(1);  
}

const char *get_client_backlog(int client_socket) {
    struct client_info *current = client_list;

    while (current != NULL) {
        if (current->client_socket == client_socket) {
            return current->backlog;
        }
        current = current->next;
    }

    fprintf(stderr, "Error: Client with socket %d not found, in get_client_session.\n", client_socket);
    exit(1);  
}

void delete_client_backlog(int client_socket) {
    struct client_info *current = client_list;

    while (current != NULL) {
        if (current->client_socket == client_socket) {
            current->backlog[0] = '\0';
            return;
        }
        current = current->next;
    }

    fprintf(stderr, "Error: Client with socket %d not found, in delete_client_backlog.\n", client_socket);
    exit(1);  
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    fd_set master; 
    fd_set read_fds;
    FD_ZERO(&master);    
    FD_ZERO(&read_fds);
    int fdmax;

    // bind the TCP port
    int listener_fd;
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

    FD_SET(listener_fd, &master); // add it to the set
    fdmax = listener_fd;

    printf(">>> Server now listening on port %d\n", port);

    initialize_user_list(); // initialize the user credentials
    print_users();

    struct sockaddr_storage remoteaddr; // the client's address

    while (1) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) { // block until one is ready to read
            perror("select");
            exit(1);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (!FD_ISSET(i, &read_fds)) {
                continue;
            }

            if (i == listener_fd) { // new client
                socklen_t addrlen = sizeof remoteaddr;
                int newfd = accept(listener_fd, (struct sockaddr *) &remoteaddr, &addrlen);

                if (newfd == -1) {
                    perror("accept");
                    exit(1);
                } 

                FD_SET(newfd, &master);
                if (newfd > fdmax) {  
                    fdmax = newfd;
                }

                add_client(newfd, "", (struct sockaddr_in *) &remoteaddr, "");
                printf("New client connected.\n");
                print_client_list();
            } else { // client socket activity
                struct message msg;
                memset(&msg, 0, sizeof(msg));
                int nbytes = receive_message(i, &msg);

                // print_message(&msg);

                if (nbytes <= 0) { // either hung up or errored
                    if (nbytes == 0) { // connection closes
                        printf("Client with fd %d hung up.\n", i);
                    } else {
                        fprintf(stderr, "Failed to receive client data.\n");
                        exit(1);
                    }
                    remove_conn(i, &master);
                } else { // got data from client

                    switch (msg.type) {
                        case LOGIN:
                            handle_login(i, &msg, &master);
                            break;
                        case SIGN_UP:
                            print_message(&msg);
                            handle_signup(i, &msg, &master);
                            break;
                        case EXIT:
                            remove_conn(i, &master);
                            break;                
                        case JOIN:
                            printf("Client %s requested to JOIN session: %s\n", msg.source, msg.data);

                            if (!session_exists((char *) msg.data)) {
                                send_joinnak(i, (char *) msg.source, (char *) msg.data, "Session does not exist.");
                            } else {
                                set_client_session(i, (char *) msg.data);
                                print_client_list();
                                send_joinack(i, (char *) msg.source, (char*) msg.data);  
                            }
                            break;
                
                        case LEAVE_SESS:
                            printf("Client %s requested to leave session.\n", (char *) msg.source);
                            set_client_session(i, "");
                            print_client_list();
                            break;
                        case NEW_SESS:
                            printf("Client requested to (create +) join session: %s\n", (char *) msg.data);
                            set_client_session(i, (char *) msg.data);
                            print_client_list();
                            send_newsessack(i, (char *) msg.source);
                            break;
                
                        case MESSAGE:
                            printf("Client %s trying to send message: %s\n", (char *) msg.source, (char *) msg.data);
                            send_to_session(get_client_session(i), i, (char *) msg.data);
                            break;
                        case QUERY:
                            printf("Client %s requested QUERY.\n", msg.source);

                            char user_list[MAX_DATA] = "Users and their Sessions\n-------------------\n";

                            struct client_info *current = client_list;
                            while (current != NULL) {
                                char entry[MAX_NAME + MAX_NAME + 40];

                                if (strlen(current->client_id) > 0) {
                                    if (strlen(current->session_id) > 0) {
                                        snprintf(entry, sizeof(entry), "%s (In session %s)\n", current->client_id, current->session_id);
                                    } else {
                                        snprintf(entry, sizeof(entry), "%s (No session)\n", current->client_id);
                                    }
                                    strcat(user_list, entry);
                                }
                                current = current->next;
                            }
                            send_quack(i, (char *) msg.source, user_list);
                            break;
                        
                        case GET_MSG:
                            printf("Client %s backlog query\n", (char *) msg.source);
                            send_usermsg(i, (char *) msg.source, get_client_backlog(i));
                            delete_client_backlog(i);
                            break;

                        case PRIV_MSG:
                            printf("Private message to %s\n", (char *) msg.source); // remember here source is the destination
                            send_to_user((char *) msg.source, (char *) msg.data);
                            break;

                        default:
                            printf("Unknown message type received: %d\n", msg.type);
                            break;
                    }                
                }

            }
        }

    }

    return 0;
}
