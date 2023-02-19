all: c1 c2 c3 r2 r3

c%: c%.txt
	./target1/hex2raw < $< | ./target1/ctarget -q

r%: r%.txt
	./target1/hex2raw < $< | ./target1/rtarget -q
