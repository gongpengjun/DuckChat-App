/*
 * properties.h
 * Author: Cole Vikupitz
 *
 * Properties to be used for the DuckChat client/server programs.
 */

#ifndef __PROPERTIES_H_
#define __PROPERTIES_H_


/* Suppresses compiler warnings for unused parameters */
#define UNUSED __attribute__((unused))

/* Maximum number of bytes for host to receive from another at a time */
#define BUFF_SIZE 150000

/* Maximum number of channels a client may be subscribed to at once */
#define MAX_CHANNELS 10

/* The timeout rate (in seconds) for the client to wait for verification from server */
/* Clients will send a verification packet before login to server for username uniqueness */
/* The client will wait this long for a server response, and exit if no response given */
/* Should be kept at 5-8 seconds */
#define TIMEOUT_RATE 8

/* The rate (in seconds) for the client to send a keep alive request */
/* Clients will send a keep alive request to prevent server from logging them out */
/* Should be kept between 45-60 seconds */
#define KEEP_ALIVE_RATE 60

/* Refresh rate (in minutes) of the server to refresh its internal data structures */
/* The server will scan and remove users who have not sent a packet past the refresh rate */
/* Inactive neighboring servers will also be removed from the routing table */
/* Should be kept between 2-5 minutes */
#define REFRESH_RATE 2

/* Size of the server's cache for storing the IDs of received S2S packets */
/* The server will cache this many IDs before replacing older ones */
#define MSGQ_SIZE 48

/* The name of the application's default channel */
/* Upon login, every client will send a join request for this channel */
/* The server will also never remove this channel, even when its empty */
#define DEFAULT_CHANNEL "Common"


#endif  /* __PROPERTIES_H_ */

