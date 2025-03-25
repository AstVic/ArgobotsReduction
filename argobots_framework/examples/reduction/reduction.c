/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/*
 * Creates multiple execution streams and runs ULTs on these execution streams.
 * Users can change the number of execution streams and the number of ULT via
 * arguments. Each ULT performs a reduction operation on a given array.
 */

#include "abt_reduction.h"

#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_NUM_XSTREAMS 2
#define DEFAULT_NUM_THREADS 8

#define NUM_ELEMS 1024

int check_not_equal(int result, int expected, const char *test_name) {
    printf("%s: result=%d, expected=%d\n", test_name, result, expected);
    return result != expected;
}

int check_not_equal_float(float result, float expected, const char *test_name) {
    printf("%s: result=%f, expected=%f\n", test_name, result, expected);
    // fabs (absolute value) is used to compare floats
    return fabs(result - expected) > FLT_EPSILON;
}

int test_sum_int(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    int *array = (int *)malloc(sizeof(int) * NUM_ELEMS);
    int result = 0;

    // all zeroes array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 0;
    }
    reduce_sum_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 0, "int_sum_all_zeroes");
    result = 0;

    // one element is not zero
    enum { NON_ZERO_VALUE = 123 };
    array[0] = NON_ZERO_VALUE;
    reduce_sum_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, NON_ZERO_VALUE, "int_sum_one_not_zero");
    result = 0;

    // only ones
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_sum_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, NUM_ELEMS, "int_sum_all_ones");
    result = 0;

    // arithmetic progression
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = idx;
    }
    reduce_sum_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, (NUM_ELEMS * (NUM_ELEMS - 1)) / 2, "int_sum_arithmetic_progression");
    result = 0;

    // free resources
    free(array);

    return bad_tests;
}

int test_sum_float(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    float *array = (float *)malloc(sizeof(float) * NUM_ELEMS);
    float result = 0;

    // all zeroes array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 0;
    }
    reduce_sum_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 0, "float_sum_all_zeroes");
    result = 0;

    // one element is not zero
    enum { NON_ZERO_VALUE = 123 };
    array[0] = NON_ZERO_VALUE;
    reduce_sum_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, NON_ZERO_VALUE, "float_sum_one_not_zero");
    result = 0;

    // only ones
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_sum_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, NUM_ELEMS, "float_sum_all_ones");
    result = 0;

    // arithmetic progression
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = idx;
    }
    reduce_sum_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, (NUM_ELEMS * (NUM_ELEMS - 1)) / 2, "float_sum_arithmetic_progression");
    result = 0;

    // free resources
    free(array);

    return bad_tests;
}

int test_prod_int(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    int *array = (int *)malloc(sizeof(int) * NUM_ELEMS);
    int result = 1;

    // all ones array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_prod_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 1, "int_mul_all_ones");
    result = 1;

    // one element is not one
    enum { NON_ONE_VALUE = 123 };
    array[0] = NON_ONE_VALUE;
    reduce_prod_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, NON_ONE_VALUE, "int_mul_one_not_one");
    result = 1;

    // one element is zero
    array[0] = 0;
    reduce_prod_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 0, "int_mul_one_zero");
    result = 1;

    // first 6 elems are powers of 2; others are 1
    for (size_t idx = 0; idx < 6; ++idx) {
      array[idx] = 1 << idx;
    }
    for (size_t idx = 6; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_prod_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 32768, "int_mul_powers_of_2");
    result = 1;

    // free resources
    free(array);

    return bad_tests;
}

int test_prod_float(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    float *array = (float *)malloc(sizeof(float) * NUM_ELEMS);
    float result = 1;

    // all ones array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_prod_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 1, "float_mul_all_ones");
    result = 1;

    // one element is not one
    enum { NON_ONE_VALUE = 123 };
    array[0] = NON_ONE_VALUE;
    reduce_prod_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, NON_ONE_VALUE, "float_mul_one_not_one");
    result = 1;

    // one element is zero
    array[0] = 0;
    reduce_prod_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 0, "float_mul_one_zero");
    result = 1;

    // first 6 elems are powers of 2
    for (size_t idx = 0; idx < 6; ++idx) {
      array[idx] = 1 << idx;
    }
    for (size_t idx = 6; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_prod_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 32768, "float_mul_powers_of_2");
    result = 1;

    // free resources
    free(array);

    return bad_tests;
}

int test_max_int(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    int *array = (int *)malloc(sizeof(int) * NUM_ELEMS);
    int result = INT_MIN;

    // all ones array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_max_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 1, "int_max_all_ones");
    result = INT_MIN;

    // one element is not one
    enum { NON_ONE_VALUE = 123 };
    array[0] = NON_ONE_VALUE;
    reduce_max_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, NON_ONE_VALUE, "int_max_one_not_one");
    result = INT_MIN;

    // first element is zero
    array[0] = 0;
    reduce_max_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 1, "int_max_first_zero");
    result = INT_MIN;

    // array[idx] = idx
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = idx;
    }
    reduce_max_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, NUM_ELEMS - 1, "int_max_array_idx");
    result = INT_MIN;

    // first elem INT_MAX
    array[0] = INT_MAX;
    reduce_max_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, INT_MAX, "int_max_first_int_max");
    result = INT_MIN;

    // free resources
    free(array);

    return bad_tests;
}

