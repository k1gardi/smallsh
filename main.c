// Author: Kaewan Gardi
// Course: CS344
// Date: 02/02/2022
// Assignment 3: smallsh
// This program is a small C shell.  It meets the following requirements:
//	Provide a prompt for running commands
//	Handle blank lines and comments, which are lines beginning with the # character
//	Provide expansion for the variable $$
//	Execute 3 commands exit, cd, and status via code built into the shell
//	Execute other commands by creating new processes using a function from the exec family of functions
//	Support input and output redirection
//	Support running commands in foreground and background processes
//	Implement custom handlers for 2 signals, SIGINT and SIGTSTP

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE_LEN 2048
#define MAX_NUM_ARGS 512
#define MAX_BACKGROUND 200
#define DEFAULT_REDIRECT "/dev/null"

struct command
{
	char *name;
	char *argv[MAX_NUM_ARGS + 1];
	int numArgs;
	char *inDirect;
	char *outDirect;
	bool isForeground;
};

volatile sig_atomic_t foregroundMode = 0; // Tracks foreground only mode

// Function prototypes
bool isCommentOrBlank(char *);
struct command *buildCommand(char *);
void freeCommand(struct command *);
void executeForeground(struct command *, int *);
void executeBackground(struct command *, int *, int *);
void checkBackgroundPIDs(int *, int *);
void killBackgroundPIDs(int *);
void printStatus(int);
void addPIDToArray(int *, int);
void handle_SIGTSTP_FG_on(int);
void handle_SIGTSTP_FG_off(int);

// ########################################## BEGIN MAIN FUNCITON ######################################################################
int main(void)
{
	char *HOME = getenv("HOME");
	int lastStatusCode = 0;
	DIR *currDir = opendir(".");
	int activeBackgroundPIDs[MAX_BACKGROUND] = {0};

	// Set up SIGINT ignore handler
	struct sigaction ignoreAction = {0};
	ignoreAction.sa_handler = SIG_IGN;
	sigaction(SIGINT, &ignoreAction, NULL);

	signal(SIGTSTP, &handle_SIGTSTP_FG_off);
	int foregroundMode_save = foregroundMode;
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	// Execute main loop, maintaining shell prompting/reading
	while (true)
	{
		// Check background processes for completion
		checkBackgroundPIDs(activeBackgroundPIDs, &lastStatusCode);

		// Prompt for a new command
		printf(": ");
		fflush(stdout);
		char *buff = NULL;
		size_t bufflen;
		int numChars = getline(&buff, &bufflen, stdin);

		// Did SIGTSTP interrupt getline?
		if (numChars == -1)
		{
			clearerr(stdin); // reset stdin status
		}

		// Handle SIGTSTP Switch **************************************
		if (foregroundMode_save != foregroundMode)
		{
			foregroundMode_save = foregroundMode;
		}

		// ***********************************************************
		// Parse input and build command structure
		struct command *currCommand = buildCommand(buff);

		// Ignore blank lines and commented lines
		if (isCommentOrBlank(currCommand->name))
		{
			fflush(stdout);
			free(buff);
			continue;
		}
		// ******************** Built-in Commands ********************
		// Handle exit command
		if (strcmp(currCommand->name, "exit") == 0)
		{
			killBackgroundPIDs(activeBackgroundPIDs);
			closedir(currDir);
			free(buff);
			exit(EXIT_SUCCESS);
		}

		// Handle pwd
		if (strcmp(currCommand->name, "pwd") == 0)
		{
			char currDirName[256];
			getcwd(currDirName, 256);

			printf("%s\n", currDirName);
			fflush(stdout);
			free(buff);
			continue;
		}

		// Handle cd
		if (strcmp(currCommand->name, "cd") == 0)
		{
			int cdError = 0;
			char *newDir = NULL;
			if (currCommand->numArgs == 1)
			{
				newDir = HOME;
				cdError = chdir(newDir);
			}
			else
			{
				newDir = currCommand->argv[1];
				cdError = chdir(newDir);
			}

			// Detect cd error
			if (cdError != 0)
			{
				printf("There was an error changing to directory %s\n", newDir);
				fflush(stdout);
			}
			free(buff);
			continue;
		}

		// Handle status
		if (strcmp(currCommand->name, "status") == 0)
		{
			printStatus(lastStatusCode);
			continue;
		}

		// ******************** Non Built-in Commands ********************
		// Foreground command
		if (currCommand->isForeground)
		{
			executeForeground(currCommand, &lastStatusCode);
		}
		// Background command
		else
		{
			executeBackground(currCommand, &lastStatusCode, activeBackgroundPIDs);
		}

		free(buff);
	}

	return 0;
}

