# Makefile for libjvs.
#
# Copyright:    (c) 2007-2024 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:      $Id$
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

.PHONY: test

MAKE_ALIB = ar rv
MAKE_SLIB = gcc -shared -o

INSTALL_LIB = $(HOME)/lib
INSTALL_INC = $(HOME)/include/libjvs

CC = gcc

LIBJVS_SRC = $(wildcard *.c)
LIBJVS_OBJ = $(LIBJVS_SRC:.c=.o)
LIBJVS_DEP = $(LIBJVS_SRC:.c=.d)
LIBJVS_TST = $(subst .c,.test,$(shell grep -l '^\#ifdef TEST' $(LIBJVS_SRC)))

OPT_FLAGS = -O3
DEP_FLAGS = -MMD
PRO_FLAGS = # -pg

CFLAGS = -std=gnu99 -D_GNU_SOURCE -g -fPIC -Wall -Wextra -Werror -pedantic \
         $(OPT_FLAGS) $(PRO_FLAGS) $(DEP_FLAGS) # -DPARANOID

all: libjvs.a libjvs.so # tags

libjvs.a: $(LIBJVS_OBJ)
	$(MAKE_ALIB) libjvs.a $(LIBJVS_OBJ)

libjvs.so: $(LIBJVS_OBJ)
	$(MAKE_SLIB) libjvs.so $(LIBJVS_OBJ) -lm

clean:
	rm -f *.o *.d *.test *.log \
            libjvs.a libjvs.so \
            libjvs.tgz \
            core vgcore.* \
            tags

libjvs.tgz: clean
	tar cvf - `ls | grep -v libjvs.tgz` | gzip > libjvs.tgz

install: test libjvs.a libjvs.so
	if [ ! -d $(INSTALL_LIB) ]; then mkdir -p $(INSTALL_LIB); fi
	cp libjvs.a libjvs.so $(INSTALL_LIB)
	if [ ! -d $(INSTALL_INC)/libjvs ]; then mkdir -p $(INSTALL_INC); fi
	rm -rf $(INSTALL_INC)/*
	cp *.h $(INSTALL_INC)

tags: $(wildcard *.[ch])
	@echo "Generating tags"
	@ctags -R --c-kinds=+p . /usr/include

%.test: %.c %.h libjvs.a
	@echo "Building tester for $*"
	@$(CC) $(CFLAGS) -DTEST -o $@ $< libjvs.a -lm

test: $(LIBJVS_TST)
	@echo "Running tests..."
	@./run_tests.sh $(LIBJVS_TST)
	@echo "All tests succeeded."

commit:
	@echo "\033[7mSubversion status:\033[0m"
	@svn status
	@echo "\033[7mGit status:\033[0m"
	@git status
	@echo -n 'Message: '
	@read msg \
           && svn commit -m "$$msg" && svn update \
           && git commit -a -m "$$msg" && git push

include $(wildcard $(LIBJVS_DEP))
