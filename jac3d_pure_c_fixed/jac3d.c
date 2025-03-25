/* Jacobi-3 program */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define Max(a, b) ((a) > (b) ? (a) : (b))

#define L 384
int ITMAX = 100;

int i, j, k, it;
float eps;
float MAXEPS = 0.5f;

/* 3D arrays block distributed along 3 dimensions */
float A[L][L][L];
float B[L][L][L];

int main(int argc, char **argv)
{   
    for (i = 0; i < L; i++)
        for (j = 0; j < L; j++)
            for (k = 0; k < L; k++)
            {
                A[i][j][k] = 0;
                if (i == 0 || j == 0 || k == 0 || i == L - 1 || j == L - 1 || k == L - 1)
                    B[i][j][k] = 0;
                else
                    B[i][j][k] = 4 + i + j + k;
            }

    clock_t start = clock();
    struct timespec start_real_time;
    clock_gettime(CLOCK_REALTIME, &start_real_time);
    /* iteration loop */
    for (it = 1; it <= ITMAX; it++)
    {
        eps = 0;
        /* calculating maximum in variable eps */
        for (i = 1; i < L - 1; i++)
            for (j = 1; j < L - 1; j++)
                for (k = 1; k < L - 1; k++)
                {
                    float tmp = fabs(B[i][j][k] - A[i][j][k]);
                    eps = Max(tmp, eps);
                    A[i][j][k] = B[i][j][k];
                }

        for (i = 1; i < L - 1; i++)
            for (j = 1; j < L - 1; j++)
                for (k = 1; k < L - 1; k++)
                    B[i][j][k] = (A[i - 1][j][k] + A[i][j - 1][k] + A[i][j][k - 1] + A[i][j][k + 1] + A[i][j + 1][k] + A[i + 1][j][k]) / 6.0f;

        // printf(" IT = %4i   EPS = %14.7E\n", it, eps)
        if (eps < MAXEPS)
            break;
    }
    clock_t end = clock();
    struct timespec end_real_time;
    clock_gettime(CLOCK_REALTIME, &end_real_time);
    double time_in_seconds = ((double)(end - start)) / CLOCKS_PER_SEC;
    long long real_time_nanoseconds = (end_real_time.tv_sec - start_real_time.tv_sec) * 1000000000 + (end_real_time.tv_nsec - start_real_time.tv_nsec);

    printf(" Jacobi3D Benchmark Completed.\n");
    printf(" Size            = %4d x %4d x %4d\n", L, L, L);
    printf(" Iterations      =       %12d\n", ITMAX);
    printf(" Time in seconds =       %12.2lf\n", time_in_seconds);
    printf(" Real time (nanos) =       %12lld\n", real_time_nanoseconds);
    printf(" Operation type  =     floating point\n");
    printf(" Verification    =       %12s\n", (fabs(eps - 5.058044) < 1e-4 ? "SUCCESSFUL" : "UNSUCCESSFUL"));
    printf(" EPS = %f\n", eps);

    printf(" END OF Jacobi3D Benchmark\n");
    return 0;
}
