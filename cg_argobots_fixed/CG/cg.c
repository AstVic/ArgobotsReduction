//-------------------------------------------------------------------------//
//                                                                         //
//  This benchmark is an OpenMP C version of the NPB CG code. This OpenMP  //
//  C version is developed by the Center for Manycore Programming at Seoul //
//  National University and derived from the OpenMP Fortran versions in    //
//  "NPB3.3-OMP" developed by NAS.                                         //
//                                                                         //
//  Permission to use, copy, distribute and modify this software for any   //
//  purpose with or without fee is hereby granted. This software is        //
//  provided "as is" without express or implied warranty.                  //
//                                                                         //
//  Information on NPB 3.3, including the technical report, the original   //
//  specifications, source code, results and information on how to submit  //
//  new results, is available at:                                          //
//                                                                         //
//           http://www.nas.nasa.gov/Software/NPB/                         //
//                                                                         //
//  Send comments or suggestions for this OpenMP C version to              //
//  cmp@aces.snu.ac.kr                                                     //
//                                                                         //
//          Center for Manycore Programming                                //
//          School of Computer Science and Engineering                     //
//          Seoul National University                                      //
//          Seoul 151-744, Korea                                           //
//                                                                         //
//          E-mail:  cmp@aces.snu.ac.kr                                    //
//                                                                         //
//-------------------------------------------------------------------------//

//-------------------------------------------------------------------------//
// Authors: Sangmin Seo, Jungwon Kim, Jun Lee, Jeongho Nah, Gangwon Jo,    //
//          and Jaejin Lee                                                 //
//-------------------------------------------------------------------------//

//---------------------------------------------------------------------
// program cg
//---------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "globals.h"
#include "randdp.h"
#include "timers.h"
#include "print_results.h"
#include "abt_reduction.h"


//---------------------------------------------------------------------
/* common / main_int_mem / */
static int colidx[NZ];
static int rowstr[NA+1];
static int iv[NZ+1+NA];
static int arow[NA+1];
static int acol[NAZ];

/* common / main_flt_mem / */
static double v[NZ];
static double aelt[NAZ];
static double a[NZ];
static double x[NA+2];
static double z[NA+2];
static double p[NA+2];
static double q[NA+2];
static double r[NA+2];

/* common /tinof/ */

#define max_threads 1024
static int last_n[max_threads+1];

/* common / partit_size / */
static int naa;
static int nzz;
static int firstrow;
static int lastrow;
static int firstcol;
static int lastcol;

/* common /urando/ */
const static double amult = 1220703125.0;

/* common /timers/ */
static logical timeron;

// Argobots section
#define DEFAULT_XSTREAMS 4
#define DEFAULT_THREADS 4
static reduction_context_t reduction_context;
static ABT_barrier barrier;

//---------------------------------------------------------------------


//---------------------------------------------------------------------
static void conj_grad(int colidx[],
                      int rowstr[],
                      double x[],
                      double z[],
                      double a[],
                      double p[],
                      double q[],
                      double r[],
                      double *rnorm);
static void makea(int thread_id,
                  double *tran_ptr,
                  int n,
                  int nz,
                  double a[],
                  int colidx[],
                  int rowstr[],
                  int firstrow,
                  int lastrow,
                  int firstcol,
                  int lastcol,
                  int arow[],
                  int acol[][NONZER+1],
                  double aelt[][NONZER+1],
                  double v[],
                  int iv[]);
static void sparse(int thread_id,
                   int ilow,
                   int ihigh,
                   double a[],
                   int colidx[],
                   int rowstr[],
                   int n,
                   int nz,
                   int nozer,
                   int arow[],
                   int acol[][NONZER+1],
                   double aelt[][NONZER+1],
                   int firstrow,
                   int lastrow,
                   int last_n[],
                   double v[],
                   int iv[],
                   int nzloc[],
                   double rcond,
                   double shift);
static void sprnvc(double *tran_ptr, int n, int nz, int nn1, double v[], int iv[]);
static int icnvrt(double x, int ipwr2);
static void vecset(int n, double v[], int iv[], int *nzv, int i, double val);
//---------------------------------------------------------------------


//---------------------------------------------------------------------
// Argobots functions
void initialize_argobots(int num_xstreams, int num_threads) {
    /* Initialize Argobots. */
    ABT_init(0, NULL);
    
    reduction_context.num_xstreams = num_xstreams;
    reduction_context.xstreams = (ABT_xstream *)calloc(num_xstreams, sizeof(ABT_xstream));
    
    int num_pools = num_xstreams;
    reduction_context.num_pools = num_pools;
    reduction_context.pools = (ABT_pool *)calloc(num_pools, sizeof(ABT_pool));
    
    reduction_context.num_threads = num_threads;
    reduction_context.threads = (ABT_thread *)calloc(num_threads, sizeof(ABT_thread));
    
    /* Get a primary execution stream. */
    ABT_xstream_self(&(reduction_context.xstreams[0]));

    /* Create secondary execution streams. */
    for (int i = 1; i < num_xstreams; i++) {
        ABT_xstream_create(ABT_SCHED_NULL, &(reduction_context.xstreams[i]));
    }

    /* Get default pools. */
    for (int i = 0; i < num_xstreams; i++) {
        ABT_xstream_get_main_pools(reduction_context.xstreams[i], 1, &(reduction_context.pools[i]));
    }

    /* Create a barrier for the threads. */
    ABT_barrier_create(num_threads, &barrier);
}

