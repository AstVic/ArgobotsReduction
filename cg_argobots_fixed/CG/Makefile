SHELL=/bin/sh
BENCHMARK=cg
BENCHMARKU=CG

include ../config/make.def

include ../sys/make.common

OBJS = cg.o \
       abt_reduction.o \
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
