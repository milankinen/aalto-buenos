/* Shell
 * Userland program to run other programs interactively
 */

#include "tests/lib.h"
#include "lib/libc.h"

#define SHELL_LINELEN 128
#define SHELL_ARGLEN 16
#define SHELL_MAXARGS 16

/* data structure to hold the command and its arguments in c like format
 */
typedef struct {
    /* number of arguments */
    int c;
    /* null ended strings in null ended array
     * +2 to accommodate program name and null
     * +1 to accommodate null
     */
    char v[SHELL_MAXARGS+2][SHELL_ARGLEN+1];
} shell_cmd;

/* data structure to hold shell status
 * error to indicate if line reading was successful
 * foreground to indicate if a process was started on foreground
 *  background
 */
typedef struct {
    int error;
    int foreground;
} shell_status;

/* auxiliary functions */

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
 * in error situation 0 is returned
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
        /* update buffer to new posisition to get new character
         * to its place
         */
        buf++;
    }
    /* we want the line to be a c string */
    *buf = '\0';
    return buf;
}

/* parse: reads line with shell_readline and builds
 * shell_cmd and shell_status data structures
 */
void
shell_parse(shell_cmd *cmd, shell_status *status) {
    char cmdln[SHELL_LINELEN];
    char *end;
    end = shell_readline(cmdln);
    /* partition cmdln to c strigs separated by whitespace and
     * assign them to shell_cmd data structure
     */
    /* TODO, remember to move the following :) */
    end = end;
    cmd = cmd;
    status = status;
}

/* stop: return true when the command is exit or EOF
 */
int
shell_stop(shell_cmd *cmd) {
    /* TODO */
    cmd = cmd;
    return 1;
}

/* execute: call execp and return pid of the created process
 */
int
shell_execute(shell_cmd *cmd) {
    /* TODO */
    cmd = cmd;
    return 0;
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
    while (1) {
        shell_prompt("> ");
        shell_parse(cmd,status);
        /* TODO: error handling of read line fail */
        if (shell_stop(cmd))
            break;
        pid = shell_execute(cmd);
        /* TODO: error handling of execp fail */
        if (shell_foreground(status)) {
            syscall_join(pid);
        }
        else {
            shell_print_int(pid);
        }
    }
    return 0;
}