void finalize_argobots() {
    /* Free ULTs. */
    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_free(&reduction_context.threads[i]);
    }    
    
    /* Join and free secondary execution streams. */
    for (int i = 1; i < reduction_context.num_xstreams; i++) {
        ABT_xstream_join(reduction_context.xstreams[i]);
        ABT_xstream_free(&reduction_context.xstreams[i]);
    }
    
    /* Free the barrier */
    ABT_barrier_free(&barrier);
    
    /* Finalize Argobots. */
    ABT_finalize();

     /* Free allocated memory. */
    free(reduction_context.xstreams);
    free(reduction_context.pools);
    free(reduction_context.threads);
}

//---------------------------------------------------------------------
// Functions converted from OpenMP sections to Argobots

typedef struct {
  int thread_id;
} init_random_number_generator_thread_args_t;

void init_random_number_generator_thread(void *args) {
  init_random_number_generator_thread_args_t *args_ptr = (init_random_number_generator_thread_args_t *)args;

  double tran      = 314159265.0;
  double* tran_ptr = &tran;
  randlc(tran_ptr, amult);

  //---------------------------------------------------------------------
  //  
  //---------------------------------------------------------------------
  int thread_id = args_ptr->thread_id;
  makea(thread_id, tran_ptr, naa, nzz, a, colidx, rowstr, 
        firstrow, lastrow, firstcol, lastcol, 
        arow, 
        (int (*)[NONZER+1])(void*)acol, 
        (double (*)[NONZER+1])(void*)aelt,
        v, iv);

  ABT_barrier_wait(barrier);

  //---------------------------------------------------------------------
  // Note: as a result of the above call to makea:
  //    values of j used in indexing rowstr go from 0 --> lastrow-firstrow
  //    values of colidx which are col indexes go from firstcol --> lastcol
  //    So:
  //    Shift the col index vals from actual (firstcol --> lastcol ) 
  //    to local, i.e., (0 --> lastcol-firstcol)
  //---------------------------------------------------------------------
  int i, j, k;
  int j_stop_original = lastrow - firstrow + 1;
  int nrows_per_thread = j_stop_original / reduction_context.num_threads;
  int j_start = thread_id * nrows_per_thread;
  int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + nrows_per_thread;
  for (j = j_start; j < j_stop; j++) {
    for (k = rowstr[j]; k < rowstr[j+1]; k++) {
      colidx[k] = colidx[k] - firstcol;
    }
  }

  //---------------------------------------------------------------------
  // set starting vector to (1, 1, .... 1)
  //---------------------------------------------------------------------
  int i_stop_original = NA+1;
  int i_per_thread = i_stop_original / reduction_context.num_threads;
  int i_start = thread_id * i_per_thread;
  int i_stop = (thread_id == reduction_context.num_threads - 1) ? i_stop_original : i_start + i_per_thread;
  for (i = i_start; i < i_stop; i++) {
    x[i] = 1.0;
  }

  int j2_stop_original = lastcol - firstcol + 1;
  int ncols_per_thread = j2_stop_original / reduction_context.num_threads;
  int j2_start = thread_id * ncols_per_thread;
  int j2_stop = (thread_id == reduction_context.num_threads - 1) ? j2_stop_original : j2_start + ncols_per_thread;
  for (j = j2_start; j < j2_stop; j++) {
    q[j] = 0.0;
    z[j] = 0.0;
    r[j] = 0.0;
    p[j] = 0.0;
  }
}

void init_random_number_generator() {
  init_random_number_generator_thread_args_t* args = (init_random_number_generator_thread_args_t*)malloc(sizeof(init_random_number_generator_thread_args_t) * reduction_context.num_threads);
  for (int i = 0; i < reduction_context.num_threads; i++) {
    args[i].thread_id = i;
    ABT_thread_create(
        reduction_context.pools[i % reduction_context.num_pools],
        init_random_number_generator_thread,
        &args[i],
        ABT_THREAD_ATTR_NULL,
        &reduction_context.threads[i]
    );
  }

  for (int i = 0; i < reduction_context.num_threads; i++) {
    ABT_thread_join(reduction_context.threads[i]);
    ABT_thread_free(&reduction_context.threads[i]);
  }
  free(args);
}


typedef struct {
  int thread_id;
} set_starting_vector_to_ones_thread_args_t;

void set_starting_vector_to_ones_thread(void *args) {
  set_starting_vector_to_ones_thread_args_t *args_ptr = (set_starting_vector_to_ones_thread_args_t *)args;

  int thread_id = args_ptr->thread_id;
  int i_stop_original = NA + 1;
  int i_per_thread = i_stop_original / reduction_context.num_threads;
  int i_start = thread_id * i_per_thread;
  int i_stop = (thread_id == reduction_context.num_threads - 1) ? i_stop_original : i_start + i_per_thread;
  for (int i = i_start; i < i_stop; i++) {
    x[i] = 1.0;
  }
}

void set_starting_vector_to_ones() {
  set_starting_vector_to_ones_thread_args_t* args = (set_starting_vector_to_ones_thread_args_t*)malloc(sizeof(set_starting_vector_to_ones_thread_args_t) * reduction_context.num_threads);
  for (int i = 0; i < reduction_context.num_threads; i++) {
    args[i].thread_id = i;
    ABT_thread_create(
        reduction_context.pools[i % reduction_context.num_pools],
        set_starting_vector_to_ones_thread,
        &args[i],
        ABT_THREAD_ATTR_NULL,
        &reduction_context.threads[i]
    );
  }

  for (int i = 0; i < reduction_context.num_threads; i++) {
    ABT_thread_join(reduction_context.threads[i]);
    ABT_thread_free(&reduction_context.threads[i]);
  }
  free(args);
}


typedef struct {
    int thread_id;
    double* norm_temp1_local;
    double* norm_temp2_local;
} calculate_norm_temps_thread_args_t;

