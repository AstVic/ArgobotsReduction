# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

TESTS = \
	reduction

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/test/Makefile.mk

reduction_SOURCES = reduction.c

testing:
	./reduction