int test_max_float(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    float *array = (float *)malloc(sizeof(float) * NUM_ELEMS);
    float result = FLT_MIN;

    // all ones array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_max_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 1, "float_max_all_ones");
    result = FLT_MIN;

    // one element is not one
    enum { NON_ONE_VALUE = 123 };
    array[0] = NON_ONE_VALUE;
    reduce_max_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, NON_ONE_VALUE, "float_max_one_not_one");
    result = FLT_MIN;

    // first element is zero
    array[0] = 0;
    reduce_max_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 1, "float_max_first_zero");
    result = FLT_MIN;

    // array[idx] = idx
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = idx;
    }
    reduce_max_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, NUM_ELEMS - 1, "float_max_array_idx");
    result = FLT_MIN;

    // first elem FLT_MAX
    array[0] = FLT_MAX;
    reduce_max_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, FLT_MAX, "float_max_first_float_max");
    result = FLT_MIN;

    // free resources
    free(array);

    return bad_tests;
}

int test_min_int(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    int *array = (int *)malloc(sizeof(int) * NUM_ELEMS);
    int result = INT_MAX;

    // all ones array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_min_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 1, "int_min_all_ones");
    result = INT_MAX;

    // one element is not one
    enum { NON_ONE_VALUE = 123 };
    array[0] = NON_ONE_VALUE;
    reduce_min_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 1, "int_min_one_not_one");
    result = INT_MAX;

    // first element is zero
    array[0] = 0;
    reduce_min_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 0, "int_min_first_zero");
    result = INT_MAX;

    // array[idx] = idx
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = idx;
    }
    reduce_min_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, 0, "int_min_array_idx");
    result = INT_MAX;

    // first elem INT_MIN
    array[0] = INT_MIN;
    reduce_min_int(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal(result, INT_MIN, "int_min_first_int_min");
    result = INT_MAX;

    // free resources
    free(array);

    return bad_tests;
}

int test_min_float(reduction_context_t* reduction_context) {
    // init
    int bad_tests = 0;
    float *array = (float *)malloc(sizeof(float) * NUM_ELEMS);
    float result = FLT_MAX;

    // all ones array
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = 1;
    }
    reduce_min_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 1, "float_min_all_ones");
    result = FLT_MAX;

    // one element is not one
    enum { NON_ONE_VALUE = 123 };
    array[0] = NON_ONE_VALUE;
    reduce_min_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 1, "float_min_one_not_one");
    result = FLT_MAX;

    // first element is zero
    array[0] = 0;
    reduce_min_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 0, "float_min_first_zero");
    result = FLT_MAX;

    // array[idx] = idx
    for (size_t idx = 0; idx < NUM_ELEMS; ++idx) {
      array[idx] = idx;
    }
    reduce_min_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, 0, "float_min_array_idx");
    result = FLT_MAX;

    // first elem FLT_MIN
    array[0] = FLT_MIN;
    reduce_min_float(
        reduction_context,
        array,
        NUM_ELEMS,
        &result
    );
    bad_tests += check_not_equal_float(result, FLT_MIN, "float_min_first_float_min");
    result = FLT_MAX;

    // free resources
    free(array);

    return bad_tests;
}

int test_different_reductions(reduction_context_t* reduction_context) {
    int bad_tests = 0;

    bad_tests += test_sum_int(reduction_context);
    bad_tests += test_sum_float(reduction_context);
    bad_tests += test_prod_int(reduction_context);
    bad_tests += test_prod_float(reduction_context);
    bad_tests += test_max_int(reduction_context);
    bad_tests += test_max_float(reduction_context);
    bad_tests += test_min_int(reduction_context);
    bad_tests += test_min_float(reduction_context);

    return bad_tests;
}


int main(int argc, char **argv)
{
    int i;
    /* Read arguments. */
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    while (1) {
        int opt = getopt(argc, argv, "he:n:");
        if (opt == -1)
            break;
        switch (opt) {
            case 'e':
                num_xstreams = atoi(optarg);
                break;
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 'h':
            default:
                printf("Usage: ./reduction_sum [-e NUM_XSTREAMS] "
                       "[-n NUM_THREADS]\n");
                return -1;
        }
    }
    if (num_xstreams <= 0)
        num_xstreams = 1;
    if (num_threads <= 0)
        num_threads = 1;

    /* Allocate memory. */
    ABT_xstream *xstreams =
        (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    int num_pools = num_xstreams;
    ABT_pool *pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_pools);
    ABT_thread *threads =
        (ABT_thread *)malloc(sizeof(ABT_thread) * num_threads);
    
    /* Initialize Argobots. */
    ABT_init(argc, argv);

    /* Get a primary execution stream. */
    ABT_xstream_self(&xstreams[0]);

    /* Create secondary execution streams. */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
    }

    /* Get default pools. */
    for (i = 0; i < num_xstreams; i++) {
        ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
    }

    reduction_context_t reduction_context = {
        .xstreams = xstreams,
        .num_xstreams = num_xstreams,
        .pools = pools,
        .num_pools = num_pools,
        .threads = threads,
        .num_threads = num_threads,
    };

    int failed_tests = test_different_reductions(&reduction_context);
    if (failed_tests > 0) {
        printf("Failed %d tests\n", failed_tests);
        return -1;
    }

    /* Free ULTs. */
    for (i = 0; i < num_threads; i++) {
        ABT_thread_free(&threads[i]);
    }

    /* Join and free secondary execution streams. */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    /* Finalize Argobots. */
    ABT_finalize();

    /* Free allocated memory. */
    free(xstreams);
    free(pools);
    free(threads);

    return 0;
}
