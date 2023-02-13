# Bomb Lab Solution

First disassemble the binary and dump the asm code.
```sh
objdump -d bomb > bomb.txt
```

Use gdb to step-run the program.
```
gdb --args bomb
(gdb) b explode_bomb  # set breakpoint at explode_bomb
(gdb) b phase_1       # set breakpoint at phase_1
(gdb) ctrl-x a        # switch to TUI mode
(gdb) layout asm      # show asm code
(gdb) layout regs     # register view
(gdb) r               # run
```

## Phase 1

Checkout the asm code first. The bomb explodes if the input (`$rdi`) doesn't equal to the string at `0x402400`.
```
0000000000400ee0 <phase_1>:
  400ee0:	48 83 ec 08          	sub    $0x8,%rsp
  400ee4:	be 00 24 40 00       	mov    $0x402400,%esi
  400ee9:	e8 4a 04 00 00       	callq  401338 <strings_not_equal>
  400eee:	85 c0                	test   %eax,%eax
  400ef0:	74 05                	je     400ef7 <phase_1+0x17>
  400ef2:	e8 43 05 00 00       	callq  40143a <explode_bomb>
  400ef7:	48 83 c4 08          	add    $0x8,%rsp
  400efb:	c3                   	retq   
```

Examine the string at `0x402400`. That is what we need to type.
```
(gdb) x/s 0x402400
0x402400:       "Border relations with Canada have never been better."
```

## Phase 2

Just randomly type something, and we come to the breakpoint in `read_six_numbers`.

In `read_six_numbers`, it uses `sscanf` to get 6 numbers from the input string.
```
000000000040145c <read_six_numbers>:
  ...
  40148a:	e8 61 f7 ff ff       	callq  400bf0 <__isoc99_sscanf@plt>
  ...
```

Checkout the `sscanf` format. We see that six numbers are separated by space.
```
(gdb) b *40148a
(gdb) x/s $esi
0x4025c3:       "%d %d %d %d %d %d"
```

Then we type 6 distinguishable numbers. Say `12345 23456 34567 45678 56789 67890`.

It failed at `400f10`.
```
0000000000400efc <phase_2>:
  ...
  400f0a:	83 3c 24 01          	cmpl   $0x1,(%rsp)
  400f0e:	74 20                	je     400f30 <phase_2+0x34>
  400f10:	e8 25 05 00 00       	callq  40143a <explode_bomb>
  ...
```

Let's see what's at `$rsp`.
```
(gdb) x/8wd $rsp
0x7fffffffdf60: 12345   23456   34567   45678
0x7fffffffdf70: 56789   67890   4199473 0
```

So we need the first number to be 1. Then try out `1 23456 34567 45678 56789 67890`, and failed at `400f20`.
```
0000000000400efc <phase_2>:
  ...
  400f1c:	39 03                	cmp    %eax,(%rbx)
  400f1e:	74 05                	je     400f25 <phase_2+0x29>
  400f20:	e8 15 05 00 00       	callq  40143a <explode_bomb>
  ...
```

Checkout the `$rbx` and `$eax`. So the second number should be `2`.
```
(gdb) x/d $rbx
0x7fffffffdf64: 23456
(gdb) p $eax
$1 = 2
```

After a couple of trials and errors, we get the correct answer `1 2 4 8 16 32`.

## Phase 3

This is a switch statement. Still doing trial and error. There are multiple possible solutions!

## Phase 4

Need to input 2 numbers. The first one (denoted as `x`) must be less than or equal to 0xe and make `func4(x, 0, 14)` return 0. The second one must be 0.

You may try from 0 to 14. Not too bad. 

Or we could dive into `func4` to see what happened. Reverse it into C code by hand. It is a recursive function. The easiest way to make it return zero is to let `(z-y)/2+y == x` in the first round. Given `y=0` and `z=14`, `x` should be 7.

```cpp
int func4(int x, int y, int z) {
  int c = (z - y) / 2 + y;
  if (c > x) {
    return 2 * func4(x, y, c - 1);
  } else if (c < x) {
    return 2 * func4(x, c + 1, z) + 1;
  } else {
    return 0;
  }
}
```

## Phase 5

