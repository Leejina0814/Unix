test: smshell.o
	gcc -o test smshell.o

smshell.o : smallsh.h smshell.c
		gcc -c  smshell.c 
clean : rm *.o
