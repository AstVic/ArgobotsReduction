# -*- Mode: Makefile; -*-
#
# See COPYRIGHT in top-level directory.
#

# Список тестов
TESTS = \
	workstealing_scheduler \
	workstealing_scheduler_cost_aware \
	workstealing_scheduler_compare

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

include $(top_srcdir)/examples/Makefile.mk

# Исходники для старого планировщика
workstealing_scheduler_SOURCES = \
	workstealing_scheduler.c \
	abt_workstealing_scheduler.c

# Исходники для cost-aware планировщика
workstealing_scheduler_cost_aware_SOURCES = \
	workstealing_scheduler.c \
	abt_workstealing_scheduler_cost_aware.c

# Исходники для теста сравнения
workstealing_scheduler_compare_SOURCES = \
	workstealing_scheduler_compare.c \
	abt_workstealing_scheduler.c \
	abt_workstealing_scheduler_cost_aware.c
