/*
 * minishell_solution.c - minishell 과제 모범 답안 (교수용)
 *
 * Compile: gcc -Wall -o minishell minishell_solution.c
 * Run:     ./minishell
 *
 * Supported features:
 *   - Single command execution: ls -l
 *   - Pipe: cmd1 | cmd2
 *   - Input redirection: cmd < file
 *   - Output redirection: cmd > file
 *   - Exit: exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LINE    1024
#define MAX_ARGS    64

struct command {
    char *argv[MAX_ARGS];
    char *infile;
    char *outfile;
};

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '\0')
        return s;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n'))
        *end-- = '\0';
    return s;
}

static int parse_command(char *line, struct command *cmd)
{
    memset(cmd, 0, sizeof(*cmd));

    int argc = 0;
    char *token = strtok(line, " \t");

    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t");
            if (token == NULL) {
                fprintf(stderr, "minishell: syntax error near '<'\n");
                return -1;
            }
            cmd->infile = token;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t");
            if (token == NULL) {
                fprintf(stderr, "minishell: syntax error near '>'\n");
                return -1;
            }
            cmd->outfile = token;
        } else {
            if (argc >= MAX_ARGS - 1) {
                fprintf(stderr, "minishell: too many arguments\n");
                return -1;
            }
            cmd->argv[argc++] = token;
        }
        token = strtok(NULL, " \t");
    }

    cmd->argv[argc] = NULL;

    if (argc == 0)
        return -1;

    return 0;
}

/*
 * Set up redirections for a command in the child process.
 * Returns 0 on success, -1 on failure (after printing error).
 */
static int setup_redirections(struct command *cmd)
{
    if (cmd->infile != NULL) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "minishell: %s: %s\n", cmd->infile, strerror(errno));
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->outfile != NULL) {
        int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "minishell: %s: %s\n", cmd->outfile, strerror(errno));
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    return 0;
}

static int execute_command(struct command *cmd)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("minishell: fork");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        if (setup_redirections(cmd) < 0)
            exit(1);

        execvp(cmd->argv[0], cmd->argv);
        fprintf(stderr, "minishell: %s: %s\n", cmd->argv[0], strerror(errno));
        exit(127);
    }

    /* Parent process */
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("minishell: waitpid");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

static int execute_pipe(struct command *cmd1, struct command *cmd2)
{
    int fd[2];
    if (pipe(fd) < 0) {
        perror("minishell: pipe");
        return -1;
    }

    /* Child 1: runs cmd1, stdout → pipe write end */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("minishell: fork");
        close(fd[0]);
        close(fd[1]);
        return -1;
    }

    if (pid1 == 0) {
        close(fd[0]);  /* close read end */
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);

        if (setup_redirections(cmd1) < 0)
            exit(1);

        execvp(cmd1->argv[0], cmd1->argv);
        fprintf(stderr, "minishell: %s: %s\n", cmd1->argv[0], strerror(errno));
        exit(127);
    }

    /* Child 2: runs cmd2, stdin ← pipe read end */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("minishell: fork");
        close(fd[0]);
        close(fd[1]);
        /* Kill child 1 since we can't proceed */
        waitpid(pid1, NULL, 0);
        return -1;
    }

    if (pid2 == 0) {
        close(fd[1]);  /* close write end */
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);

        if (setup_redirections(cmd2) < 0)
            exit(1);

        execvp(cmd2->argv[0], cmd2->argv);
        fprintf(stderr, "minishell: %s: %s\n", cmd2->argv[0], strerror(errno));
        exit(127);
    }

    /* Parent: close both pipe ends and wait for both children */
    close(fd[0]);
    close(fd[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    if (WIFEXITED(status2))
        return WEXITSTATUS(status2);
    return -1;
}

int main(void)
{
    char line[MAX_LINE];

    while (1) {
        printf("minishell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        char *input = trim(line);
        if (*input == '\0')
            continue;

        if (strcmp(input, "exit") == 0)
            break;

        char *pipe_pos = strchr(input, '|');

        if (pipe_pos != NULL) {
            *pipe_pos = '\0';
            char *left = input;
            char *right = pipe_pos + 1;

            char left_copy[MAX_LINE], right_copy[MAX_LINE];
            strncpy(left_copy, left, sizeof(left_copy) - 1);
            left_copy[sizeof(left_copy) - 1] = '\0';
            strncpy(right_copy, right, sizeof(right_copy) - 1);
            right_copy[sizeof(right_copy) - 1] = '\0';

            struct command cmd1, cmd2;
            if (parse_command(left_copy, &cmd1) < 0)
                continue;
            if (parse_command(right_copy, &cmd2) < 0)
                continue;

            execute_pipe(&cmd1, &cmd2);
        } else {
            char copy[MAX_LINE];
            strncpy(copy, input, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';

            struct command cmd;
            if (parse_command(copy, &cmd) < 0)
                continue;

            execute_command(&cmd);
        }
    }

    return 0;
}
