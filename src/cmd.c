// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	int status = chdir(get_word(dir));

	return status;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	exit(SHELL_EXIT);

	 /* TODO: Replace with actual exit code. */
}

// function for handling redirections
void parse_redirection(simple_command_t *s)
{
	char *in = get_word(s->in), *out = get_word(s->out), *err = get_word(s->err);

	if (in != NULL) {
		int fd = open(in, O_RDONLY);

		dup2(fd, 0);
		close(fd);
	}

	if (out != NULL && err != NULL && strcmp(out, err) == 0) {
		int fd;

		if (s->io_flags == IO_OUT_APPEND)
			fd = open(out, O_WRONLY | O_CREAT | O_APPEND, 0644);
		else
			fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);

		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
	} else {
		if (out != NULL) {
			int fd;

			if (s->io_flags == IO_OUT_APPEND)
				fd = open(out, O_WRONLY | O_CREAT | O_APPEND, 0644);
			else
				fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			dup2(fd, 1);
			close(fd);
		}

		if (err != NULL) {
			int fd;

			if (s->io_flags == IO_ERR_APPEND)
				fd = open(err, O_WRONLY | O_CREAT | O_APPEND, 0644);
			else
				fd = open(err, O_WRONLY | O_CREAT | O_TRUNC, 0644);

			dup2(fd, 2);
			close(fd);
		}
	}

	// freeing the strings
	free(in);
	free(out);
	free(err);
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */

	/* TODO: If builtin command, execute the command. */

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */

	int status, p = fork();

	if (p == 0) {
		// handiling redirections
		parse_redirection(s);

		// calling the command from the child process
		int size;
		char **arg = get_argv(s, &size);

		execvp(arg[0], arg);

		// the execvp failed
		fprintf(stderr, "Execution failed for '%s'\n", get_word(s->verb));
		exit(EXIT_FAILURE);
	} else if (p == -1) {
		parse_error("Fork failed", level);
		status = -1;
	} else {
		waitpid(p, &status, 0);
	}
	return (WIFEXITED(status) && WEXITSTATUS(status)); /* TODO: Replace with actual exit status. */
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	int p1 = fork();
	int status, status2;

	if (p1 == 0) {
		// executing the first command
		status = parse_command(cmd1, level + 1, father);
		exit(status);
	} else if (p1 == -1) {
		parse_error("Fork failed", level);
		exit(false);
	} else {
		int p2 = fork();

		if (p2 == 0) {
			// executing the second command
			status = parse_command(cmd2, level + 1, father);
			exit(status);
		} else if (p2 == -1) {
			parse_error("Fork failed", level);
			exit(false);
		} else {
			// waiting for the second command to finish
			waitpid(p2, &status2, 0);
		}

		// waiting for first command to finish
		waitpid(p1, &status, 0);
	}

	return status2; /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	// creating the pipe
	int fd[2];
	int status;

	status = pipe(fd);
	if (status < 0)
		parse_error("Pipe failed", level);

	// first split
	int p1 = fork();

	if (p1 == 0) {
		// the first child becomes the one who sends the output
		close(fd[READ]);
		dup2(fd[WRITE], STDOUT_FILENO);
		close(fd[WRITE]);

		status = parse_command(cmd1, level + 1, father);

		exit(status);
	} else if (p1 == -1) {
		parse_error("Fork failed", level);
	}

	// the second split
	int p2 = fork();

	if (p2 == 0) {
		// the second child becomes the one who receives the input
		close(fd[WRITE]);
		dup2(fd[READ], STDIN_FILENO);
		close(fd[READ]);

		status = parse_command(cmd2, level + 1, father);

		exit(status);
	} else if (p2 == -1) {
		parse_error("Fork failed", level);
	}

	// closing the pipes for the parent
	close(fd[READ]);
	close(fd[WRITE]);

	// the parent waits for the children
	waitpid(p1, NULL, 0);
	waitpid(p2, &status, 0);

	return status; /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */
	int status;

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		char *command = get_word(c->scmd->verb);

		if (strchr(command, '=')) {
			// the command is actually an enviromental variable
			const char *name = c->scmd->verb->string;
			char *value = get_word(c->scmd->verb->next_part->next_part);

			status = setenv(name, value, 1);
		} else {
			// execute the command
			if (strcmp(c->scmd->verb->string, "cd") == 0) {
				int p = fork();

				if (p == 0) {
					parse_redirection(c->scmd);
					status = shell_cd(c->scmd->params);
					if (status == -1)
						parse_error("Path not found", level);
					exit(0);
				} else if (p == -1) {
					parse_error("Fork failed", level);
					status = -1;
				} else {
					int wp = waitpid(p, &status, 0);

					if (wp == -1)
						parse_error("Process failed", level);

					status = shell_cd(c->scmd->params);
				}
			} else if (strcmp(c->scmd->verb->string, "exit") == 0 || strcmp(c->scmd->verb->string, "quit") == 0) {
				status = shell_exit();
			} else {
				status = parse_simple(c->scmd, level, father);
			}
		}
		return status; /* TODO: Replace with actual exit code of command. */
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		status = parse_command(c->cmd1, level + 1, c);
		status = parse_command(c->cmd2, level + 1, c);

		break;

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		status = run_in_parallel(c->cmd1, c->cmd2, level, c);

		break;

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		status = parse_command(c->cmd1, level + 1, c);

		if (status != 0)
			parse_command(c->cmd2, level + 1, c);

		break;

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		status = parse_command(c->cmd1, level + 1, c);

		if (status == 0)
			status = parse_command(c->cmd2, level + 1, c);

		break;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		status = run_on_pipe(c->cmd1, c->cmd2, level, father);

		break;

	default:
		return SHELL_EXIT;
	}

	return status; /* TODO: Replace with actual exit code of command. */
}
