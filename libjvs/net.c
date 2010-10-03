/*
 * Provides a simplified interface to TCP/IP networking.
 *
 * Copyright:   (c) 2007 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:     $Id: net.c 245 2007-08-13 11:00:49Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#include "net.h"

static struct linger linger = { 1, 5 }; /* 5 second linger */

static int one = 1;

/* Create a socket */

static int OpenSocket(void)
{
   int sd;    /* socket descriptor */

   sd = socket (AF_INET, SOCK_STREAM, 0);
   if (sd == -1) {
      perror("unable to create socket");
      return(-1);
   }

   if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
      perror("Couldn't setsockopt(REUSEADDR)");
      return(-1);
   }

   if (setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) != 0) {
      perror("Couldn't setsockopt(LINGER)");
      return(-1);
   }

   return(sd);
}

/* Bind a socket to a port (using a port number) */

static int BindSocketToPort(int socket, int port)
{
   struct sockaddr_in myaddr_in;    /* for local socket address       */

   memset(&myaddr_in, 0, sizeof(myaddr_in));

   myaddr_in.sin_addr.s_addr = INADDR_ANY;
   myaddr_in.sin_port = htons(port);
   myaddr_in.sin_family = AF_INET;

   if (bind(socket, (struct sockaddr *) &myaddr_in,
           sizeof(struct sockaddr_in)) != 0) {
      perror("BindSocketToPort: bind failed");
      return(-1);
   }

   return(0);
}

/* Tell a socket to be a listener */

static int ListenOnSocket(int socket)
{
   if (listen(socket, 5) == -1) {
      perror("SetListenSocket: listen failed");
      return(-1);
   }

   return(0);
}

/* Open a listen port (using a port number) */

int netOpenPort(int port)
{
   int lsd;

   lsd = OpenSocket();
   if (lsd == -1) {
      fprintf(stderr, "OpenPort: OpenSocket failed\n");
      return(-1);
   }

   if (BindSocketToPort(lsd, port) != 0) {
      fprintf(stderr, "OpenPort: BindSocketToPort failed\n");
      return(-1);
   }

   if (ListenOnSocket(lsd) != 0) {
      perror("OpenPort: ListenOnSocket failed");
      return(-1);
   }

   return(lsd);
}

/* Connect to a port (using a port number) */

int netConnect(int port, char *server)
{
   struct hostent *host_ptr;        /* pointer to host info for remote host */
   struct sockaddr_in peeraddr_in;  /* for peer socket address              */

   int sd;                          /* socket descriptor */

   memset(&peeraddr_in, 0, sizeof(peeraddr_in));

   peeraddr_in.sin_family = AF_INET;
   peeraddr_in.sin_port = htons(port);

   host_ptr = gethostbyname (server);
   if (host_ptr == NULL) {
      fprintf(stderr, "ConnectPort: gethostbyname(%s) failed\n", server);
      return(-1);
   }
   peeraddr_in.sin_addr.s_addr = ((struct in_addr *)(host_ptr->h_addr))->s_addr;

   sd = OpenSocket();

   if (sd == -1) {
      perror("ConnectPort: OpenSocket failed");
      return(-1);
   }

   if (connect(sd, (struct sockaddr *) &peeraddr_in,
              sizeof(struct sockaddr_in)) != 0) {
      perror("ConnectPort: connect failed");
      close(sd);
      return(-1);
   }

   return(sd);
}

/* Get the port that corresponds to service <service>. */

int netPortFor(char *service)
{
   struct servent *serv_ptr;        /* pointer to service information */

   serv_ptr = getservbyname (service, "tcp");

   if (serv_ptr == NULL) {
      fprintf(stderr, "service \"%s\" not found in /etc/services\n", service);
      return(-1);
   }

   return serv_ptr->s_port;
}

/* Accept an incoming connection request on a listen socket */

int netAccept(int sd)
{
   struct sockaddr_in peeraddr_in;  /* for peer socket address        */
   socklen_t addrlen;
   int csd;

   memset(&peeraddr_in, 0, sizeof(struct sockaddr_in));

   addrlen = sizeof(struct sockaddr_in);

   do {
      csd = accept(sd, (struct sockaddr *) &peeraddr_in, &addrlen);
   } while (csd == -1 && errno == EINTR);

   if ( csd == -1) perror("AcceptClient: accept failed");

   return(csd);
}

/* Get hostname of peer */

char *netGetPeerName(int sd)
{
   struct sockaddr_in peeraddr;
   struct hostent *ent;
   socklen_t len = sizeof(struct sockaddr_in);

   getpeername(sd, (struct sockaddr *) &peeraddr, &len);

   ent = gethostbyaddr((char *) &peeraddr.sin_addr, sizeof(peeraddr.sin_addr), AF_INET);

   if (ent == NULL) return(NULL);

   return(ent->h_name);
}

/* Read until <buf> is full */

int netRead(int fd, void *buf, int len)
{
   int res, n = 0;

   do {
      res = read(fd, (char *) buf + n, len - n);
   } while ((res > 0 || errno == EINTR) && (n += res) < len);

   if (res == -1) {
      perror("read(1) failed");
      return(-1);
   }
   else {
      return(n);
   }
}

/* Write all of <buf> */

int netWrite(int fd, void *buf, int len)
{
   int res, n = 0;

   do {
      res = write(fd, (char *) buf + n, len - n);
   } while ((res > 0 || errno == EINTR) && (n += res) < len);

   if (res == -1) {
      perror("write(1) failed");
      return(-1);
   }
   else {
      return(n);
   }
}
