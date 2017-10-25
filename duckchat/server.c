/**
 * server.c
 * Author: Cole Vikupitz
 *
 * Server side of a chat application using the DuckChat protocol. The server receives
 * and sends packets to and from clients using this protocol and handles each of the
 * packets accordingly.
 *
 * Usage: ./server domain_name port_num
 */

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "duckchat.h"
#include "hashmap.h"
#include "linkedlist.h"

/// FIXME - Ensure byte order, htonl/s()....
/// FIXME - Server mesage when error occurs
/// FIXME - Unique user in channel, joining twice results in 2 messages...

/* Suppress compiler warnings for unused parameters */
#define UNUSED __attribute__((unused))
/* Maximum buffer size for messages and packets */
#define BUFF_SIZE 10000
/* FIXME */
#define DEFAULT_CHANNEL "Common"
/* Refresh rate (in minutes) of the server to forcefully logout inactive users */
#define REFRESH_RATE 2

/**/
static struct sockaddr_in server;
/**/
static int socket_fd = -1;
/**/
static HashMap *users = NULL;
/**/
static HashMap *channels = NULL;


/**
 * FIXME
 */
typedef struct {
    struct sockaddr_in *addr;
    socklen_t len;
    LinkedList *channels;   /* List of channels */
    char *ip_addr;
    char *username;
} User;

/**
 * FIXME
 */
static User *malloc_user(const char *ip, const char *name, struct sockaddr_in *addr, socklen_t len) {

    User *new_user;
    if ((new_user = (User *)malloc(sizeof(User))) != NULL) {
	new_user->addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	new_user->channels = ll_create();
	new_user->ip_addr = (char *)malloc(strlen(ip) + 1);
	int name_len = ((strlen(name) > (USERNAME_MAX - 1)) ? (USERNAME_MAX - 1) : strlen(name));
	new_user->username = (char *)malloc(name_len + 1);

	if (new_user->addr == NULL || new_user->channels == NULL || 
	    new_user->ip_addr == NULL || new_user->username == NULL) {
	    if (new_user->addr != NULL) free(new_user->addr);
	    if (new_user->channels != NULL) ll_destroy(new_user->channels, free);
	    if (new_user->ip_addr != NULL) free(new_user->ip_addr);
	    if (new_user->username != NULL) free(new_user->username);
	    return NULL;
	}

	*new_user->addr = *addr;
	new_user->len = len;
	strcpy(new_user->ip_addr, ip);
	memcpy(new_user->username, name, name_len);
	new_user->username[name_len] = '\0';
    }

    return new_user;    
}

/**
 * FIXME
 */
static void free_user(User *user) {
    
    if (user != NULL) {
	free(user->addr);
	ll_destroy(user->channels, free);
	free(user->ip_addr);
	free(user->username);
	free(user);
    }
}

/**
 * FIXME
 */
static void print_log_message(const char *msg) {

    struct tm *timestamp;
    time_t timer;
    time(&timer);
    timestamp = localtime(&timer);
    fprintf(stdout, "[%02d/%02d/%d %02d:%02d] %s\n", (timestamp->tm_mon + 1),
		    timestamp->tm_mday, (1900 + timestamp->tm_year),
		    timestamp->tm_hour, timestamp->tm_min, msg);
}

/**
 * FIXME
 */
static void server_send_error(struct sockaddr_in *addr, socklen_t len, const char *msg) {
    
    struct text_error error_packet;
    char buffer[256];

    memset(&error_packet, 0, sizeof(error_packet));
    error_packet.txt_type = TXT_ERROR;
    strncpy(error_packet.txt_error, msg, (SAY_MAX - 1));
    sendto(socket_fd, &error_packet, sizeof(error_packet), 0,
		(struct sockaddr *)addr, len);

    sprintf(buffer, "Sent error message to %s:%d -> %s",
		inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), msg);
    print_log_message(buffer);
}

/**
 * FIXME
 */
