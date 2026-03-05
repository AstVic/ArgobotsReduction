SHELL=/bin/sh
BENCHMARK=cg
BENCHMARKU=CG

include ../config/make.def

include ../sys/make.common

OBJS = cg.o \
       abt_reduction.o \
       ws_old.o \
       ws_new.o \
       ${COMMON}/print_results.o  \
       ${COMMON}/${RAND}.o \
       ${COMMON}/c_timers.o \
       ${COMMON}/wtime.o


${PROGRAM}: config ${OBJS}
	${CLINK} ${CLINKFLAGS} -o ${PROGRAM} ${OBJS} ${C_LIB}

.c.o:
	${CCOMPILE} $<

cg.o:		cg.c globals.h npbparams.h abt_reduction.h

abt_reduction.o: abt_reduction.c abt_reduction.h

clean:
	- rm -f *.o *~ 
	- rm -f npbparams.h core
	- if [ -d rii_files ]; then rm -r rii_files; fi

ws_old.o: ../../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler.c ../../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler.h
	${CCOMPILE} ../../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler.c -o ws_old.o

ws_new.o: ../../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler_cost_aware.c ../../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler_cost_aware.h
	${CCOMPILE} ../../argobots_framework/examples/workstealing_scheduler/abt_workstealing_scheduler_cost_aware.c -o ws_new.o