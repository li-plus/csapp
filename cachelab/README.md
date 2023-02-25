# Cache Lab Solution

Checkout the code in [csim.c](cachelab-handout/csim.c) and [trans.c](cachelab-handout/trans.c). To evaluate the implementation, run `make && ./driver.py`.

Final results:
```
Cache Lab summary:
                        Points   Max pts      Misses
Csim correctness          27.0        27
Trans perf 32x32           8.0         8         287
Trans perf 64x64           8.0         8        1139
Trans perf 61x67          10.0        10        1992
          Total points    53.0        53
```

One thing worth mentioning is the transpose function of a 64x64 matrix, on a 1KB direct mapped cache with a block size of 32 bytes. Every 4 rows in the matrix fill up the cache. Each cache line stores 8 integers. To make full use of the cache line, divide the matrix into 8x8 blocks. Following the steps below, we get a near 1/8 miss rate.
```
Step 0: Initial state.
                                      Block A
                            +-------------------------+
                            | 00 01 02 03 04 05 06 07 |
                            | 10 11 12 13 14 15 16 17 |
                            | 20 21 22 23 24 25 26 27 |
                            | 30 31 32 33 34 35 36 37 |
                            +-------------------------+
                            | 40 41 42 43 44 45 46 47 |
                            | 50 51 52 53 54 55 56 57 |
                            | 60 61 62 63 64 65 66 67 |
                            | 70 71 72 73 74 75 76 77 |
                            +-------------------------+
+-------------------------+
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
+-------------------------+
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
+-------------------------+
          Block B

Step 1:
* Load A[0, 0:8] into registers.
* Store A[0, 0:4] into B[0:4, 0] and A[0, 4:8] into B[0:4, 4].
                                      Block A
                            +-------------------------+
                            | -- -- -- -- -- -- -- -- |
                            | 10 11 12 13 14 15 16 17 |
                            | 20 21 22 23 24 25 26 27 |
                            | 30 31 32 33 34 35 36 37 |
                            +-------------------------+
                            | 40 41 42 43 44 45 46 47 |
                            | 50 51 52 53 54 55 56 57 |
                            | 60 61 62 63 64 65 66 67 |
                            | 70 71 72 73 74 75 76 77 |
                            +-------------------------+
+-------------------------+
| 00 xx xx xx 04 xx xx xx |
| 01 xx xx xx 05 xx xx xx |
| 02 xx xx xx 06 xx xx xx |
| 03 xx xx xx 07 xx xx xx |
+-------------------------+
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
+-------------------------+
          Block B

Step 2: Repeat Step 1 on the next 3 lines of block A.
                                      Block A
                            +-------------------------+
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            +-------------------------+
                            | 40 41 42 43 44 45 46 47 |
                            | 50 51 52 53 54 55 56 57 |
                            | 60 61 62 63 64 65 66 67 |
                            | 70 71 72 73 74 75 76 77 |
                            +-------------------------+
+-------------------------+
| 00 10 20 30 04 14 24 34 |
| 01 11 21 31 05 15 25 35 |
| 02 12 22 32 06 16 26 36 |
| 03 13 23 33 07 17 27 37 |
+-------------------------+
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
+-------------------------+
          Block B

Step 3:
* Load A[4:8, 0] into registers and B[0, 0:4] into registers.
* Store A[4:8, 0] into B[0, 0:4].
* Load A[4:8, 4] into registers.
* Store the original B[0, 0:4] into B[4, 0:4], and A[4:8, 4] into B[4, 4:8].
                                      Block A
                            +-------------------------+
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            +-------------------------+
                            | -- 41 42 43 -- 45 46 47 |
                            | -- 51 52 53 -- 55 56 57 |
                            | -- 61 62 63 -- 65 66 67 |
                            | -- 71 72 73 -- 75 76 77 |
                            +-------------------------+
+-------------------------+
| 00 10 20 30 40 50 60 70 |
| 01 11 21 31 05 15 25 35 |
| 02 12 22 32 06 16 26 36 |
| 03 13 23 33 07 17 27 37 |
+-------------------------+
| 04 14 24 34 44 54 64 74 |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
| xx xx xx xx xx xx xx xx |
+-------------------------+
          Block B

Step 4: Repeat Step 3 on the next 3 lines of block B.
                                      Block A
                            +-------------------------+
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            +-------------------------+
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            | -- -- -- -- -- -- -- -- |
                            +-------------------------+
+-------------------------+
| 00 10 20 30 40 50 60 70 |
| 01 11 21 31 41 51 61 71 |
| 02 12 22 32 42 52 62 72 |
| 03 13 23 33 43 53 63 73 |
+-------------------------+
| 04 14 24 34 44 54 64 74 |
| 05 15 25 35 45 55 65 75 |
| 06 16 26 36 46 56 66 76 |
| 07 17 27 37 47 57 67 77 |
+-------------------------+
          Block B
```