Must type a 6-byte-long string `s`. A series of transformation is performed on this string, and the output `os` should equal to the target string. Let's reverse the asm to checkout the transformation.
```cpp
char os[6];
for (int i = 0; i < 6; i++) {
  os[i] = *((char*)0x4024b0 + (s[i] & 0xf));
}
if (strings_not_equal(os, (char*)0x40245e)) {
  explode_bomb();
}
```

Checkout these 2 strings.
```
(gdb) x/s 0x4024b0
0x4024b0 <array.3449>:  "maduiersnfotvbylSo you think you can stop the bomb with ctrl-c, do you?"
(gdb) x/s 0x40245e
0x40245e:       "flyers"
```

So we need to type a 6-byte string where the lower 4 bits of each character serve as the index of `maduiersnfotvbyl`, picking out 6 characters that form `flyers`. The input string should consist of visible characters. Just write a one-liner python to do this.

```python
>>> ''.join([chr('maduiersnfotvbyl'.index(x) + 0x40) for x in 'flyers'])
'IONEFG'
```

## Phase 6

It is getting more difficult! Need to reverse it with patience.
```cpp
unsigned nums[6];
read_six_numbers(input, nums);
// 40110b: ensure the input is a combination of 1-6
for (int i = 0; i < 6; i++) {
  if (nums[i] - 1 > 5) {
    explode_bomb();
  }
  for (int j = i + 1; j < 6; j++) {
    if (nums[i] == nums[j]) {
      explode_bomb();
    }
  }
}
// 401153: subtract each number by 7
for (int i = 0; i < 6; i++) {
  nums[i] = 7 - nums[i];
}
// 40116f: store next addresses in an array. loop reorganized.
struct node {
  int value;
  int index;
  struct node *next;
};
struct node *next_addrs[6];
struct node *p;
for (int i = 0; i < 6; i++) {
  p = &node1;
  for (int idx = 1; idx != nums[i]; idx++) {
    p = p->next;
  }
  next_addrs[i] = p;
}
// 4011ab: build a singly-linked list
p = next_addrs[0];
for (int i = 1; i < 6; i++) {
  p->next = next_addrs[i];
  p = p->next;
}
p->next = NULL;
// 4011da: ensure the list value is non-increasing
p = next_addrs[0];
for (int i = 5; i != 0; i--) {
  if (p->value < p->next->value) {
    explode_bomb();
  }
  p = p->next;
}

// data section: 0x6032d0
struct node node1 {0x14c, 1, &node2};
struct node node2 {0x0a8, 2, &node3};
struct node node3 {0x39c, 3, &node4};
struct node node4 {0x2b3, 4, &node5};
struct node node5 {0x1dd, 5, &node6};
struct node node6 {0x1bb, 6, NULL};
```

Type `1 2 3 4 5 6` to see what's going on. At `0x4011e9 explode_bomb`, examine the list node around `$rbx`. That is `node6 -> node5 -> node4 -> node3 -> node2 -> node1 -> NULL`, exactly in the given order after subtracted by 7. Sorted by their values, the correct order is `node3(0x39c) -> node4(0x2b3) -> node5(0x1dd) -> node6(0x1bb) -> node1(0x14c) -> node2(0xa8)`. Subtract them by 7, the correct input would be `4 3 2 1 6 5`.

```
(gdb) x/32wx $rbx-0x60
0x6032c0 <n48+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x6032d0 <node1>:       0x0000014c      0x00000001      0x00000000      0x00000000
0x6032e0 <node2>:       0x000000a8      0x00000002      0x006032d0      0x00000000
0x6032f0 <node3>:       0x0000039c      0x00000003      0x006032e0      0x00000000
0x603300 <node4>:       0x000002b3      0x00000004      0x006032f0      0x00000000
0x603310 <node5>:       0x000001dd      0x00000005      0x00603300      0x00000000
0x603320 <node6>:       0x000001bb      0x00000006      0x00603310      0x00000000
0x603330:               0x00000000      0x00000000      0x00000000      0x00000000
```

## Secret Phase

In the asm code we find a `secret_phase` that we haven't yet discovered. It is invoked in `phase_defused`. Let's set a breakpoint at `phase_defused`. Only if the last phase is defused, we come to an `sscanf`.
```
00000000004015c4 <phase_defused>:
  ...
  4015fa:	e8 f1 f5 ff ff       	callq  400bf0 <__isoc99_sscanf@plt>
  ...
```