void calculate_norm_temps_thread(void *args) {
    calculate_norm_temps_thread_args_t *args_ptr = (calculate_norm_temps_thread_args_t *)args;

    int thread_id = args_ptr->thread_id;
    double norm_temp1 = 0.0;
    double norm_temp2 = 0.0;

    int j_stop_original = lastcol - firstcol + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    for (int j = j_start; j < j_stop; j++) {
        norm_temp1 += x[j] * z[j];
        norm_temp2 += z[j] * z[j];
    }

    *(args_ptr->norm_temp1_local) = norm_temp1;
    *(args_ptr->norm_temp2_local) = norm_temp2;
}

typedef struct {
  double norm_temp1;
  double norm_temp2;
} norm_temps_result;

norm_temps_result calculate_norm_temps() {
    double* norm_temp1_values = (double*)malloc(sizeof(double) * reduction_context.num_threads);
    double* norm_temp2_values = (double*)malloc(sizeof(double) * reduction_context.num_threads);
    calculate_norm_temps_thread_args_t* args = (calculate_norm_temps_thread_args_t*)malloc(sizeof(calculate_norm_temps_thread_args_t) * reduction_context.num_threads);
    for (int i = 0; i < reduction_context.num_threads; i++) {
        args[i].thread_id = i;
        args[i].norm_temp1_local = &norm_temp1_values[i];
        args[i].norm_temp2_local = &norm_temp2_values[i];
        ABT_thread_create(
            reduction_context.pools[i % reduction_context.num_pools],
            calculate_norm_temps_thread,
            &args[i],
            ABT_THREAD_ATTR_NULL,
            &reduction_context.threads[i]
        );
    }

    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_join(reduction_context.threads[i]);
        ABT_thread_free(&reduction_context.threads[i]);
    }

    double norm_temp1 = 0.0;
    double norm_temp2 = 0.0;
    reduce_sum_double(&reduction_context, norm_temp1_values, reduction_context.num_threads, &norm_temp1);
    reduce_sum_double(&reduction_context, norm_temp2_values, reduction_context.num_threads, &norm_temp2);

    norm_temps_result result;
    result.norm_temp1 = norm_temp1;
    result.norm_temp2 = norm_temp2;

    free(norm_temp1_values);
    free(norm_temp2_values);
    free(args);
    return result;
}


typedef struct {
    int thread_id;
    double norm_temp2;
} normalize_z_thread_args_t;

void normalize_z_thread(void *args) {
    normalize_z_thread_args_t *args_ptr = (normalize_z_thread_args_t *)args;

    int thread_id = args_ptr->thread_id;
    double norm_temp2 = args_ptr->norm_temp2;

    int j_stop_original = lastcol - firstcol + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    for (int j = j_start; j < j_stop; j++) {
      x[j] = norm_temp2 * z[j];
    }
}

void normalize_z(double norm_temp2) {
    normalize_z_thread_args_t* args = (normalize_z_thread_args_t*)malloc(sizeof(normalize_z_thread_args_t) * reduction_context.num_threads);
    for (int i = 0; i < reduction_context.num_threads; i++) {
        args[i].thread_id = i;
        args[i].norm_temp2 = norm_temp2;
        ABT_thread_create(
            reduction_context.pools[i % reduction_context.num_pools],
            normalize_z_thread,
            &args[i],
            ABT_THREAD_ATTR_NULL,
            &reduction_context.threads[i]
        );
    }

    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_join(reduction_context.threads[i]);
        ABT_thread_free(&reduction_context.threads[i]);
    }

    free(args);
}


//---------------------------------------------------------------------