static void server_login_request(const char *packet, char *client_ip, struct sockaddr_in *addr, socklen_t len) {

    User *user;
    char buffer[256];
    struct request_login *login_packet = (struct request_login *) packet;

    if ((user = malloc_user(client_ip, login_packet->req_username, addr, len)) == NULL) {
	server_send_error(addr, len, "Failed to log into the server.");
	return;
    }
    if (!hm_put(users, client_ip, user, NULL)) {
	server_send_error(addr, len, "Failed to log into the server.");
	free(user);
	return;
    }

    sprintf(buffer, "User %s logged in from %s", user->username, user->ip_addr);
    print_log_message(buffer);
}

/**
 * FIXME
 */
static void server_join_request(const char *packet, char *client_ip, struct sockaddr_in *addr, socklen_t len) {
    
    User *user, *temp;
    LinkedList *user_list;
    char *joined;
    char buffer[256];
    long i;
    struct request_join *join_packet = (struct request_join *) packet;

    if (!hm_get(users, client_ip, (void **)&user)) {
	server_send_error(addr, len, "You are not currently logged in.");
	return;
    }

    int ch_len = ((strlen(join_packet->req_channel) > (CHANNEL_MAX - 1)) ?
				(CHANNEL_MAX - 1) : strlen(join_packet->req_channel));
    if ((joined = (char *)malloc(ch_len + 1)) == NULL) {
	sprintf(buffer, "Failed to join the channel %s.", join_packet->req_channel);
	server_send_error(user->addr, user->len, buffer);
	return;
    }

    memcpy(joined, join_packet->req_channel, ch_len);
    joined[ch_len] = '\0';
    if (!ll_add(user->channels, joined)) {
	sprintf(buffer, "Failed to join the channel %s.", joined);
	server_send_error(user->addr, user->len, buffer);
	free(joined);
	return;
    }

    if (!hm_get(channels, joined, (void **)&user_list)) {

	if ((user_list = ll_create()) == NULL) {
	    sprintf(buffer, "Failed to join the channel %s.", join_packet->req_channel);
	    server_send_error(user->addr, user->len, buffer);
	    return;
	}
	if (!ll_add(user_list, user)) {
	    ll_destroy(user_list, NULL);
	    sprintf(buffer, "Failed to join the channel %s.", join_packet->req_channel);
	    server_send_error(user->addr, user->len, buffer);
	    return;
	}
	if (!hm_put(channels, joined, user_list, NULL)) {
	    ll_destroy(user_list, NULL);
	    sprintf(buffer, "Failed to join the channel %s.", join_packet->req_channel);
	    server_send_error(user->addr, user->len, buffer);
	    return;
	}
	sprintf(buffer, "User %s created the channel %s", user->username, joined);
	print_log_message(buffer);

    } else {

	for (i = 0L; i < ll_size(user_list); i++) { /// CHECK TO SEE IF ALREADY JOINED
	    (void)ll_get(user_list, i, (void **)&temp);
	    if (strcmp(user->ip_addr, temp->ip_addr) == 0)
		return;
	}
	
	if (!ll_add(user_list, user)) {
	    sprintf(buffer, "Failed to join the channel %s.", join_packet->req_channel);
	    server_send_error(user->addr, user->len, buffer);
	    return;
	}
    }

    sprintf(buffer, "User %s joined the channel %s", user->username, joined);
    print_log_message(buffer);
}

/**
 * FIXME
 */
