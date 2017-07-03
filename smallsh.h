#define EOL 1
#define ARG 2
#define AMPERSAND 3
#define SEMICOLON 4
#define INTERRUPT 5

#define MAXARG 512
#define MAXBUF 512

#define FOREGROUND 0
#define BACKGROUND 1

int userin(char *p);
void procline();
int gettok(char **p);
int runcommand(char **cline, int where);
int inarg(char c);
