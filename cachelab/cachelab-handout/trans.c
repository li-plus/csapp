/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32) {
        for (int bi = 0; bi < N; bi += 8) {
            for (int bj = 0; bj < M; bj += 8) {
                for (int i = bi; i < bi + 8; i++) {
                    int t0 = A[i][bj + 0];
                    int t1 = A[i][bj + 1];
                    int t2 = A[i][bj + 2];
                    int t3 = A[i][bj + 3];
                    int t4 = A[i][bj + 4];
                    int t5 = A[i][bj + 5];
                    int t6 = A[i][bj + 6];
                    int t7 = A[i][bj + 7];

                    B[bj + 0][i] = t0;
                    B[bj + 1][i] = t1;
                    B[bj + 2][i] = t2;
                    B[bj + 3][i] = t3;
                    B[bj + 4][i] = t4;
                    B[bj + 5][i] = t5;
                    B[bj + 6][i] = t6;
                    B[bj + 7][i] = t7;
                }
            }
        }
    } else if (M == 64 && N == 64) {
        for (int bi = 0; bi < N; bi += 8) {
            for (int bj = 0; bj < M; bj += 8) {
                int t0, t1, t2, t3, t4, t5, t6, t7;
                for (int i = bi; i < bi + 4; i++) {
                    t0 = A[i][bj + 0];
                    t1 = A[i][bj + 1];
                    t2 = A[i][bj + 2];
                    t3 = A[i][bj + 3];
                    t4 = A[i][bj + 4];
                    t5 = A[i][bj + 5];
                    t6 = A[i][bj + 6];
                    t7 = A[i][bj + 7];
                    B[bj + 0][i] = t0;
                    B[bj + 1][i] = t1;
                    B[bj + 2][i] = t2;
                    B[bj + 3][i] = t3;
                    B[bj + 0][i + 4] = t4;
                    B[bj + 1][i + 4] = t5;
                    B[bj + 2][i + 4] = t6;
                    B[bj + 3][i + 4] = t7;
                }
                for (int j = bj; j < bj + 4; j++) {
                    t0 = A[bi + 4][j];
                    t1 = A[bi + 5][j];
                    t2 = A[bi + 6][j];
                    t3 = A[bi + 7][j];
                    t4 = B[j][bi + 4];
                    t5 = B[j][bi + 5];
                    t6 = B[j][bi + 6];
                    t7 = B[j][bi + 7];
                    B[j][bi + 4] = t0;
                    B[j][bi + 5] = t1;
                    B[j][bi + 6] = t2;
                    B[j][bi + 7] = t3;
                    t0 = A[bi + 4][j + 4];
                    t1 = A[bi + 5][j + 4];
                    t2 = A[bi + 6][j + 4];
                    t3 = A[bi + 7][j + 4];
                    B[j + 4][bi + 0] = t4;
                    B[j + 4][bi + 1] = t5;
                    B[j + 4][bi + 2] = t6;
                    B[j + 4][bi + 3] = t7;
                    B[j + 4][bi + 4] = t0;
                    B[j + 4][bi + 5] = t1;
                    B[j + 4][bi + 6] = t2;
                    B[j + 4][bi + 7] = t3;
                }
            }
        }
    } else {
        for (int bi = 0; bi < N; bi += 16) {
            for (int bj = 0; bj < M; bj += 16) {
                for (int i = bi; i < bi + 16 && i < N; i++) {
                    for (int j = bj; j < bj + 16 && j < M; j++) {
                        B[j][i] = A[i][j];
                    }
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