int main(int argc, char *argv[])
{
  int num_xstreams = DEFAULT_XSTREAMS;
  int num_threads = DEFAULT_THREADS;
  if (argc > 1) {
    num_xstreams = atoi(argv[1]);
    if (num_xstreams < 1) {
        num_xstreams = DEFAULT_XSTREAMS;
    }
  }
  if (argc > 2) {
    num_threads = atoi(argv[2]);
    if (num_threads < 1) {
        num_threads = DEFAULT_THREADS;
    }
  }
  initialize_argobots(num_xstreams, num_threads);

  int i, it;

  double zeta;
  double rnorm;
  double norm_temp2;

  double t, mflops, tmax;
  char Class;
  logical verified;
  double zeta_verify_value, epsilon, err;

  char *t_names[T_last];

  for (i = 0; i < T_last; i++) {
    timer_clear(i);
  }

  FILE *fp;
  if ((fp = fopen("timer.flag", "r")) != NULL) {
    timeron = true;
    t_names[T_init] = "init";
    t_names[T_bench] = "benchmk";
    t_names[T_conj_grad] = "conjgd";
    fclose(fp);
  } else {
    timeron = false;
  }

  timer_start(T_init);

  firstrow = 0;
  lastrow  = NA-1;
  firstcol = 0;
  lastcol  = NA-1;

  if (NA == 1400 && NONZER == 7 && NITER == 15 && SHIFT == 10) {
    Class = 'S';
    zeta_verify_value = 8.5971775078648;
  } else if (NA == 7000 && NONZER == 8 && NITER == 15 && SHIFT == 12) {
    Class = 'W';
    zeta_verify_value = 10.362595087124;
  } else if (NA == 14000 && NONZER == 11 && NITER == 15 && SHIFT == 20) {
    Class = 'A';
    zeta_verify_value = 17.130235054029;
  } else if (NA == 75000 && NONZER == 13 && NITER == 75 && SHIFT == 60) {
    Class = 'B';
    zeta_verify_value = 22.712745482631;
  } else if (NA == 150000 && NONZER == 15 && NITER == 75 && SHIFT == 110) {
    Class = 'C';
    zeta_verify_value = 28.973605592845;
  } else if (NA == 1500000 && NONZER == 21 && NITER == 100 && SHIFT == 500) {
    Class = 'D';
    zeta_verify_value = 52.514532105794;
  } else if (NA == 9000000 && NONZER == 26 && NITER == 100 && SHIFT == 1500) {
    Class = 'E';
    zeta_verify_value = 77.522164599383;
  } else {
    Class = 'U';
  }

  printf("\n\n NAS Parallel Benchmarks (NPB3.3-OMP-C) - CG Benchmark\n\n");
  printf(" Size: %11d\n", NA);
  printf(" Iterations:                  %5d\n", NITER);
  printf("\n");

  naa = NA;
  nzz = NZ;

  //---------------------------------------------------------------------
  // Inialize random number generator
  //---------------------------------------------------------------------
  init_random_number_generator();

  //---------------------------------------------------------------------
  //---->
  // Do one iteration untimed to init all code and data page tables
  //---->                    (then reinit, start timing, to niter its)
  //---------------------------------------------------------------------
  for (it = 1; it <= 1; it++) {
    //---------------------------------------------------------------------
    // The call to the conjugate gradient routine:
    //---------------------------------------------------------------------
    conj_grad(colidx, rowstr, x, z, a, p, q, r, &rnorm);

    //---------------------------------------------------------------------
    // zeta = shift + 1/(x.z)
    // So, first: (x.z)
    // Also, find norm of z
    // So, first: (z.z)
    //---------------------------------------------------------------------
    norm_temps_result norm_temps = calculate_norm_temps();
    norm_temp2 = 1.0 / sqrt(norm_temps.norm_temp2);

    //---------------------------------------------------------------------
    // Normalize z to obtain x
    //---------------------------------------------------------------------
    normalize_z(norm_temp2);
  } // end of do one iteration untimed


  //---------------------------------------------------------------------
  // set starting vector to (1, 1, .... 1)
  //---------------------------------------------------------------------
  set_starting_vector_to_ones();

  zeta = 0.0;

  timer_stop(T_init);

  printf(" Initialization time = %15.3f seconds\n", timer_read(T_init));

  timer_start(T_bench);
  clock_t start = clock();

  //---------------------------------------------------------------------
  //---->
  // Main Iteration for inverse power method
  //---->
  //---------------------------------------------------------------------
  for (it = 1; it <= NITER; it++) {
    //---------------------------------------------------------------------
    // The call to the conjugate gradient routine:
    //---------------------------------------------------------------------
    if (timeron) timer_start(T_conj_grad);
    conj_grad(colidx, rowstr, x, z, a, p, q, r, &rnorm);
    if (timeron) timer_stop(T_conj_grad);

    //---------------------------------------------------------------------
    // zeta = shift + 1/(x.z)
    // So, first: (x.z)
    // Also, find norm of z
    // So, first: (z.z)
    //---------------------------------------------------------------------
    norm_temps_result norm_temps = calculate_norm_temps();
    norm_temp2 = 1.0 / sqrt(norm_temps.norm_temp2);

    zeta = SHIFT + 1.0 / norm_temps.norm_temp1;
    if (it == 1) 
      printf("\n   iteration           ||r||                 zeta\n");
    printf("    %5d       %20.14E%20.13f\n", it, rnorm, zeta);

    //---------------------------------------------------------------------
    // Normalize z to obtain x
    //---------------------------------------------------------------------
    normalize_z(norm_temp2);
  } // end of main iter inv pow meth

  timer_stop(T_bench);
  clock_t end = clock();
  double processor_time = ((double)(end - start)) / CLOCKS_PER_SEC;

  //---------------------------------------------------------------------
  // End of timed section
  //---------------------------------------------------------------------

  t = timer_read(T_bench);

  printf(" Benchmark completed\n");
  finalize_argobots();

  epsilon = 1.0e-10;
  if (Class != 'U') {
    err = fabs(zeta - zeta_verify_value) / zeta_verify_value;
    if (err <= epsilon) {
      verified = true;
      printf(" VERIFICATION SUCCESSFUL\n");
      printf(" Zeta is    %20.13E\n", zeta);
      printf(" Error is   %20.13E\n", err);
    } else {
      verified = false;
      printf(" VERIFICATION FAILED\n");
      printf(" Zeta                %20.13E\n", zeta);
      printf(" The correct zeta is %20.13E\n", zeta_verify_value);
    }
  } else {
    verified = false;
    printf(" Problem size unknown\n");
    printf(" NO VERIFICATION PERFORMED\n");
  }

  if (t != 0.0) {
    mflops = (double)(2*NITER*NA)
                   * (3.0+(double)(NONZER*(NONZER+1))
                     + 25.0*(5.0+(double)(NONZER*(NONZER+1)))
                     + 3.0) / t / 1000000.0;
  } else {
    mflops = 0.0;
  }

  print_results("CG", Class, NA, 0, 0,
                NITER, t, processor_time,
                mflops, "          floating point", 
                verified, NPBVERSION, COMPILETIME,
                CS1, CS2, CS3, CS4, CS5, CS6, CS7);

  //---------------------------------------------------------------------
  // More timers
  //---------------------------------------------------------------------
  if (timeron) {
    tmax = timer_read(T_bench);
    if (tmax == 0.0) tmax = 1.0;
    printf("  SECTION   Time (secs)\n");
    for (i = 0; i < T_last; i++) {
      t = timer_read(i);
      if (i == T_init) {
        printf("  %8s:%9.3f\n", t_names[i], t);
      } else {
        printf("  %8s:%9.3f  (%6.2f%%)\n", t_names[i], t, t*100.0/tmax);
        if (i == T_conj_grad) {
          t = tmax - t;
          printf("    --> %8s:%9.3f  (%6.2f%%)\n", "rest", t, t*100.0/tmax);
        }
      }
    }
  }

  return 0;
}