static void server_leave_request(const char *packet, char *client_ip, struct sockaddr_in *addr, socklen_t len) {

    User *user, *tmp;
    LinkedList *user_list;
    char *ch;
    char channel[CHANNEL_MAX], buffer[256];
    int removed = 0;
    long i;
    struct request_leave *leave_packet = (struct request_leave *) packet;

    if (!hm_get(users, client_ip, (void **)&user)) {
	server_send_error(addr, len, "You are not currently logged in.");
	return;
    }

    memset(channel, 0, sizeof(channel));
    strncpy(channel, leave_packet->req_channel, (CHANNEL_MAX - 1));
    if (!hm_get(channels, channel, (void **)&user_list)) {
	sprintf(buffer, "No channel by the name %s", leave_packet->req_channel);
	server_send_error(user->addr, user->len, buffer);
	return;
    }

    for (i = 0L; i < ll_size(user->channels); i++) {
	(void)ll_get(user->channels, i, (void **)&ch);
	if (strcmp(channel, ch) == 0) {
	    ll_remove(user->channels, i, (void **)&ch);
	    free(ch);
	    removed = 1;
	    break;
	}
    }

    for (i = 0L; i < ll_size(user_list); i++) {
	(void)ll_get(user_list, i, (void **)&tmp);
	if (strcmp(user->ip_addr, tmp->ip_addr) == 0) {
	    (void)ll_remove(user_list, i, (void **)&tmp);
	    break;
	}
    }

    if (removed) {
	sprintf(buffer, "User %s left the channel %s", user->username, channel);
	print_log_message(buffer);
    } else {
	sprintf(buffer, "You are not subscribed to the channel %s", channel);
	server_send_error(user->addr, user->len, buffer);
	return;
    }

    if (ll_isEmpty(user_list) && strcmp(channel, DEFAULT_CHANNEL)) {
	(void)hm_remove(channels, channel, (void **)&user_list);
	ll_destroy(user_list, NULL);
	sprintf(buffer, "Removed the empty channel %s\n", channel);
	print_log_message(buffer);
    }
}

/**
 * FIXME
 */
static void server_say_request(const char *packet, char *client_ip) {
    
    User *user;
    User **listeners;
    LinkedList *ch_users;
    long i, len;
    char buffer[256];
    struct request_say *say_packet = (struct request_say *) packet;
    struct text_say msg_packet;

    if (!hm_get(users, client_ip, (void **)&user))
	return;
    if (!hm_get(channels, say_packet->req_channel, (void **)&ch_users))
	return;
    if ((listeners = (User **)ll_toArray(ch_users, &len)) == NULL)
	return;

    memset(&msg_packet, 0, sizeof(msg_packet));
    strncpy(msg_packet.txt_channel, say_packet->req_channel, (CHANNEL_MAX - 1));
    strncpy(msg_packet.txt_username, user->username, (USERNAME_MAX - 1));
    strncpy(msg_packet.txt_text, say_packet->req_text, (SAY_MAX - 1));

    for (i = 0L; i < len; i++)
	sendto(socket_fd, &msg_packet, sizeof(msg_packet), 0,
		(struct sockaddr *)listeners[i]->addr, listeners[i]->len);

    sprintf(buffer, "User %s said in channel %s -> %s", user->username,
		msg_packet.txt_channel, msg_packet.txt_text);
    print_log_message(buffer);
    free(listeners);
}

/**
 * FIXME
 */
static void server_list_request(char *client_ip) {

    User *user;
    long i, len;
    char **ch_list;
    char buffer[256];
    struct text_list *list_packet;

    if (!hm_get(users, client_ip, (void **)&user))
	return;
    if ((ch_list = hm_keyArray(channels, &len)) == NULL)
	return;

    int size = sizeof(struct text_list) + (sizeof(struct channel_info) * len);
    
    list_packet = malloc(size);
    memset(list_packet, 0, size);
    list_packet->txt_type = TXT_LIST;
    list_packet->txt_nchannels = (int)len;

    for (i = 0L; i < len; i++)
	strncpy(list_packet->txt_channels[i].ch_channel, ch_list[i], (CHANNEL_MAX - 1));

    sendto(socket_fd, list_packet, size, 0,
		(struct sockaddr *)user->addr, user->len);

    sprintf(buffer, "User %s listed available channels on server", user->username);
    print_log_message(buffer);

    free(ch_list);
    free(list_packet);
}

/**
 * FIXME
 */
