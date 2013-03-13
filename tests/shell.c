/* Shell
 * Userland program to run other programs interactively
 */

#include "tests/lib.h"
#include "tests/str.h"

#define SHELL_LINELEN 128
#define SHELL_ARGLEN 16
#define SHELL_MAXARGS 16

/* +2 to accommodate program name and null
 * +1 to accommodate null
 */
#define SHELL_ARGUMENT_ARRAY_LEN    SHELL_MAXARGS+2
#define SHELL_ARGUMENT_STRING_LEN   SHELL_ARGLEN+1

/* data structure to hold the command and its arguments in c like format
 */
typedef struct {
    /* number of arguments */
    int argc;
    /* pointers for the 2-dim array below
     * this is null ended
     * used as a wrapper for arga to give smoother typecast
    */
    char *argv[SHELL_ARGUMENT_ARRAY_LEN];
    /* _not_ null ended array! this should only be accessed from argv
     * each string in arga[something] _is_ however null ended
     * this is used for strings and argv is used to feed execp
     */
    char arga[SHELL_ARGUMENT_ARRAY_LEN][SHELL_ARGUMENT_STRING_LEN];
} shell_cmd;

/* data structure to hold shell status
 * error to indicate if line reading was successful
 * foreground to indicate if a process was started on foreground
 *  background
 */
typedef enum error_e {
    OK,
    READFAIL,
    EXECPFAIL,
    ARGNFAIL,
    ARGLENFAIL,
    FATAL
} error_t;

typedef struct {
    error_t error;
    int foreground;
} shell_status;

/* auxiliary functions */

void
shell_init(shell_cmd *cmd, shell_status *status) {
    /* set argv to correspond to the 2-dim array arga */
    int i;
    for (i = 0; i < SHELL_ARGUMENT_ARRAY_LEN-1; i++)
        cmd->argv[i] = cmd->arga[i];
    /* null ending */
    cmd->argv[i] = 0;
    /* no errors yet */
    status->error = OK;
}

/* print_str: print c string */
void
shell_print_str(const char *str) {
    syscall_write(stdout, str, strlen(str));
}

/* prompt: write something for user to give input */
void
shell_prompt(const char *str) {
    shell_print_str(str);
}

/* readline: reads line character by character until line break
 * returns pointer to the end of the read buffer i.e. the \0 character
 * in error situation 0 is returned if only end of line is read the
 * same address buf is returned
 */
char *
shell_readline(char *buf) {
    int read;
    while ((read = syscall_read(stdin, buf,sizeof(char)))) {
        /* if read fails */
        if (read < 0) {
            return 0;
        }
        /* note: if read == 0 while loop is not executed any more
         * this means end of file was encountered
         */
        /* if we read \n stop */
        else if (*buf == '\n') {
            break;
        }
        /* echo the written character */
        syscall_write(stdout, buf, sizeof(char));
        /* update buffer to new posisition to get new character
         * to its place
         */
        buf++;
    }
    /* we want the line to be a c string */
    *buf = '\0';
    return buf;
}

int
shell_iswhitespace(char c) {
    if (c == ' ' || c == '\n' || c == '\t' || c == '\0')
        return 1;
    else
        return 0;
}

/* next whitespace returns pointer to the next whitespace character
 * in a string until end ptr
 * if end is encountered return it
 */
char *
shell_next_whitespace(char *str, char *end) {
    while (!shell_iswhitespace(*str) && (str < end))
        str++;
    return str;
}

/* next non-white returns pointer to the next non-white space
 * character in a string until end
 * if end is encountered return it
 */
char *
shell_next_nonwhite(char *str, char *end) {
    while (shell_iswhitespace(*str) && (str < end))
        str++;
    return str;
}

/* parse: reads line with shell_readline and builds
 * shell_cmd and shell_status data structures
 * - sets shell_cmd null if end of file was the only thing read
 * - sets the fields of shell_cmd null if no command was read
 * - sets status foreground field
 */
