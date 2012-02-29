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

/* Subscribe to input. */
void cxAddFile(CX *cx, int fd, void *udata, int (*handler)(CX *cx, int fd, void
                 *udata));

/* Drop subscription to fd <fd>. */
void cxDropFile(CX *cx, int fd, int (*handler)(CX *cx, int fd, void *udata));

/* Add a timeout at time <t>. */
void cxAddTime(CX *cx, double t, void *udata, int (*handler)(CX *cx, double t,
                 void *udata));

/* Drop timeout at time <t>. */
void cxDropTime(CX *cx, double t, int (*handler)(CX *cx, double t, void *udata));

/* Run the communications exchange. */
int cxRun(CX *cx);

#endif
