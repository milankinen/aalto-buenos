/* Shell
 * Userland program to run other programs interactively
 */

#include "tests/lib.h"
#include "tests/str.h"

#define SHELL_LINELEN 512
#define SHELL_MAXEXECSZ 128
#define SHELL_ARGLEN 16
#define SHELL_MAXARGS 32
#define SHELL_DISK "[disk1]"
#define SHELL_NAME "[disk1]shell"
#define SHELL_CLEAR_LINES 150

#define SHELL_DEL_CHAR 127

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
    char *argv[SHELL_MAXARGS];
    /* _not_ null ended array! this should only be accessed from argv
     * each string in arga[something] _is_ however null ended
     * this is used for strings and argv is used to feed execp
     */
    char arga[SHELL_LINELEN];
} shell_cmd;

/* data structure to hold shell status
 * error to indicate if line reading was successful
 * foreground to indicate if a process was started on foreground
 *  background
 */
typedef enum error_e {
    OK,
    READFAIL,
    LINELENFAIL,
    EXECPFAIL,
    ARGNFAIL,
    FATAL,
    SYNTAXFAIL,
    MAXEXECSZFAIL
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
    memoryset(cmd->arga, 0, SHELL_LINELEN);
    for (i = 0; i < SHELL_ARGLEN; i++)
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

/**
 * NOTE this does not notify arrows, we are not supporting arrow navigation
 */
int shell_is_ctrl_char(char ch) {
    return ch < 32 || ch > 126;
}

void shell_remove_control_chars(char* str) {
    int i, actual;
    char temp[SHELL_LINELEN];
    actual = 0;
    for (i = 0 ; str[i] != '\0' ; i++) {
        if (!shell_is_ctrl_char(str[i])) {
            temp[actual++] = str[i];
        }
    }
    temp[actual] = '\0';
    memcopy(actual + 1, str, temp);
}

void shell_remove_char_from_cin() {
    char buf[4];
    /* this sequence deletes one character fron console */
    buf[0] = 27;
    buf[1] = 91;
    buf[2] = 68;
    buf[3] = ' ';
    syscall_write(stdout, buf, 4);
    syscall_write(stdout, buf, 3);
}

int shell_readchar(char* buf, int offset) {
    int read;
    while (1) {
        read = syscall_read(stdin, buf + offset, sizeof(char));
        if (read <= 0) return read;
        /* disable arrows */
        if ((int)buf[offset] == 27) {
            /* next two chars are for control.. ignore those */
            syscall_read(stdin, buf + offset, sizeof(char));
            syscall_read(stdin, buf + offset, sizeof(char));
        } else {
            /* proper char readed! */
            break;
        }
    }

    return read;
}

/* readline: reads line character by character until line break
 * returns error code or OK depending on read result
 */
int
shell_readline(char *buf, const int size) {
    int read, i;
    i = 0;

    /* note: if read == 0 while loop is not executed any more
     * this means end of file was encountered
     */
    while ((read = shell_readchar(buf, i))) {
        /* if read fails */
        if (read < 0) {
            return READFAIL;
        }

        if (buf[i] == '\n' || buf[i] == '\r') {
            cout("\n");
            i++;
            break;
        }

        if (buf[i] == SHELL_DEL_CHAR) {
            if (i > 0) {
                shell_remove_char_from_cin();
                i--;
            }
            continue;
        }

        /* echo the written character */
        cout("%c", buf[i]);
        /* update buffer to new posisition to get new character
         * to its place
         */
        i++;
        if (i >= size - 1) {
            return LINELENFAIL;
        }
    }
    /* we want the line to be a c string */
    buf[i] = '\0';
    shell_remove_control_chars(buf);
    //cout("GOT: '%s'\n", buf);
    //for (i = 0 ; buf[i] != '\0' ; i++) cout("%d ", buf[i]);
    return OK;
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
    char *next_arg;
    int res, parsing_arg, i, argc;

    shell_init(cmd, status);
    res = shell_readline(cmd->arga, SHELL_LINELEN);
    if (res != OK) {
        status->error = res;
        return;
    }

    i = 0;
    parsing_arg = 0;
    argc = 0;
    next_arg = NULL;
    /* we will find ending \0 because shell_readline returned OK */
    while (cmd->arga[i] != '\0') {
        if (!parsing_arg) {
            if (!shell_iswhitespace(cmd->arga[i])) {
                next_arg = cmd->arga + i;
                parsing_arg = 1;
            }
        } else {
            if (shell_iswhitespace(cmd->arga[i])) {
                if (next_arg) {
                    cmd->argv[argc++] = next_arg;
                    /* replace whitespace with \0 so that we can use same buffer as data */
                    cmd->arga[i] = '\0';
                    if (argc >= SHELL_MAXARGS) {
                        /* too many arguments */
                        status->error = ARGNFAIL;
                        return;
                    }
                }
                next_arg = NULL;
                parsing_arg = 0;
            }
        }
        i++;
    }
    /* put our last (or first) argumentment to list */
    if (next_arg != NULL) {
        cmd->argv[argc++] = next_arg;
    }

    /* Determine foreground status, default = executing foreground */
    status->foreground = 1;
    if (argc > 0) {
        if (stringcmp("&", cmd->argv[argc-1]) == 0) {
            if (argc == 1) {
                status->error = SYNTAXFAIL;
                return;
            }
            status->foreground = 0;
            argc--;
        }
    }
    /* inform argument num */
    cmd->argc = argc;
    status->error = OK;
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

/* execute: call execp and join process
 */
void
shell_execute(shell_cmd *cmd, shell_status *status) {
    char filename[SHELL_MAXEXECSZ];
    int pid, retval;
    if (status->foreground) {
        if (strlen(cmd->argv[0]) + strlen(SHELL_DISK) >= SHELL_MAXEXECSZ) {
            status->error = MAXEXECSZFAIL;
            return;
        }
        snprintf(filename, SHELL_MAXEXECSZ, "%s%s", SHELL_DISK, cmd->argv[0]);
        /* execute original program and join immediately */
        pid = syscall_execp(filename, cmd->argc - 1, (const char**)(cmd->argv + 1));
        if (pid < 0) {
            status->error = EXECPFAIL;
            return;
        }

        retval = syscall_join(pid);
        cout("Done, got: %d\n", retval);
    } else {
        /* execute shell program with arguments */
        pid = syscall_execp(SHELL_NAME, cmd->argc, (const char**)cmd->argv);
        if (pid < 0) {
            status->error = EXECPFAIL;
            return;
        }
        /* join shell instance (process space management deals that process table don't run out..
         * shell starts a new child process but don't join so actual process remains in background
         * shell returns the pid of created child process
         */
        retval = syscall_join(pid);
        cout("[%d]\n", retval);
    }
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

/**
 * returns 1 if user requests exit from shell (types "exit")
 */
int shell_request_exit(shell_cmd* cmd, shell_status* status) {
    if (status->error == OK && cmd->argc > 0) {
        if (stringcmp("exit", cmd->argv[0]) == 0) {
            /* if we have one argument then it is ok. */
            if (cmd->argc == 1) return 1;
            /* otherwise we have a syntax error */
            status->error = SYNTAXFAIL;
        }
    }
    return 0;
}

/** implement clear inside shell because we have to print return value.. so we cannot
 * create new executable becuase its return value would ruin the clearing..
 */
int shell_request_clear(shell_cmd* cmd, shell_status* status) {
    int i;
    if (status->error == OK && cmd->argc > 0) {
        if (stringcmp("clear", cmd->argv[0]) == 0) {
            /* if we have one argument then it is ok. */
            if (cmd->argc == 1) {
                for (i = 0 ; i < SHELL_CLEAR_LINES ; i++) cout("\n");
                return 1;
            }
            /* otherwise we have a syntax error */
            status->error = SYNTAXFAIL;
        }
    }
    return 0;
}


int shell_start_background_proc(int argc, char** argv) {
    char filename[SHELL_MAXEXECSZ];
    if (strlen(argv[1]) + strlen(SHELL_DISK) >= SHELL_MAXEXECSZ) {
        return -1;
    }
    snprintf(filename, SHELL_MAXEXECSZ, "%s%s", SHELL_DISK, argv[1]);
    return syscall_execp(filename, argc - 2, (const char **)(argv + 2));
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
        case LINELENFAIL:
            shell_print_str("Error: too long line.\n");
            /* non fatal error, return ok state */
            status->error = OK;
            break;
        case EXECPFAIL:
            shell_print_str("Error: execution failed.\n");
            /* non fatal error, return ok state */
            status->error = OK;
            break;
        case SYNTAXFAIL:
            shell_print_str("Error: syntax error.\n");
            /* non fatal error, return ok state */
            status->error = OK;
            break;
        case MAXEXECSZFAIL:
            shell_print_str("Error: too long executable name.\n");
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
main(int argc, char** argv) {
    shell_cmd cmd_s;
    shell_status status_s;
    /* since parse function modifies cmd_s and status_s 
     * and we do not want to pass cmd_s by value,
     * we use passing by reference
     */
    shell_cmd *cmd = &cmd_s;
    shell_status *status = &status_s;

    /* if we have arguments then it means that shell is meant to run in background
     * execp a new process without joining and return pid.
     */
    if (argc > 1) {
        return shell_start_background_proc(argc, argv);
    }


    shell_init(cmd, status);
    while (1) {
        shell_prompt("> ");
        shell_parse(cmd,status);
        if (shell_request_exit(cmd, status)) {
            /** User requests exit. */
            return 0;
        }
        if (shell_request_clear(cmd, status)) {
            continue;
        }

        if (status->error != OK) {
            /* this if is at the moment needed to
             * continue the while loop from the beginning
             */
            shell_handle_error(status);
            continue;
        }

        if (shell_stop(cmd, status))
            break;
        if (cmd->argc > 0) {
            shell_execute(cmd, status);
            if (status->error != OK) {
                shell_handle_error(status);
                continue;
            }
        }
    }
    return 0;
}