// ########################################## END OF MAIN FUNCITON ######################################################################

/* Returns whether the given line is blank or a comment */
bool isCommentOrBlank(char *line)
{
	if (line == NULL)
	{
		return true;
	}
	if (strlen(line) == 0)
	{
		return true;
	}

	if (strncmp(line, "#", 1) == 0)
	{
		return true;
	}

	return false;
}

/* Returns a command structure holding data from the given line.
*	strtok is used to parse user input and build the command struct
*	This is where final data will be determined as saved for the following:
*	Command name (function), NULL terminated args, outfile for redirection (if any),
*	infile for redirection (if any), and whether the command should be ran in the
*	foreground or background.
*/
struct command *buildCommand(char *input)
{
	struct command *currCommand = malloc(sizeof(struct command));
	char *argPtr;

	// Get PID string
	pid_t raw_PID = getpid();
	int length = snprintf(NULL, 0, "%d", raw_PID);
	char *PID = calloc(length + 1, sizeof(char));
	sprintf(PID, "%d", raw_PID);

	bool isInfile = false;
	bool isOutfile = false;

	// Set initial vals
	currCommand->inDirect = NULL;
	currCommand->outDirect = NULL;
	currCommand->numArgs = 0;
	currCommand->isForeground = true;

	// ************************************************************************
	// Get command name
	char *token = strtok_r(input, " \n", &argPtr);
	// Handle empty command
	if (token == NULL)
	{
		return currCommand;
	}

	currCommand->name = calloc(strlen(token) + 1, sizeof(char));
	strcpy(currCommand->name, token);

	// ************************************************************************
	// Parse out remaining arguments
	char **nextArg = currCommand->argv;
	*nextArg++ = currCommand->name;
	currCommand->numArgs++;

	char *argument = strtok_r(NULL, " \n", &argPtr);
	while (argument != NULL)
	{
		// Check if we need to redirect stdin or out
		if (strcmp(argument, "<") == 0)
		{
			isInfile = true;
			argument = strtok_r(NULL, " \n", &argPtr);
			continue;
		}
		if (strcmp(argument, ">") == 0)
		{
			isOutfile = true;
			argument = strtok_r(NULL, " \n", &argPtr);
			continue;
		}

		// This argument is an input direction
		if (isInfile)
		{
			isInfile = false;
			currCommand->inDirect = calloc(strlen(argument) + 1, sizeof(char));
			strcpy(currCommand->inDirect, argument);
			argument = strtok_r(NULL, " \n", &argPtr);
			continue;
		}

		// This argument is an output direction
		if (isOutfile)
		{
			isOutfile = false;
			currCommand->outDirect = calloc(strlen(argument) + 1, sizeof(char));
			strcpy(currCommand->outDirect, argument);
			argument = strtok_r(NULL, " \n", &argPtr);
			continue;
		}

		// Check if arg needs any expanding
		char *arg = calloc(256, sizeof(char));
		for (int i = 0; i < strlen(argument); i++)
		{
			// Do we need to expand here?
			if (i < strlen(argument) - 1 && strncmp(&argument[i], "$$", 2) == 0)
			{
				strcat(arg, PID);
				i++;
			}
			// Don't expand, just copy next char
			else
			{
				strncat(arg, &argument[i], 1);
			}
		}

		*nextArg++ = arg;
		currCommand->numArgs++;
		argument = strtok_r(NULL, " \n", &argPtr);
	}

