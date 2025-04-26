SHELL=/bin/sh
BENCHMARK=bt-mz
BENCHMARKU=BT-MZ
VEC=

include ../config/make.def


OBJS = bt.o  initialize.o exact_solution.o exact_rhs.o \
       set_constants.o adi.o  rhs.o zone_setup.o  \
       x_solve$(VEC).o y_solve$(VEC).o  exch_qbc.o solve_subs.o \
       z_solve$(VEC).o add.o error.o verify.o omp_setup.o \
       ${COMMON}/c_print_results.o ${COMMON}/c_timers.o ${COMMON}/wtime.o

include ../sys/make.common

# npbparams.h is included by header.h
# The following rule should do the trick but many make programs (not gmake)
# will do the wrong thing and rebuild the world every time (because the
# mod time on header.h is not changed. One solution would be to 
# touch header.h but this might cause confusion if someone has
# accidentally deleted it. Instead, make the dependency on npbparams.h
# explicit in all the lines below (even though dependence is indirect). 

# header.h: npbparams.h

${PROGRAM}: config
	@if [ x$(VERSION) = xvec ] ; then	\
		${MAKE} VEC=_vec exec;		\
	elif [ x$(VERSION) = xVEC ] ; then	\
		${MAKE} VEC=_vec exec;		\
	else					\
		${MAKE} exec;			\
	fi

exec: $(OBJS)
	${CLINK} ${CLINKFLAGS} -o ${PROGRAM} ${OBJS} ${F_LIB} -lm

.c.o:
	${CCOMPILE} $<

bt.o:             bt.c  omp_stuff.h header.h npbparams.h commons.h
initialize.o:     initialize.c  header.h npbparams.h commons.h
exact_solution.o: exact_solution.c  header.h npbparams.h commons.h
exact_rhs.o:      exact_rhs.c  header.h npbparams.h commons.h
set_constants.o:  set_constants.c  header.h npbparams.h commons.h
adi.o:            adi.c  header.h npbparams.h commons.h
rhs.o:            rhs.c  header.h npbparams.h commons.h
zone_setup.o:     zone_setup.c omp_stuff.h header.h npbparams.h commons.h
x_solve$(VEC).o:  x_solve$(VEC).c  header.h work_lhs$(VEC).h npbparams.h commons.h
y_solve$(VEC).o:  y_solve$(VEC).c  header.h work_lhs$(VEC).h npbparams.h commons.h
z_solve$(VEC).o:  z_solve$(VEC).c  header.h work_lhs$(VEC).h npbparams.h commons.h
solve_subs.o:     solve_subs.c  npbparams.h commons.h
add.o:            add.c  header.h npbparams.h commons.h
error.o:          error.c  header.h npbparams.h commons.h
verify.o:         verify.c  omp_stuff.h header.h npbparams.h commons.h
exch_qbc.o:       exch_qbc.c omp_stuff.h header.h npbparams.h commons.h
omp_setup.o:      omp_setup.c  omp_stuff.h header.h npbparams.h commons.h

clean:
	- rm -f *.o *~ mputil*
	- rm -f npbparams.h core
