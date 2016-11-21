#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define TRUE 1
#define FALSE 0

// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;      // number of arguments
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none
	int type;	   // command type (i.e. background or not)
	int ctype;	   // condition type (and / or)
	int tag;	   // keeping track of order for debugging
	command* next; // next command in list
	command* prev; // prev command in list
	command* up;   // yay conditionals
	command* down; // more conditionals yay
	int cont;	   // indicator to wait or continue
};

void run_vert(command* c);

command* head;
//command* tail;

// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    c->pid = -1;
	c->tag = 0;
    return c;
}

// command_add(c)
//    Add next command to list.
void command_add(command* c, int type) {
	if (type == TOKEN_BACKGROUND || type == TOKEN_SEQUENCE) {
		//command* cnext = command_alloc();
		c->next = command_alloc();
		c->next->tag = c->tag+1;
		c->next->prev = c;
	}
	else if ((type == TOKEN_AND) || (type == TOKEN_OR)) {
		//printf("we're going up!");
		//command* ccond = command_alloc();
		c->up = command_alloc();
		c->up->type = c->type; // background affects whole conditional
		c->up->down = c;
	}
}

// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
	command* tmp = c;
	while (tmp){
		c = tmp;
    	for (int i = 0; i != c->argc; ++i)
        	free(c->argv[i]);
    	free(c->argv);
		tmp = c->next;
    	free(c);
	}
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND EVALUATION

// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t start_command(command* c, pid_t pgid) {
    (void) pgid;
    // Your code here!
	pid_t pidc = fork();

	// child process or fork error
	if (pidc <= 0) {
		// child process
		if (pidc == 0)
			execvp(c->argv[0],c->argv);
		_exit(1);
		return c->pid;
	}
	// parent process: update pid
	c->pid = pidc;
    //fprintf(stderr, "start_command not done yet\n");
    return c->pid;
}


// run_list(c)
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `set_foreground(pgid)` before waiting for the pipeline.
//       - Call `set_foreground(0)` once the pipeline is complete.
//       - Cancel the list when you detect interruption.

void run_list(command* c) {
	while (c) {
		if (c->type == TOKEN_BACKGROUND) {
			pid_t f = fork();
			if (f == 0) {
				run_vert(c);
				_exit(1);
			}
			else if (f > 0) {
				c = c->next;
				continue;
			}
			else if (f == -1) {
				_exit(1);
			}
		}
		else {
			//run_vert(c);
			
			if (c->up) {
				run_vert(c);
			}
			else {
				int status;
				pid_t pidc = start_command(c, 0);
				waitpid(pidc, &status, 0);
			}
			
			c = c->next;
		}
	}
    //fprintf(stderr, "run_command not done yet\n");
}

void run_vert(command* c) {
	int status;
	while (c) {
		/*
		if ((status != 0 && c->ctype == TOKEN_OR) ||
			(status == 0 && c->ctype == TOKEN_AND)) {
			pid_t pc = start_command(c, 0);
			waitpid(pc, &status, 0);
			if (WIFEXITED(status)) {
				status = WEXITSTATUS(status);
			}
		}
		else if ((status != 0 && c->ctype == TOKEN_AND) ||
			(status == 0 && c->ctype == TOKEN_OR)) {
			_exit(status);
			break;
		}
		*/
		pid_t pc = start_command(c, 0);
		waitpid(pc, &status, 0);	
		if (WIFEXITED(status)) {
			status = WEXITSTATUS(status);
			if ((status != 0 && c->ctype == TOKEN_AND) || 
				(status == 0 && c->ctype == TOKEN_OR)) {
				//status = WEXITSTATUS(status);
				_exit(status);
				//break;
			}
			else {
				c = c->up;
				continue;
			}	
		}
		c = c->up;
	}
}


// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

void eval_line(const char* s) {
    int type;
    char* token;
    // Your code here!

	// build the command
	// initialize "head"
    //command* c = command_alloc();
	head = command_alloc();
	// cursor used for traversing & building
	command* curr = head;
	command* top = head;
    while ((s = parse_shell_token(s, &type, &token)) != NULL) {

		// normal token
		//curr->type = type;
		//if (type == TOKEN_NORMAL) {
		//	command_append_arg(curr, token);
		//}
		if (type == TOKEN_AND || type == TOKEN_OR) {
			curr->up = command_alloc();
			curr->ctype = type;
			curr = curr->up;
		}
		else if (type == TOKEN_BACKGROUND || type == TOKEN_SEQUENCE) {
			top->next = command_alloc();
			if (type == TOKEN_BACKGROUND) {			
				top->type = type;
			}
			//command_add(curr, type);
			curr = top->next;
			top = curr;
		}
		else {
			command_append_arg(curr, token);
		}
	}
    // execute it
	if (head->argc)
		run_list(head);
    command_free(head);
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    set_foreground(0);
    handle_signal(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file))
                    perror("sh61");
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
    }

    return 0;
}
