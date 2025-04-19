/* Jacobi-3 program */

#include <math.h>
#include <stdio.h>
#include <omp.h>
#include <time.h>

#define Max(a, b) ((a) > (b) ? (a) : (b))

#define L 384
#define ITMAX 100
#define TILE_SIZE 32

int i, j, k, it;
float eps;
float MAXEPS = 0.5f;

/* 3D arrays */
float A[L][L][L];
float B[L][L][L];

int main(int an, char **as)
{
    double startt, endt;
    clock_t start, end;
    
    /* Initialize arrays */
    #pragma omp parallel for collapse(3) private(i,j,k) schedule(static)
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

    startt = omp_get_wtime();
    start = clock();

    /* iteration loop */
    for (it = 1; it <= ITMAX; it++)
    {
        eps = 0;

        /* Calculate maximum difference and update A using tiled approach */
        #pragma omp parallel private(i,j,k) reduction(max:eps)
        {
            float local_eps = 0;
            
            #pragma omp for schedule(static)
            for (i = 1; i < L - 1; i += TILE_SIZE)
                for (j = 1; j < L - 1; j += TILE_SIZE)
                    for (k = 1; k < L - 1; k += TILE_SIZE)
                    {
                        int i_end = (i + TILE_SIZE) < (L - 1) ? (i + TILE_SIZE) : (L - 1);
                        int j_end = (j + TILE_SIZE) < (L - 1) ? (j + TILE_SIZE) : (L - 1);
                        int k_end = (k + TILE_SIZE) < (L - 1) ? (k + TILE_SIZE) : (L - 1);
                        
                        for (int ii = i; ii < i_end; ii++)
                            for (int jj = j; jj < j_end; jj++)
                                for (int kk = k; kk < k_end; kk++)
                                {
                                    float tmp = fabs(B[ii][jj][kk] - A[ii][jj][kk]);
                                    local_eps = Max(tmp, local_eps);
                                    A[ii][jj][kk] = B[ii][jj][kk];
                                }
                    }
            eps = Max(eps, local_eps);
        }

        /* Update B using values from A with tiled approach */
        #pragma omp parallel private(i,j,k)
        {
            #pragma omp for schedule(static)
            for (i = 1; i < L - 1; i += TILE_SIZE)
                for (j = 1; j < L - 1; j += TILE_SIZE)
                    for (k = 1; k < L - 1; k += TILE_SIZE)
                    {
                        int i_end = (i + TILE_SIZE) < (L - 1) ? (i + TILE_SIZE) : (L - 1);
                        int j_end = (j + TILE_SIZE) < (L - 1) ? (j + TILE_SIZE) : (L - 1);
                        int k_end = (k + TILE_SIZE) < (L - 1) ? (k + TILE_SIZE) : (L - 1);
                        
                        for (int ii = i; ii < i_end; ii++)
                            for (int jj = j; jj < j_end; jj++)
                                for (int kk = k; kk < k_end; kk++)
                                    B[ii][jj][kk] = (A[ii - 1][jj][kk] + A[ii][jj - 1][kk] + A[ii][jj][kk - 1] + 
                                                   A[ii][jj][kk + 1] + A[ii][jj + 1][kk] + A[ii + 1][jj][kk]) / 6.0f;
                    }
        }

        // printf(" IT = %4i   EPS = %14.7E\n", it, eps);
        if (eps < MAXEPS)
            break;
    }

    endt = omp_get_wtime();
    end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf(" Jacobi3D Benchmark Completed.\n");
    printf(" Size            = %4d x %4d x %4d\n", L, L, L);
    printf(" Iterations      =       %12d\n", ITMAX);
    printf(" Time in seconds   =       %12.2lf\n", cpu_time_used);
    printf(" Real time (nanos) =       %12.2lf\n", endt - startt);
    printf(" Operation type  =     floating point\n");
    printf(" Verification    =       %12s\n", (fabs(eps - 5.058044) < 1e-4 ? "SUCCESSFUL" : "UNSUCCESSFUL"));

    printf(" END OF Jacobi3D Benchmark\n");
    return 0;
}