//---------------------------------------------------------------------
// Floaging point arrays here are named as in NPB1 spec discussion of 
// CG algorithm
//---------------------------------------------------------------------
typedef struct {
    int thread_id;
    double* rho_local;
    double* d_local;
    double* sum_local;
    double alpha;
    double beta;
} conj_grad_thread_args_t;

void conj_grad_init_thread(void *args) {
    conj_grad_thread_args_t *args_ptr = (conj_grad_thread_args_t *)args;
    int thread_id = args_ptr->thread_id;
    
    int j_stop_original = naa + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    
    for (int j = j_start; j < j_stop; j++) {
        q[j] = 0.0;
        z[j] = 0.0;
        r[j] = x[j];
        p[j] = r[j];
    }
}

void conj_grad_rho_thread(void *args) {
    conj_grad_thread_args_t *args_ptr = (conj_grad_thread_args_t *)args;
    int thread_id = args_ptr->thread_id;
    double rho_local = 0.0;
    
    int j_stop_original = lastcol - firstcol + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    
    for (int j = j_start; j < j_stop; j++) {
        rho_local += r[j] * r[j];
    }
    
    *(args_ptr->rho_local) = rho_local;
}

void conj_grad_q_thread(void *args) {
    conj_grad_thread_args_t *args_ptr = (conj_grad_thread_args_t *)args;
    int thread_id = args_ptr->thread_id;
    
    int j_stop_original = lastrow - firstrow + 1;
    int nrows_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * nrows_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + nrows_per_thread;
    
    for (int j = j_start; j < j_stop; j++) {
        double suml = 0.0;
        for (int k = rowstr[j]; k < rowstr[j+1]; k++) {
            suml += a[k] * p[colidx[k]];
        }
        q[j] = suml;
    }
}

void conj_grad_d_thread(void *args) {
    conj_grad_thread_args_t *args_ptr = (conj_grad_thread_args_t *)args;
    int thread_id = args_ptr->thread_id;
    double d_local = 0.0;
    
    int j_stop_original = lastcol - firstcol + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    
    for (int j = j_start; j < j_stop; j++) {
        d_local += p[j] * q[j];
    }
    
    *(args_ptr->d_local) = d_local;
}

void conj_grad_update_thread(void *args) {
    conj_grad_thread_args_t *args_ptr = (conj_grad_thread_args_t *)args;
    int thread_id = args_ptr->thread_id;
    double rho_local = 0.0;
    double alpha = args_ptr->alpha;
    
    int j_stop_original = lastcol - firstcol + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    
    for (int j = j_start; j < j_stop; j++) {
        z[j] += alpha * p[j];
        r[j] -= alpha * q[j];
        //---------------------------------------------------------------------
        // rho = r.r
        // Now, obtain the norm of r: First, sum squares of r elements locally..
        //---------------------------------------------------------------------
        rho_local += r[j] * r[j];
    }
    
    *(args_ptr->rho_local) = rho_local;
}

void conj_grad_p_thread(void *args) {
    conj_grad_thread_args_t *args_ptr = (conj_grad_thread_args_t *)args;
    int thread_id = args_ptr->thread_id;
    double beta = args_ptr->beta;
    
    int j_stop_original = lastcol - firstcol + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    
    for (int j = j_start; j < j_stop; j++) {
        p[j] = r[j] + beta * p[j];
    }
}

void conj_grad_final_thread(void *args) {
    conj_grad_thread_args_t *args_ptr = (conj_grad_thread_args_t *)args;
    int thread_id = args_ptr->thread_id;
    double sum_local = 0.0;
    
    int j_stop_original = lastcol - firstcol + 1;
    int ncols_per_thread = j_stop_original / reduction_context.num_threads;
    int j_start = thread_id * ncols_per_thread;
    int j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + ncols_per_thread;
    
    for (int j = j_start; j < j_stop; j++) {
        double suml = x[j] - r[j];
        sum_local += suml * suml;
    }
    
    *(args_ptr->sum_local) = sum_local;
}

