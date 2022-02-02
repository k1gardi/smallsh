// Author: Kaewan Gardi
// Course: CS344
// Date: 01/24/2022
// This program reads movie data from a csv and loads it into a sorted linked list.
// 		Then saves the movie titles into a new directory in .txt files organized and named
//		by release year.
//		The user can choose to load movie data from the largest file in a directory, the smallest file,
//		or a file with the name of their choosing
#include <dirent.h>
#include <err.h>
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
#define MAX_BACKGROUND 10
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

// Function prototypes
bool isCommentOrBlank(char *);
struct command *buildCommand(char *, bool);
void executeForeground(struct command *, int *);
void executeBackground(struct command *, int *, int *);
void checkBackgroundPIDs(int *, int *);
void printStatus(int);
void addPIDToArray(int *, int);
void handle_SIGTSTP(int);

/* This program reads movie data from a csv and loads it into a linked list.
* The user is then given multiple options to filter/display the data
*/
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

	struct sigaction handle_SIGTSTP_action = {0};
	handle_SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&handle_SIGTSTP_action.sa_mask);
	handle_SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &handle_SIGTSTP_action, NULL);

	bool foregroundMode = false;

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
		getline(&buff, &bufflen, stdin);

		// Parse input and build command structure
		struct command *currCommand = buildCommand(buff, foregroundMode);
		free(buff);

		// Ignore blank lines and commented lines
		// TODO: Make work for line full of whitespace.  Should probably buildCommand before this
		if (isCommentOrBlank(currCommand->name))
		{
			printf("line was blank or comment\n");
			fflush(stdout);
			continue;
		}
		// ******************** Built-in Commands ********************
		// Handle exit command
		if (strcmp(currCommand->name, "exit") == 0)
		{
			// TODO: Go through my array of child PIDs, kill them if they're running,
			// and then reach the return EXIT_SUCCESS?
			closedir(currDir);
			break;
		}

		// Handle pwd
		if (strcmp(currCommand->name, "pwd") == 0)
		{
			char currDirName[256];
			// TODO: REMOVE?? -> ????? char *test = getcwd(currDirName, 256);
			getcwd(currDirName, 256);

			printf("%s\n", currDirName);
			fflush(stdout);
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
				printf("changing dir to %s\n", newDir);
				fflush(stdout);
				cdError = chdir(newDir);
			}
			else
			{
				newDir = currCommand->argv[1];
				printf("changing dir to %s\n", newDir);
				fflush(stdout);
				cdError = chdir(newDir);
			}

			// Detect cd error
			if (cdError != 0)
			{
				printf("There was an error changing to directory %s\n", newDir);
				fflush(stdout);
			}
			continue;
		}

		// Handle status
		// prints either exit status or terminating signal of last foreground process ran by shell
		// If this command is run before any foreground command is run, then it should simply return the exit status 0.
		// The three built-in shell commands do not count as foreground processes for the purposes of this built-in command - i.e., status should ignore built-in commands.
		if (strncmp(buff, "status", 6) == 0)
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
		// Background command

		// **********************************************************
		// Non-native commands
		// Fork a child
		// Run command with exec
		// should allow shell scripts to be executed
		// if no command found in PATH print error message ant exit(1)
		// Child must terminate after command (for success and fail)

		// ************************************************************
		// 6. Input & Output Redirection
		// 7. Executing Commands in Foreground & Background
	}

	return 0;
}

// ########################################## END OF MAIN FUNCITON ######################################################################

/* Inserts a movie node into the LL in its correct sorted position.
***		Source Citation:
*				Code adapted from https://www.techiedelight.com/given-linked-list-change-sorted-order/ function "sortedInsert"
*				On date: 01/08/2022
*/

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

struct command *buildCommand(char *input, bool foregroundMode)
{
	struct command *currCommand = malloc(sizeof(struct command));
	char *argPtr;
	bool isInfile = false;
	bool isOutfile = false;

	currCommand->inDirect = NULL;
	currCommand->outDirect = NULL;
	currCommand->numArgs = 0;
	currCommand->isForeground = true;

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
	*nextArg++ = token;
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

		// TODO: Check if arg needs any expanding
		*nextArg++ = argument;
		currCommand->numArgs++;
		argument = strtok_r(NULL, " \n", &argPtr);
	}

	// Last argument should be null for exec calls
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

	return currCommand;
}

void freeCommand(struct command *oldCommand)
{
	// TODO: Free an old command struct
}

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

				// TODO: Clean process

				*lastStatusCode = childStatus;
				// Clear this PID from the array
				backgroundPIDs[i] = 0;
			}
		}
	}
}

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

void handle_SIGTSTP(int signo)
{
	char *message = "\nEntering foreground-only mode (& is now ignored)\n";
	// We are using write rather than printf
	write(STDOUT_FILENO, message, 50);
}