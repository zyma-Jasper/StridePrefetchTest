modprobe msr
wrmsr 0x1a4 -p0  0x7
rdmsr 0x1a4 -p0
gcc -D_GNU_SOURCE test.c -o ttt
taskset 0x1 ./ttt