//---------------------------------------------------------------------
// Floaging point arrays here are named as in NPB1 spec discussion of 
// CG algorithm
//---------------------------------------------------------------------
static void conj_grad(int colidx[],
                      int rowstr[],
                      double x[],
                      double z[],
                      double a[],
                      double p[],
                      double q[],
                      double r[],
                      double *rnorm)
{
    int cgit, cgitmax = 25;
    double d, sum, rho, rho0, alpha, beta;
    
    conj_grad_thread_args_t* args = (conj_grad_thread_args_t*)malloc(
        reduction_context.num_threads * sizeof(conj_grad_thread_args_t));
    double* rho_values = (double*)malloc(reduction_context.num_threads * sizeof(double));
    double* d_values = (double*)malloc(reduction_context.num_threads * sizeof(double));
    double* sum_values = (double*)malloc(reduction_context.num_threads * sizeof(double));
    
    for (int i = 0; i < reduction_context.num_threads; i++) {
        args[i].thread_id = i;
        args[i].rho_local = &rho_values[i];
        args[i].d_local = &d_values[i];
        args[i].sum_local = &sum_values[i];
    }

    //---------------------------------------------------------------------
    // Initialize the CG algorithm:
    //---------------------------------------------------------------------
    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_create(
            reduction_context.pools[i % reduction_context.num_pools],
            conj_grad_init_thread,
            &args[i],
            ABT_THREAD_ATTR_NULL,
            &reduction_context.threads[i]
        );
    }
    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_join(reduction_context.threads[i]);
        ABT_thread_free(&reduction_context.threads[i]);
    }
    
    //---------------------------------------------------------------------
    // rho = r.r
    // Now, obtain the norm of r: First, sum squares of r elements locally...
    //---------------------------------------------------------------------
    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_create(
            reduction_context.pools[i % reduction_context.num_pools],
            conj_grad_rho_thread,
            &args[i],
            ABT_THREAD_ATTR_NULL,
            &reduction_context.threads[i]
        );
    }
    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_join(reduction_context.threads[i]);
        ABT_thread_free(&reduction_context.threads[i]);
    }
    rho = 0.0;
    reduce_sum_double(&reduction_context, rho_values, reduction_context.num_threads, &rho);
    
    //---------------------------------------------------------------------
    //---->
    // The conj grad iteration loop
    //---->
    //---------------------------------------------------------------------
    for (cgit = 1; cgit <= cgitmax; cgit++) {
        //---------------------------------------------------------------------
        // Save a temporary of rho and initialize reduction variables
        //---------------------------------------------------------------------
        rho0 = rho;
        d = 0.0;
        rho = 0.0;
        
        //---------------------------------------------------------------------
        // q = A.p
        //---------------------------------------------------------------------
        for (int i = 0; i < reduction_context.num_threads; i++) {
            ABT_thread_create(
                reduction_context.pools[i % reduction_context.num_pools],
                conj_grad_q_thread,
                &args[i],
                ABT_THREAD_ATTR_NULL,
                &reduction_context.threads[i]
            );
        }
        for (int i = 0; i < reduction_context.num_threads; i++) {
            ABT_thread_join(reduction_context.threads[i]);
            ABT_thread_free(&reduction_context.threads[i]);
        }
        
        //---------------------------------------------------------------------
        // Obtain p.q
        //---------------------------------------------------------------------
        for (int i = 0; i < reduction_context.num_threads; i++) {
            ABT_thread_create(
                reduction_context.pools[i % reduction_context.num_pools],
                conj_grad_d_thread,
                &args[i],
                ABT_THREAD_ATTR_NULL,
                &reduction_context.threads[i]
            );
        }
        for (int i = 0; i < reduction_context.num_threads; i++) {
            ABT_thread_join(reduction_context.threads[i]);
            ABT_thread_free(&reduction_context.threads[i]);
        }
        reduce_sum_double(&reduction_context, d_values, reduction_context.num_threads, &d);
        
        //---------------------------------------------------------------------
        // Obtain alpha = rho / (p.q)
        //---------------------------------------------------------------------
        alpha = rho0 / d;

        //---------------------------------------------------------------------
        // Obtain z = z + alpha*p
        // and    r = r - alpha*q
        //---------------------------------------------------------------------
        for (int i = 0; i < reduction_context.num_threads; i++) {
            args[i].alpha = alpha;
            ABT_thread_create(
                reduction_context.pools[i % reduction_context.num_pools],
                conj_grad_update_thread,
                &args[i],
                ABT_THREAD_ATTR_NULL,
                &reduction_context.threads[i]
            );
        }
        for (int i = 0; i < reduction_context.num_threads; i++) {
            ABT_thread_join(reduction_context.threads[i]);
            ABT_thread_free(&reduction_context.threads[i]);
        }
        reduce_sum_double(&reduction_context, rho_values, reduction_context.num_threads, &rho);
        
        //---------------------------------------------------------------------
        // Obtain beta:
        //---------------------------------------------------------------------
        beta = rho / rho0;

        //---------------------------------------------------------------------
        // p = r + beta*p
        //---------------------------------------------------------------------
        for (int i = 0; i < reduction_context.num_threads; i++) {
            args[i].beta = beta;
            ABT_thread_create(
                reduction_context.pools[i % reduction_context.num_pools],
                conj_grad_p_thread,
                &args[i],
                ABT_THREAD_ATTR_NULL,
                &reduction_context.threads[i]
            );
        }
        for (int i = 0; i < reduction_context.num_threads; i++) {
            ABT_thread_join(reduction_context.threads[i]);
            ABT_thread_free(&reduction_context.threads[i]);
        }
    } // end of do cgit=1,cgitmax
    
    // Calculate final residual norm
    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_create(
            reduction_context.pools[i % reduction_context.num_pools],
            conj_grad_final_thread,
            &args[i],
            ABT_THREAD_ATTR_NULL,
            &reduction_context.threads[i]
        );
    }
    for (int i = 0; i < reduction_context.num_threads; i++) {
        ABT_thread_join(reduction_context.threads[i]);
        ABT_thread_free(&reduction_context.threads[i]);
    }
    sum = 0.0;
    reduce_sum_double(&reduction_context, sum_values, reduction_context.num_threads, &sum);
    
    *rnorm = sqrt(sum);
    
    free(args);
    free(rho_values);
    free(d_values);
    free(sum_values);
}


