#ifndef DX_H
#define DX_H

/*
 * dx.h: Description
 *
 * Copyright:	(c) 2012 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id$
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */


typedef struct CX CX;

/* Create a communications exchange. */
CX *cxCreate(void);

/* Add file descriptor <fd>. */
void cxAddFd(CX *cx, int fd);

/* Add a timeout at time <t>. */
void cxAddTimeout(CX *cx, double t, void *udata, int (*handler)(CX *cx, double t,
                    void *udata));

/* Run the communications exchange. */
int cxRun(CX *cx);


#endif
