#ifndef SDP_H
#define SDP_H

/*
 * sdp.h: Simple Data Parser.
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-07-03
 * Version:	$Id$
 */

#include <stdio.h>

#include "list.h"

typedef enum {
    SDP_STRING,
    SDP_LONG,
    SDP_DOUBLE,
    SDP_CONTAINER
} SdpType;

typedef struct SdpObject SdpObject;

struct SdpObject {
    ListNode _node;
    SdpType type;
    int     line;
    union {
        char *s;
        long l;
        double d;
        List c;
    } u;
};

/*
 * Read SdpObjects from file <fp> and append them to <objects>.
 */
int sdpReadFile(FILE *fp, List *objects);

/*
 * Read SdpObjects from file descriptor <fd> and append them to <objects>.
 */
int sdpReadFD(int fd, List *objects);

/*
 * Read SdpObjects from string <str> and append them to <objects>.
 */
int sdpReadString(const char *str, List *objects);

/*
 * Dump the list of SdpObjects in <objects> on <fp>, indented by <indent> levels.
 */
void sdpDump(FILE *fp, int indent, List *objects);

/*
 * Get the error that occurred as indicated by one of the sdpRead... functions returning -1. The
 * error message that is returned is dynamically allocated and should be free'd by the caller.
 */
char *sdpError(void);

/*
 * Clear the list of SdpObjects at <objects>. Clears the contents of the list but leaves the list
 * itself alone.
 */
void sdpClear(List *objects);

/*
 * Clear and free the list of SdpObjects at <objects>.
 */
void sdpFree(List *objects);

#endif