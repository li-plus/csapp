# Shell Lab Solution

Tips:
* Some shared variables (jobs/susp) are modified within both signal handlers and the main routine, creating a concurrent situation that we need to handle correctly.
* Block signals before accessing shared data, and must restore the signal mask when finished, just like locking and unlocking a mutex.
* Do not wait a child process twice. Suspend in `waitfg` and do all the `waitpid`s in `sigchld_handler`.
* Make sure to do error checking for all system calls.

To test all the cases, run the below command and compare the two output files.
```sh
make $(seq -s " " -f test%02g 1 16) > test.txt
make $(seq -s " " -f rtest%02g 1 16) > rtest.txt
```