static void server_who_request(const char *packet, char *client_ip) {

    User *user, **user_list;
    LinkedList *u_list;
    int size;
    long i, len;
    char buffer[256];
    struct text_who *msg_packet;
    struct request_who *who_packet = (struct request_who *) packet;

    if (!hm_get(users, client_ip, (void **)&user))
	return;//User not logged in
    if (!hm_get(channels, who_packet->req_channel, (void **)&u_list))
	return;//Channel non existent
    if ((user_list = (User **)ll_toArray(u_list, &len)) == NULL)
	return;///malloc error

    size = sizeof(struct text_who) + (sizeof(struct user_info) * len);
    
    msg_packet = malloc(size);
    memset(msg_packet, 0, size);
    msg_packet->txt_type = TXT_WHO;
    msg_packet->txt_nusernames = (int)len;
    strncpy(msg_packet->txt_channel, who_packet->req_channel, (CHANNEL_MAX - 1));

    for (i = 0L; i < len; i++)
	strncpy(msg_packet->txt_users[i].us_username, user_list[i]->username, (USERNAME_MAX - 1));

    sendto(socket_fd, msg_packet, size, 0,
		(struct sockaddr *)user->addr, user->len);

    sprintf(buffer, "User %s listed all users on channel %s", user->username, who_packet->req_channel);
    print_log_message(buffer);

    free(user_list);
    free(msg_packet);
}

/**
 * FIXME
 */
static void server_keep_alive_request(char *client_ip) {

    fprintf(stdout, "Received KEEP_ALIVE from %s\n", client_ip); 
}

/**
 * FIXME
 */
static void server_logout_request(char *client_ip) {

    User *user, *tmp;
    LinkedList *user_list;
    long i;
    char *channel;
    char buffer[256];

    if (!hm_remove(users, client_ip, (void **)&user))
	return;

    sprintf(buffer, "User %s logged out", user->username);
    print_log_message(buffer);

    while (ll_removeFirst(user->channels, (void **)&channel)) {
	if (!hm_get(channels, channel, (void **)&user_list)) {
	    free(channel);
	    continue;
	}
	for (i = 0L; i < ll_size(user_list); i++) {
	    (void)ll_get(user_list, i, (void **)&tmp);
	    if (strcmp(user->ip_addr, tmp->ip_addr) == 0) {
		(void)ll_remove(user_list, i, (void **)&tmp);
		break;
	    }
	}
	if (ll_isEmpty(user_list) && strcmp(channel, DEFAULT_CHANNEL)) {
	    (void)hm_remove(channels, channel, (void **)&user_list);
	    ll_destroy(user_list, NULL);
	    sprintf(buffer, "Removed the empty channel %s", channel);
	    print_log_message(buffer);
	}
	free(channel);
    }
    free_user(user);
}

/**
 * FIXME
 */
static void free_ll(LinkedList *ll) {
    
    if (ll != NULL)
	ll_destroy(ll, NULL);
}

/**
 * FIXME
 */
static void cleanup(void) {
    
    if (socket_fd != -1)
	close(socket_fd);
    if (channels != NULL)
	hm_destroy(channels, (void *)free_ll);
    if (users != NULL)
	hm_destroy(users, (void *)free_user);
}

/**
 * Prints the specified message to standard error stream as a program error
 * message, then terminates the server application.
 */
static void print_error(const char *msg) {
    
    fprintf(stderr, "Server: %s\n", msg);
    exit(0);
}

/**
 * FIXME
 */
static void sig_handler(UNUSED int signo) {
    
    fprintf(stdout, "\n\nShutting down server...\n\n");
    exit(0);
}

/**
 * Runs the Duckchat server.
 */