//---------------------------------------------------------------------
// generate the test problem for benchmark 6
// makea generates a sparse matrix with a
// prescribed sparsity distribution
//
// parameter    type        usage
//
// input
//
// n            i           number of cols/rows of matrix
// nz           i           nonzeros as declared array size
// rcond        r*8         condition number
// shift        r*8         main diagonal shift
//
// output
//
// a            r*8         array for nonzeros
// colidx       i           col indices
// rowstr       i           row pointers
//
// workspace
//
// iv, arow, acol i
// aelt           r*8
//---------------------------------------------------------------------
static void makea(int thread_id,
                  double *tran_ptr,
                  int n,
                  int nz,
                  double a[],
                  int colidx[],
                  int rowstr[],
                  int firstrow,
                  int lastrow,
                  int firstcol,
                  int lastcol,
                  int arow[],
                  int acol[][NONZER+1],
                  double aelt[][NONZER+1],
                  double v[],
                  int iv[])
{
  int iouter, ivelt, nzv, nn1;
  int ivc[NONZER+1];
  double vc[NONZER+1];

  //---------------------------------------------------------------------
  // nonzer is approximately  (int(sqrt(nnza /n)));
  //---------------------------------------------------------------------
  int work; 

  //---------------------------------------------------------------------
  // nn1 is the smallest power of two not less than n
  //---------------------------------------------------------------------
  nn1 = 1;
  do {
    nn1 = 2 * nn1;
  } while (nn1 < n);

  //---------------------------------------------------------------------
  // Generate nonzero positions and save for the use in sparse.
  //---------------------------------------------------------------------

  int num_threads = reduction_context.num_threads;
  if (num_threads > max_threads) {
    if (thread_id == 0) {
      printf(" Warning: num_threads%6d exceeded an internal limit%6d\n",
          num_threads, max_threads);
    }
    exit(EXIT_FAILURE);
  }
  work  = (n + num_threads - 1)/num_threads;
  int ilow  = work * thread_id;
  int ihigh = ilow + work;
  if (ihigh > n) ihigh = n;
  
  for (iouter = 0; iouter < ihigh; iouter++) {
    nzv = NONZER;
    sprnvc(tran_ptr, n, nzv, nn1, vc, ivc);
    if (iouter >= ilow) {
      vecset(n, vc, ivc, &nzv, iouter+1, 0.5);
      arow[iouter] = nzv;
      for (ivelt = 0; ivelt < nzv; ivelt++) {
        acol[iouter][ivelt] = ivc[ivelt] - 1;
        aelt[iouter][ivelt] = vc[ivelt];
      }
    }
  }

  ABT_barrier_wait(barrier);

  //---------------------------------------------------------------------
  // ... make the sparse matrix from list of elements with duplicates
  //     (v and iv are used as  workspace)
  //---------------------------------------------------------------------
  sparse(thread_id, ilow, ihigh, a, colidx, rowstr, n, nz, NONZER, arow, acol, 
         aelt, firstrow, lastrow, last_n,
         v, &iv[0], &iv[nz], RCOND, SHIFT);
}


