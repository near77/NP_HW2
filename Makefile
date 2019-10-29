all:
	gcc -c shell.c
	gcc shell.o -o shell
g++:
	g++ -o npshell npshell.cpp