The input string is equal to what we have typed in `phase_4`. If we add a string after the two numbers, we can go ahead.
```
(gdb) x/s $edi
0x603870 <input_strings+240>:   "7 0"
(gdb) x/s $esi
0x402619:       "%d %d %s"
```

Then we find that the string must equal to `DrEvil` at `0x402622`. Now we have discovered the `secret_phase`. It reads a number `x <= 1000` and calls `fun7(x)`. The return value is required to be 2.

Let's reverse `fun7` into the below C code. It acts like a binary node structure.
```cpp
struct node {
  long value;
  struct node *left;
  struct node *right;
  /* 8 bytes padding */
};

long fun7(struct node *n, long x) {
  if (n == NULL) {
    return -1;
  }
  if (n->value > x) {
    return 2 * fun7(n->left, x);
  } else if (n->value == x) {
    return 0;
  } else {
    return 2 * fun7(n->right, x) + 1;
  }
}
```

Set a breakpoint at `fun7`, and examine around `$rdi`.
```
(gdb) x/128wx $rdi
0x6030f0 <n1>:          0x00000024      0x00000000      0x00603110      0x00000000
0x603100 <n1+16>:       0x00603130      0x00000000      0x00000000      0x00000000
0x603110 <n21>:         0x00000008      0x00000000      0x00603190      0x00000000
0x603120 <n21+16>:      0x00603150      0x00000000      0x00000000      0x00000000
0x603130 <n22>:         0x00000032      0x00000000      0x00603170      0x00000000
0x603140 <n22+16>:      0x006031b0      0x00000000      0x00000000      0x00000000
0x603150 <n32>:         0x00000016      0x00000000      0x00603270      0x00000000
0x603160 <n32+16>:      0x00603230      0x00000000      0x00000000      0x00000000
0x603170 <n33>:         0x0000002d      0x00000000      0x006031d0      0x00000000
0x603180 <n33+16>:      0x00603290      0x00000000      0x00000000      0x00000000
0x603190 <n31>:         0x00000006      0x00000000      0x006031f0      0x00000000
0x6031a0 <n31+16>:      0x00603250      0x00000000      0x00000000      0x00000000
0x6031b0 <n34>:         0x0000006b      0x00000000      0x00603210      0x00000000
0x6031c0 <n34+16>:      0x006032b0      0x00000000      0x00000000      0x00000000
0x6031d0 <n45>:         0x00000028      0x00000000      0x00000000      0x00000000
0x6031e0 <n45+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x6031f0 <n41>:         0x00000001      0x00000000      0x00000000      0x00000000
0x603200 <n41+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x603210 <n47>:         0x00000063      0x00000000      0x00000000      0x00000000
0x603220 <n47+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x603230 <n44>:         0x00000023      0x00000000      0x00000000      0x00000000
0x603240 <n44+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x603250 <n42>:         0x00000007      0x00000000      0x00000000      0x00000000
0x603260 <n42+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x603270 <n43>:         0x00000014      0x00000000      0x00000000      0x00000000
0x603280 <n43+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x603290 <n46>:         0x0000002f      0x00000000      0x00000000      0x00000000
0x6032a0 <n46+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x6032b0 <n48>:         0x000003e9      0x00000000      0x00000000      0x00000000
0x6032c0 <n48+16>:      0x00000000      0x00000000      0x00000000      0x00000000
0x6032d0 <node1>:       0x0000014c      0x00000001      0x006032e0      0x00000000
0x6032e0 <node2>:       0x000000a8      0x00000002      0x00000000      0x00000000
```

It is a binary search tree! To make `fun7` produce 2, we need to input a value that exists in the tree, otherwise `fun7` returns -1 at the leaf node and the final result must be negative. Given 2 possible operations `2 * x` and `2 * x + 1`, we can let `2 = 2 * (2 * (0) + 1)`. That means we first take the left child and then the right child, and we come to n32(0x16). So type 22 (0x16) and the bomb is defused.

```
                   +-----------------n1(0x24)-----------------+
                   |                                          |
        +-------n21(0x8)------+                    +-------n22(0x32)------+
        |                     |                    |                      |
   +-n31(0x6)-+         +-n32(0x16)-+         +-n33(0x2d)-+         +-n34(0x6b)-+
   |          |         |           |         |           |         |           |
n41(0x1)   n42(0x7)  n43(0x14)  n44(0x23)  n45(0x28)  n46(0x2f)  n47(0x63)  n48(0x3e9)
```
