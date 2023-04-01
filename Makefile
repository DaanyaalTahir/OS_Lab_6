all:
	clang -g -Wall -Wextra -o main.out main.c tlb.c

clean:
	rm -f *.out main 