	// Last argument should be null for exec calls to work
	*nextArg = NULL;

	// ************************************************************************
	// Is background funciton?
	if (strcmp(currCommand->argv[currCommand->numArgs - 1], "&") == 0)
	{
		currCommand->argv[currCommand->numArgs - 1] = NULL;
		// If we are in foreground only mode, do not set this as a background command
		if (!foregroundMode)
		{
			currCommand->isForeground = false;

			// Make sure std input and output are marked to redirect
			if (!currCommand->inDirect)
			{
				currCommand->inDirect = calloc(strlen(DEFAULT_REDIRECT) + 1, sizeof(char));
				strcpy(currCommand->inDirect, DEFAULT_REDIRECT);
			}
			if (!currCommand->outDirect)
			{
				currCommand->outDirect = calloc(strlen(DEFAULT_REDIRECT) + 1, sizeof(char));
				strcpy(currCommand->outDirect, DEFAULT_REDIRECT);
			}
		}
	}

	free(PID);
	return currCommand;
}

/* Runs the provided command in the foreground */
void executeForeground(struct command *activeCommand, int *lastStatusCode)
{
	int childStatus;
	char *commName = activeCommand->name;
	struct sigaction obeyAction = {0};

	// Fork a new process
	pid_t spawnPid = fork();

	switch (spawnPid)
	{
	case -1:
		perror("fork()\n");
		*lastStatusCode = 1;
		exit(1);
		break;
	case 0:
		// In the child process
		// Set up signal handler
		obeyAction.sa_handler = SIG_DFL;
		sigaction(SIGINT, &obeyAction, NULL);

		// Do we need to redirect input?
		if (activeCommand->inDirect)
		{
			int sourceFD = open(activeCommand->inDirect, O_RDONLY);
			if (sourceFD == -1)
			{
				perror("source open()");
				exit(1);
			}

			// Redirect stdin
			int inCode = dup2(sourceFD, 0);
			if (inCode == -1)
			{
				perror("source dup2()");
				exit(2);
			}
		}
		// Do we need to redirect output?
		if (activeCommand->outDirect)
		{
			int targetFD = open(activeCommand->outDirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (targetFD == -1)
			{
				perror("target open()");
				exit(1);
			}

			// Redirect stdout
			int outCode = dup2(targetFD, 1);
			if (outCode == -1)
			{
				perror("target dup2()");
				exit(2);
			}
		}

		execvp(commName, activeCommand->argv);

		// exec only returns if there is an error
		perror("execvp");
		exit(2);
		break;
	default:
		// In the parent process
		// Wait for child's termination
		spawnPid = waitpid(spawnPid, &childStatus, 0);

		// Was this a signal termination??
		if (WIFSIGNALED(childStatus))
		{
			int readableCode = WTERMSIG(childStatus);
			printf("terminated by signal %d\n", readableCode);
		}
		*lastStatusCode = childStatus;
		break;
	}
}

/* Runs the provided command in the background and updates backgroundPIDs array */
void executeBackground(struct command *activeCommand, int *lastStatusCode, int *backgroundPIDs)
{
	int childStatus;
	char *commName = activeCommand->name;

	// Fork a new process
	pid_t spawnPid = fork();

	switch (spawnPid)
	{
	case -1:
		perror("fork()\n");
		*lastStatusCode = 1;
		exit(1);
		break;
	case 0:
		// In the child process
		// Do we need to redirect input?
		if (activeCommand->inDirect)
		{
			// Open infile
			int sourceFD = open(activeCommand->inDirect, O_RDONLY);
			if (sourceFD == -1)
			{
				perror("source open()");
				exit(1);
			}

			// Redirect stdin
			int inCode = dup2(sourceFD, 0);
			if (inCode == -1)
			{
				perror("source dup2()");
				exit(2);
			}
		}
		// Do we need to redirect output?
		if (activeCommand->outDirect)
		{
			// Open outfile
			int targetFD = open(activeCommand->outDirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (targetFD == -1)
			{
				perror("target open()");
				exit(1);
			}

			// Redirect stdout
			int outCode = dup2(targetFD, 1);
			if (outCode == -1)
			{
				perror("target dup2()");
				exit(2);
			}
		}

		execvp(commName, activeCommand->argv);

		// exec only returns if there is an error
		perror("execvp");
		exit(2);
		break;
	default:
		// In the parent process
		printf("background pid is %d\n", spawnPid);
		addPIDToArray(backgroundPIDs, spawnPid);
		spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
		*lastStatusCode = childStatus;
		break;
	}
}

/* Checks backgroundPIDs array for any processes that have been completed since we last checked.
*	Updates last status code if appropriate.
*/
void checkBackgroundPIDs(int *backgroundPIDs, int *lastStatusCode)
{
	for (int i = 0; i < MAX_BACKGROUND; i++)
	{
		if (backgroundPIDs[i])
		{
			int childStatus;
			// Check if process is done
			if (waitpid(backgroundPIDs[i], &childStatus, WNOHANG) != 0)
			{
				// Print completion
				printf("background pid %d is done: ", backgroundPIDs[i]);
				printStatus(childStatus);
				fflush(stdout);

				*lastStatusCode = childStatus;
				// Clear this PID from the array
				backgroundPIDs[i] = 0;
			}
		}
	}
}

/* Prints information for the given statusCode*/
void printStatus(int statusCode)
{
	int readableCode;
	if (WIFEXITED(statusCode))
	{
		readableCode = WEXITSTATUS(statusCode);
		printf("exit value %d\n", readableCode);
	}
	else
	{
		readableCode = WTERMSIG(statusCode);
		printf("terminated by signal %d\n", readableCode);
	}
}

/* Adds a child process PID to the array of current child PIDs */
void addPIDToArray(int *backgroundPIDs, int pid)
{
	for (int i = 0; i < MAX_BACKGROUND; i++)
	{
		if (!backgroundPIDs[i])
		{
			backgroundPIDs[i] = pid;
			break;
		}
	}
}

/* SIGTSTP handler function used when foreground mode is on.
*	Turns foreground mode off when triggered.
*/
void handle_SIGTSTP_FG_on(int sig)
{
	foregroundMode = 0;
	char *message = "Exiting foreground-only mode\n";
	write(STDOUT_FILENO, message, 29);
	signal(SIGTSTP, &handle_SIGTSTP_FG_off);
}

/* SIGTSTP handler function used when foreground mode is off.
*	Turns foreground mode on when triggered.
*/
void handle_SIGTSTP_FG_off(int sig)
{
	foregroundMode = 1;
	char *message = "Entering foreground-only mode (& is now ignored)\n";
	write(STDOUT_FILENO, message, 49);
	signal(SIGTSTP, &handle_SIGTSTP_FG_on);
}

/* Kills all outstanding child background processes.
*	This is in preperation for the main shell's exit.
*/
void killBackgroundPIDs(int *backgroundPIDs)
{
	// Iterate over background PIDs
	for (int i = 0; i < MAX_BACKGROUND; i++)
	{
		if (backgroundPIDs[i])
		{
			int childStatus;
			// Check if process is already done
			if (waitpid(backgroundPIDs[i], &childStatus, WNOHANG) != 0)
			{
				backgroundPIDs[i] = 0;
			}
			else
			{
				int ret = kill(backgroundPIDs[i], SIGKILL);
				if (ret == -1)
				{
					perror("kill");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
}
