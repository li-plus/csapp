# Performance Lab Solution

Checkout the code in [kernels.c](perflab-handout/kernels.c).

Remember to set the baseline CPEs on your machine in [config.h](perflab-handout/config.h), otherwise the speedup results are invalid.

Final results on a `Intel(R) Xeon(R) Platinum 8260 CPU @ 2.40GHz`:
```
$ ./driver -t
Rotate: Version = naive_rotate: Naive baseline implementation:
Dim             64      128     256     512     1024    Mean
Your CPEs       2.2     3.0     5.3     8.9     13.0
Baseline CPEs   2.2     3.0     5.4     8.9     12.7
Speedup         1.0     1.0     1.0     1.0     1.0     1.0

Rotate: Version = rotate: Current working version:
Dim             64      128     256     512     1024    Mean
Your CPEs       2.3     2.4     2.4     3.4     5.2
Baseline CPEs   2.2     3.0     5.4     8.9     12.7
Speedup         0.9     1.2     2.3     2.7     2.4     1.8

Smooth: Version = smooth: Current working version:
Dim             32      64      128     256     512     Mean
Your CPEs       13.2    13.3    13.5    13.5    13.7
Baseline CPEs   54.3    54.2    54.2    54.8    54.8
Speedup         4.1     4.1     4.0     4.1     4.0     4.1

Smooth: Version = naive_smooth: Naive baseline implementation:
Dim             32      64      128     256     512     Mean
Your CPEs       54.4    54.2    54.2    54.9    55.2
Baseline CPEs   54.3    54.2    54.2    54.8    54.8
Speedup         1.0     1.0     1.0     1.0     1.0     1.0

Summary of Your Best Scores:
  Rotate: 1.8 (rotate: Current working version)
  Smooth: 4.1 (smooth: Current working version)
```
