# Makefile for libjvs.
#
# Copyright:    (c) 2007-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
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

LIBJVS_SRC = $(wildcard *.c) vectors.c
LIBJVS_OBJ = $(LIBJVS_SRC:.c=.o)
LIBJVS_DEP = $(LIBJVS_SRC:.c=.d)
LIBJVS_TST = $(subst .c,.test,$(shell grep -l '^\#ifdef TEST' $(LIBJVS_SRC)))

OPT_FLAGS = -O0
DEP_FLAGS = -MMD
PRO_FLAGS = # -pg

CFLAGS = -std=gnu11 -D_GNU_SOURCE -g -fPIC -Wall -Wextra -Werror -pedantic \
         $(OPT_FLAGS) $(PRO_FLAGS) $(DEP_FLAGS) # -DPARANOID

all: libjvs.a libjvs.so # tags

latlon_fields.c: latlon_fields.txt
	./gen_enum -p "" -n LLF_COUNT -s latlon_string -e latlon_enum -c $< > $@

latlon_fields.h: latlon_fields.txt
	./gen_enum -p "" -n LLF_COUNT -s latlon_string -e latlon_enum -h $< > $@

latlon_fields.o: latlon_fields.c latlon_fields.h

latlon.o: latlon_fields.h

vectors.o: vectors.c vectors.h

vectors.c: make_vectors.sh
	./make_vectors.sh -c > $@

vectors.h: make_vectors.sh
	./make_vectors.sh -h > $@

libjvs.a: $(LIBJVS_OBJ) latlon_fields.o
	$(MAKE_ALIB) libjvs.a $^

libjvs.so: $(LIBJVS_OBJ) latlon_fields.o
	$(MAKE_SLIB) libjvs.so $^ -lm

clean:
	rm -f *.o *.d *.test *.log \
            libjvs.a libjvs.so \
            libjvs.tgz \
            core vgcore.* \
            latlon_fields.c latlon_fields.h \
            vectors.c vectors.h \
            tags

libjvs.tgz: clean
	tar cvf - `ls | grep -v libjvs.tgz` | gzip > libjvs.tgz

install: test libjvs.a libjvs.so
	if [ ! -d $(INSTALL_LIB) ]; then mkdir -p $(INSTALL_LIB); fi
	cp libjvs.a libjvs.so $(INSTALL_LIB)
	if [ ! -d $(INSTALL_INC)/libjvs ]; then mkdir -p $(INSTALL_INC); fi
	rm -rf $(INSTALL_INC)/*
	cp *.h $(INSTALL_INC)

update:
	git pull && make install

tags: $(wildcard *.[ch])
	@echo "Generating tags"
	@ctags -R --c-kinds=+p . /usr/include

vectors: vectors.c vectors.h
	$(CC) $(CFLAGS) -o $@ $^ -lm

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
