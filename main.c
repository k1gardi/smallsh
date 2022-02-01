// Author: Kaewan Gardi
// Course: CS344
// Date: 01/24/2022
// This program reads movie data from a csv and loads it into a sorted linked list.
// 		Then saves the movie titles into a new directory in .txt files organized and named
//		by release year.
//		The user can choose to load movie data from the largest file in a directory, the smallest file,
//		or a file with the name of their choosing
#include <dirent.h>
#include <fcntl.h>
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
#define DEFAULT_REDIRECT "/dev/null"

struct command
{
	char *name;
	char *argv[MAX_NUM_ARGS + 1];
	int numArgs;
	char *inDirect;
	char *outDirect;
	int processId;
	int exitStatus;
	bool isForeground;
};

// Function prototypes
bool isCommentOrBlank(char *);
struct command *buildCommand(char *);
void executeForeground(struct command *, int *);

/* This program reads movie data from a csv and loads it into a linked list.
* The user is then given multiple options to filter/display the data
*/
int main(void)
{
	char *HOME = getenv("HOME");
	int statusCode = 0;
	DIR *currDir = opendir(".");

	// Execute main loop, maintaining shell prompting/reading
	while (true)
	{
		printf(": ");
		fflush(stdout);
		char *buff = NULL;
		size_t bufflen;
		getline(&buff, &bufflen, stdin);

		// Ignore blank lines and commented lines
		if (isCommentOrBlank(buff))
		{
			free(buff);
			printf("line was blank or comment\n");
			fflush(stdout);
			continue;
		}
		// ******************** Built-in Commands ********************
		// Handle exit command
		if (strncmp(buff, "exit", 4) == 0)
		{
			// TODO: Go through my array of child PIDs, kill them if they're running,
			// and then reach the return EXIT_SUCCESS?
			free(buff);
			closedir(currDir);
			break;
		}

		// Handle pwd
		if (strncmp(buff, "pwd", 3) == 0)
		{
			char currDirName[256];
			// TODO: REMOVE?? -> ????? char *test = getcwd(currDirName, 256);
			getcwd(currDirName, 256);

			printf("%s\n", currDirName);
			fflush(stdout);
			free(buff);
			continue;
		}

		struct command *currCommand = buildCommand(buff);

		// Handle cd
		// no args -> go to dir specified in HOME env var
		// Takes path as second arg.  Must support absolute and relative path
		if (strncmp(buff, "cd", 2) == 0)
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
			free(buff);
			continue;
		}

		// Handle status
		// prints either exit status or terminating signal of last foreground process ran by shell
		// If this command is run before any foreground command is run, then it should simply return the exit status 0.
		// The three built-in shell commands do not count as foreground processes for the purposes of this built-in command - i.e., status should ignore built-in commands.
		if (strncmp(buff, "status", 6) == 0)
		{
			printf("Exit value %d\n", statusCode);
			fflush(stdout);
			continue;
		}

		// ******************** Non Built-in Commands ********************
		// Foreground command
		if (currCommand->isForeground)
		{
			executeForeground(currCommand, &statusCode);
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

struct command *buildCommand(char *input)
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
		currCommand->isForeground = false;
		currCommand->argv[currCommand->numArgs - 1] = NULL;

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

	return currCommand;
}

void freeCommand(struct command *oldCommand)
{
	// TODO: Free an old command struct
}

void executeForeground(struct command *activeCommand, int *statusCode)
{
	int childStatus;
	char *commName = activeCommand->name;

	// Fork a new process
	pid_t spawnPid = fork();

	switch (spawnPid)
	{
	case -1:
		perror("fork()\n");
		*statusCode = 1;
		exit(1);
		break;
	case 0:
		// In the child process
		// TODO: REMOVE PRINT STATEMENT
		printf("CHILD(%d) running ls command\n", getpid());

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
		if (WIFEXITED(childStatus))
		{
			*statusCode = WEXITSTATUS(childStatus);
		}
		else
		{
			*statusCode = WTERMSIG(childStatus);
		}
		break;
	}
}