void
shell_parse(shell_cmd *cmd, shell_status *status) {
    char cmdln[SHELL_LINELEN];
    char *end = shell_readline(cmdln);
    char *next_white, *next_nonwhite;
    /* check end for errors */
    if (end == 0) {
        /* read failed */
        status->error = READFAIL;
        return;
    }
    /* error is left OK since it was initialized in shell_init */
    /* check if EOF was encountered */
    if (cmdln-end == 0) {
        cmd = 0;
        return;
    }
    /* partition cmdln to c strigs separated by whitespace and
     * assign them to shell_cmd data structure
     *
     * start by entering arguments in their positions
     */
    int i,j;
    next_nonwhite = shell_next_nonwhite(cmdln, end);
    for (i = 0; i < SHELL_ARGUMENT_ARRAY_LEN; i++) {
        next_white = shell_next_whitespace(next_nonwhite, end);
        /* copy the string between next_nonwhite and next_white
         * to its slot in the shell_cmd if arg is too long
         * update error field in status
         */
        for (j = 0; next_nonwhite + j < next_white; j++) {
            if (j >= SHELL_ARGLEN) {
                status->error = ARGLENFAIL;
                cmd->arga[i][j-1] = '\0';
                goto break_both_loops;
            }
            cmd->arga[i][j] = next_nonwhite[j];
        }
        /* make the string null ended
         * there is room since even if now j = SHELL_ARGLEN
         * the array was designed to hold one more
         */
        cmd->arga[i][j] = '\0';
        next_nonwhite = shell_next_nonwhite(next_white, end);
        if (next_nonwhite == end)
            break;
    }
break_both_loops:
    /* check if all array was used
     * this should not happen if there was the allowed number
     * of arguments
     */
    if (i == SHELL_ARGUMENT_ARRAY_LEN - 1) {
        /* i.e. no room for null which is added _after_ the
         * last position which is SHELL_ARGUMENT_ARRAY_LEN - 1 
         */
        status->error = ARGNFAIL;
        /* the array is not null ended any more
         * correct that for robustness
         */
        cmd->argv[i] = 0;
        return;
    }
    /* if the last argument is &
     * set background
     */
    if (*cmd->argv[i] == '&') {
        status->foreground = 0;
    }
    else {
        status->foreground = 1;
        i++; /* see argc below */
    }
    /* now all arguments are in place 
     * make the array null ended and update argc
     */
    cmd->argv[i] = 0;
    /* argc is the number of arguments plus the program name
     * thus argv[i] is not the last argument but null
     * we must add the one above to the i
     */
    cmd->argc = i;
    return;
}

/* stop: return true when the command is exit or EOF
 */
int
shell_stop(shell_cmd *cmd, shell_status *status) {
    /* cmd 0 if only end of file was read
     * this is when we stop
     */
    if (cmd && (status->error != FATAL))
        return 0;
    else
        return 1;
}

/* execute: call execp and return pid of the created process
 */
int
shell_execute(shell_cmd *cmd, shell_status *status) {
    int pid = syscall_execp(cmd->argv[0], cmd->argc, (const char **)cmd->argv);
    if (pid < 0)
        status->error = EXECPFAIL;
    return pid;
}

/* foreground: return true if the shell was started in foreground
 * & at the end of the command indicates background execution
 */
int
shell_foreground(shell_status *s) {
    return s->foreground;
}

/* print an int
 */
void
shell_print_int(int pid) {
    /* str to fit any int */
    char pidstr[16];
    snprintf(pidstr,16,"%d\n",pid);
    shell_print_str(pidstr);
}

void
shell_handle_error(shell_status *status) {
    switch(status->error) {
        case OK:
            /* this will not be executed by the current code
             * but is added to include all possible cases
             * for error_t (can't cause compiler warning)
             */
            break;
        case READFAIL:
            shell_print_str("Error: syscall_read failed.\n");
            /* shut down the shell in the next phase */
            status->error = FATAL;
            break;
        case ARGNFAIL:
            shell_print_str("Error: too many arguments.\n");
            /* non fatal error, return ok state */
            status->error = OK;
            break;
        case ARGLENFAIL:
            shell_print_str("Error: too long arguments.\n");
            /* non fatal error, return ok state */
            status->error = OK;
            break;
        case EXECPFAIL:
            shell_print_str("Error: execution failed.\n");
            /* non fatal error, return ok state */
            status->error = OK;
            break;
        case FATAL:
            /* is not handled here but shell stop will
             * shutdown the shell later
             */
            break;
    }
}

int
main(void) {
    int pid;
    shell_cmd cmd_s;
    shell_status status_s;
    /* since parse function modifies cmd_s and status_s 
     * and we do not want to pass cmd_s by value,
     * we use passing by reference
     */
    shell_cmd *cmd = &cmd_s;
    shell_status *status = &status_s;
    shell_init(cmd, status);
    while (1) {
        shell_prompt("> ");
        shell_parse(cmd,status);
        if (status->error != OK) {
            /* this if is at the moment needed to
             * continue the while loop from the beginning
             */
            shell_handle_error(status);
            continue;
        }
        if (shell_stop(cmd, status))
            break;
        pid = shell_execute(cmd, status);
        if (status->error != OK) {
            shell_handle_error(status);
            continue;
        }
        if (shell_foreground(status)) {
            syscall_join(pid);
        }
        else {
            shell_print_int(pid);
        }
    }
    return 0;
}
