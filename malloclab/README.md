# Malloc Lab Solution

Traces are taken from [jon-whit/malloc-lab](https://github.com/jon-whit/malloc-lab).

Remember to set the `AVG_LIBC_THRUPUT` on your machine in [config.h](malloclab-handout/config.h), otherwise the throughput score is invalid. The libc malloc throughput on my machine (Intel(R) Xeon(R) Platinum 8260 CPU @ 2.40GHz) is roughly 9000 Kops/sec.

Final results:
```
$ ./mdriver -v -t traces -l
Results for libc malloc:
trace  valid  util     ops      secs  Kops
 0       yes    0%    5694  0.000954  5969
 1       yes    0%    5848  0.000794  7369
 2       yes    0%    6648  0.001383  4809
 3       yes    0%    5380  0.001421  3786
 4       yes    0%   14400  0.000310 46392
 5       yes    0%    4800  0.002488  1929
 6       yes    0%    4800  0.002378  2019
 7       yes    0%   12000  0.001025 11705
 8       yes    0%   24000  0.000898 26726
 9       yes    0%   14401  0.000492 29270
10       yes    0%   14401  0.000245 58780
Total           0%  112372  0.012388  9071

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000202 28258
 1       yes  100%    5848  0.000211 27755
 2       yes   99%    6648  0.000240 27688
 3       yes  100%    5380  0.000191 28227
 4       yes  100%   14400  0.000256 56338
 5       yes   96%    4800  0.000968  4960
 6       yes   94%    4800  0.000939  5113
 7       yes   55%   12000  0.000328 36630
 8       yes   51%   24000  0.000659 36408
 9       yes  100%   14401  0.000294 48950
10       yes   87%   14401  0.000282 51085
Total          89%  112372  0.004568 24600

Perf index = 54 (util) + 40 (thru) = 94/100
```

Roadmaps:
|                       | First Fit                      | Best Fit                       |
|-----------------------|--------------------------------|--------------------------------|
| Implicit Free Lists   | 53 (util) + 1 (thru) = 54/100  | 54 (util) + 1 (thru) = 55/100  |
| Explicit Free Lists   | 51 (util) + 6 (thru) = 57/100  | 54 (util) + 5 (thru) = 59/100  |
| Segregated Free Lists | 52 (util) + 40 (thru) = 92/100 | 54 (util) + 40 (thru) = 94/100 |