//---------------------------------------------------------------------
// rows range from firstrow to lastrow
// the rowstr pointers are defined for nrows = lastrow-firstrow+1 values
//---------------------------------------------------------------------
static void sparse(int thread_id,
                   int ilow,
                   int ihigh,
                   double a[],
                   int colidx[],
                   int rowstr[],
                   int n,
                   int nz,
                   int nozer,
                   int arow[],
                   int acol[][NONZER+1],
                   double aelt[][NONZER+1],
                   int firstrow,
                   int lastrow,
                   int last_n[],
                   double v[],
                   int iv[],
                   int nzloc[],
                   double rcond,
                   double shift)
{
  int nrows;

  //---------------------------------------------------
  // generate a sparse matrix from a list of
  // [col, row, element] tri
  //---------------------------------------------------
  int i, j, j1, j2, nza, k, kk, nzrow, jcol;
  double size, scale, ratio, va;
  logical cont40;

  //---------------------------------------------------------------------
  // how many rows of result
  //---------------------------------------------------------------------
  nrows = lastrow - firstrow + 1;
  j1 = ilow + 1;
  j2 = ihigh + 1;

  //---------------------------------------------------------------------
  // ...count the number of triples in each row
  //---------------------------------------------------------------------
  for (j = j1; j < j2; j++) {
    rowstr[j] = 0;
  }

  for (i = 0; i < n; i++) {
    for (nza = 0; nza < arow[i]; nza++) {
      j = acol[i][nza];
      if (j >= ilow && j < ihigh) {
        j = j + 1;
        rowstr[j] = rowstr[j] + arow[i];
      }
    }
  }

  if (thread_id == 0) {
    rowstr[0] = 0;
    j1 = 0;
  }
  for (j = j1+1; j < j2; j++) {
    rowstr[j] = rowstr[j] + rowstr[j-1];
  }
  if (thread_id < reduction_context.num_threads) last_n[thread_id] = rowstr[j2-1];
  ABT_barrier_wait(barrier);

  nzrow = 0;
  if (thread_id < reduction_context.num_threads) {
    for (i = 0; i < thread_id; i++) {
      nzrow = nzrow + last_n[i];
    }
  }
  if (nzrow > 0) {
    for (j = j1; j < j2; j++) {
      rowstr[j] = rowstr[j] + nzrow;
    }
  }
  ABT_barrier_wait(barrier);

  nza = rowstr[nrows] - 1;

  //---------------------------------------------------------------------
  // ... rowstr(j) now is the location of the first nonzero
  //     of row j of a
  //---------------------------------------------------------------------
  if (nza > nz) {
    if (thread_id == 0) {
      printf("Space for matrix elements exceeded in sparse\n");
      printf("nza, nzmax = %d, %d\n", nza, nz);
    }
    exit(EXIT_FAILURE);
  }

  //---------------------------------------------------------------------
  // ... preload data pages
  //---------------------------------------------------------------------
  for (j = ilow; j < ihigh; j++) {
    for (k = rowstr[j]; k < rowstr[j+1]; k++) {
      v[k] = 0.0;
      iv[k] = -1;
    }
    nzloc[j] = 0;
  }

  //---------------------------------------------------------------------
  // ... generate actual values by summing duplicates
  //---------------------------------------------------------------------
  size = 1.0;
  ratio = pow(rcond, (1.0 / (double)(n)));

  for (i = 0; i < n; i++) {
    for (nza = 0; nza < arow[i]; nza++) {
      j = acol[i][nza];

      if (j < ilow || j >= ihigh) continue;

      scale = size * aelt[i][nza];
      for (nzrow = 0; nzrow < arow[i]; nzrow++) {
        jcol = acol[i][nzrow];
        va = aelt[i][nzrow] * scale;

        //--------------------------------------------------------------------
        // ... add the identity * rcond to the generated matrix to bound
        //     the smallest eigenvalue from below by rcond
        //--------------------------------------------------------------------
        if (jcol == j && j == i) {
          va = va + rcond - shift;
        }

        cont40 = false;
        for (k = rowstr[j]; k < rowstr[j+1]; k++) {
          if (iv[k] > jcol) {
            //----------------------------------------------------------------
            // ... insert colidx here orderly
            //----------------------------------------------------------------
            for (kk = rowstr[j+1]-2; kk >= k; kk--) {
              if (iv[kk] > -1) {
                v[kk+1]  = v[kk];
                iv[kk+1] = iv[kk];
              }
            }
            iv[k] = jcol;
            v[k]  = 0.0;
            cont40 = true;
            break;
          } else if (iv[k] == -1) {
            iv[k] = jcol;
            cont40 = true;
            break;
          } else if (iv[k] == jcol) {
            //--------------------------------------------------------------
            // ... mark the duplicated entry
            //--------------------------------------------------------------
            nzloc[j] = nzloc[j] + 1;
            cont40 = true;
            break;
          }
        }
        if (cont40 == false) {
          printf("internal error in sparse: i=%d\n", i);
          exit(EXIT_FAILURE);
        }
        v[k] = v[k] + va;
      }
    }
    size = size * ratio;
  }
  ABT_barrier_wait(barrier);


  //---------------------------------------------------------------------
  // ... remove empty entries and generate final results
  //---------------------------------------------------------------------
  for (j = ilow+1; j < ihigh; j++) {
    nzloc[j] = nzloc[j] + nzloc[j-1];
  }
  if (thread_id < reduction_context.num_threads) last_n[thread_id] = nzloc[ihigh-1];
  ABT_barrier_wait(barrier);

  nzrow = 0;
  if (thread_id < reduction_context.num_threads) {
    for (i = 0; i < thread_id; i++) {
      nzrow = nzrow + last_n[i];
    }
  }
  if (nzrow > 0) {
    for (j = ilow; j < ihigh; j++) {
      nzloc[j] = nzloc[j] + nzrow;
    }
  }
  ABT_barrier_wait(barrier);

  int nrows_per_thread = nrows / reduction_context.num_threads;
  int j_start = thread_id * nrows_per_thread;
  int j_stop = (thread_id == reduction_context.num_threads - 1) ? nrows : j_start + nrows_per_thread;
  for (j = j_start; j < j_stop; j++) {
    if (j > 0) {
      j1 = rowstr[j] - nzloc[j-1];
    } else {
      j1 = 0;
    }
    j2 = rowstr[j+1] - nzloc[j];
    nza = rowstr[j];
    for (k = j1; k < j2; k++) {
      a[k] = v[nza];
      colidx[k] = iv[nza];
      nza = nza + 1;
    }
  }

  int j_start_original = 1;
  int j_stop_original = nrows + 1;
  j_start = j_start_original + thread_id * nrows_per_thread;
  j_stop = (thread_id == reduction_context.num_threads - 1) ? j_stop_original : j_start + nrows_per_thread;
  for (j = j_start; j < j_stop; j++) {
    rowstr[j] = rowstr[j] - nzloc[j-1];
  }
  nza = rowstr[nrows] - 1;
}


//---------------------------------------------------------------------
// generate a sparse n-vector (v, iv)
// having nzv nonzeros
//
// mark(i) is set to 1 if position i is nonzero.
// mark is all zero on entry and is reset to all zero before exit
// this corrects a performance bug found by John G. Lewis, caused by
// reinitialization of mark on every one of the n calls to sprnvc
//---------------------------------------------------------------------
static void sprnvc(double *tran_ptr, int n, int nz, int nn1, double v[], int iv[])
{
  int nzv, ii, i;
  double vecelt, vecloc;

  nzv = 0;

  while (nzv < nz) {
    vecelt = randlc(tran_ptr, amult);

    //---------------------------------------------------------------------
    // generate an integer between 1 and n in a portable manner
    //---------------------------------------------------------------------
    vecloc = randlc(tran_ptr, amult);
    i = icnvrt(vecloc, nn1) + 1;
    if (i > n) continue;

    //---------------------------------------------------------------------
    // was this integer generated already?
    //---------------------------------------------------------------------
    logical was_gen = false;
    for (ii = 0; ii < nzv; ii++) {
      if (iv[ii] == i) {
        was_gen = true;
        break;
      }
    }
    if (was_gen) continue;
    v[nzv] = vecelt;
    iv[nzv] = i;
    nzv = nzv + 1;
  }
}


//---------------------------------------------------------------------
// scale a double precision number x in (0,1) by a power of 2 and chop it
//---------------------------------------------------------------------
static int icnvrt(double x, int ipwr2)
{
  return (int)(ipwr2 * x);
}


//---------------------------------------------------------------------
// set ith element of sparse vector (v, iv) with
// nzv nonzeros to val
//---------------------------------------------------------------------
static void vecset(int n, double v[], int iv[], int *nzv, int i, double val)
{
  int k;
  logical set;

  set = false;
  for (k = 0; k < *nzv; k++) {
    if (iv[k] == i) {
      v[k] = val;
      set  = true;
    }
  }
  if (set == false) {
    v[*nzv]  = val;
    iv[*nzv] = i;
    *nzv     = *nzv + 1;
  }
}