int main(int argc, char *argv[]) {

    struct sockaddr_in client;
    struct hostent *host_end;
    socklen_t addr_len = sizeof(client);
    int port_num;
    char buffer[BUFF_SIZE], client_ip[128];

    /* Assert that the correct number of arguments were given */
    /* Print program usage otherwise */
    if (argc != 3) {
	fprintf(stdout, "Usage: %s domain_name port_num\n", argv[0]);
	return 0;
    }

    /* Register function to cleanup when user stops the server */
    /* Also register the cleanup() function to be invoked upon program termination */
    if (signal(SIGINT, sig_handler))
	print_error("Failed to catch SIGINT.");
    if ((atexit(cleanup)) != 0)
	print_error("Call to atexit() failed.");

    /* Assert that path name to unix domain socket does not exceed maximum allowed */
    /* Print error message and exit otherwise */
    /* Maximum length is specified in duckchat.h */
    if (strlen(argv[1]) > UNIX_PATH_MAX) {
	sprintf(buffer, "Path name to domain socket length exceeds the length allowed (%d).",
			UNIX_PATH_MAX);
	print_error(buffer);
    }

    /* Parse port number given by user, assert that it is in valid range */
    /* Print error message and exit otherwise */
    /* Port numbers typically go up to 65535 (0-1024 for privileged services) */
    port_num = atoi(argv[2]);
    if (port_num < 0 || port_num > 65535)
	print_error("Server socket must be in the range [0, 65535].");

    /* Obtain the address of the specified host */
    if ((host_end = gethostbyname(argv[1])) == NULL)
	print_error("Failed to locate the host.");

    /* Create server address struct, set internet family, address, & port number */
    memset((char *)&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    memcpy((char *)host_end->h_addr, (char *)&server.sin_addr.s_addr, host_end->h_length);
    server.sin_port = htons(port_num);

    /* Create the UDP socket, bind name to socket */
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	print_error("Failed to create a socket for the server.");
    if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
	print_error("Failed to assign the requested address.");

    /* Create & initialize ADTs for server to use */
    if ((users = hm_create(100L, 0.0f)) == NULL)
	print_error("Failed to allocate a sufficient amount of memory.");
    if ((channels = hm_create(100L, 0.0f)) == NULL)
	print_error("Failed to allocate a sufficient amount of memory.");
    LinkedList *default_ll;
    if ((default_ll = ll_create()) == NULL)
	print_error("Failed to allocate a sufficient amount of memory.");
    if (!hm_put(channels, DEFAULT_CHANNEL, default_ll, NULL))
	print_error("Failed to allocate a sufficient amount of memory.");

    /* Display successful launch title, timestamp & address */
    time_t timer;
    time(&timer);
    fprintf(stdout, "------ Launched DuckChat server ~ %s", ctime(&timer)); 
    fprintf(stdout, "------ Server assigned to address %s:%d\n", inet_ntoa(server.sin_addr),
	    ntohs(server.sin_port));

    /**
     * FIXME
     */
    while (1) {
    
	memset(buffer, 0, sizeof(buffer));
	recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client, &addr_len);
	sprintf(client_ip, "%s:%d", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
	
	struct text *packet_type = (struct text *) buffer;
	switch (packet_type->txt_type) {
	    case REQ_LOGIN:	/**/
		server_login_request(buffer, client_ip, &client, addr_len);
		break;
	    case REQ_LOGOUT:	/**/
		server_logout_request(client_ip);
		break;
	    case REQ_JOIN:  /**/
		server_join_request(buffer, client_ip, &client, addr_len);
		break;
	    case REQ_LEAVE: /**/
		server_leave_request(buffer, client_ip, &client, addr_len);
		break;
	    case REQ_SAY:   /**/
		server_say_request(buffer, client_ip);
		break;
	    case REQ_LIST:  /**/
		server_list_request(client_ip);
		break;
	    case REQ_WHO:   /**/
		server_who_request(buffer, client_ip);
		break;
	    case REQ_KEEP_ALIVE:
		server_keep_alive_request(client_ip);
	    default:	/* Do nothing, likey a bogus packet */
		break;
	}
    }

    return 0;
}
