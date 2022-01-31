// Author: Kaewan Gardi
// Course: CS344
// Date: 01/24/2022
// This program reads movie data from a csv and loads it into a sorted linked list.
// 		Then saves the movie titles into a new directory in .txt files organized and named
//		by release year.
//		The user can choose to load movie data from the largest file in a directory, the smallest file,
//		or a file with the name of their choosing
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINE_LEN 2048
#define MAX_NUM_ARGS 512

struct command
{
	char *name;
	char arguments[MAX_NUM_ARGS][256];
	int numArgs;
	char *inDirect;
	char *outDirect;
	int processId;
	int exitStatus;
};

// Function prototypes
bool isCommentOrBlank(char *);
struct command *buildCommand(char *);

/* This program reads movie data from a csv and loads it into a linked list.
* The user is then given multiple options to filter/display the data
*/
int main(void)
{
	char *HOME = getenv("HOME");
	int exitStatus = 0;
	DIR *currDir = opendir(".");

	// Execute main loop, maintaining shell prompting/reading
	while (true)
	{
		printf(": ");
		fflush(stdout);
		char *buff = NULL;
		size_t bufflen;
		getline(&buff, &bufflen, stdin);
		int inputlen = strlen(buff);

		if (buff[inputlen - 1] == '\n')
			buff[inputlen - 1] = '\0';

		// Ignore comment lines (beginning with '#') -> continue
		// Ignore blank lines -> continue
		if (isCommentOrBlank(buff))
		{
			free(buff);
			printf("line was blank or comment\n");
			fflush(stdout);
			continue;
		}
		// ******************** Built-in Commands ********************
		// Handle exit command
		if (strncmp(buff, "exit", 4))
		{
			// TODO: Go through my array of child PIDs, kill them if they're running,
			// and then reach the return EXIT_SUCCESS?
			free(buff);
			closedir(currDir);
			exit(0);
		}

		// Handle cd
		struct command *currCommand = buildCommand(buff);
		// no args -> go to dir specified in HOME env var
		// Takes path as second arg.  Must support absolute and relative path
		if (strncmp(buff, "cd", 2))
		{

			char *newDir = currCommand->arguments[1];

			printf("changing dir to %s", newDir);
			chdir(newDir);
			continue;
		}

		// Handle status
		// prints either exit status or terminating signal of last foreground process ran by shell
		// If this command is run before any foreground command is run, then it should simply return the exit status 0.
		// The three built-in shell commands do not count as foreground processes for the purposes of this built-in command - i.e., status should ignore built-in commands.
		else if (strncmp(buff, "status", 6))
		{
			printf("Exit value %d\n", exitStatus);
			fflush(stdout);
			continue;
		}

		// ******************** Non Built-in Commands ********************

		// Check for <, >, and &
		// last word '&' means execute command in background. '&' elsewhere -> treat like normal
		// < and > are used for file redirection after all command args

		// Expand instances of "$$" (dont need to expand other variables)

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

	currCommand->inDirect = NULL;
	currCommand->outDirect = NULL;
	currCommand->numArgs = 0;

	// Get command name
	char *token = strtok_r(input, " ", &argPtr);
	currCommand->name = calloc(strlen(token) + 1, sizeof(char));
	strcpy(currCommand->name, token);

	// Build argument array into command struct
	int argIndex = 0;
	strcpy(currCommand->arguments[argIndex], currCommand->name);
	argIndex++;

	char *argument = strtok_r(NULL, " ", &argPtr);
	while (argument != NULL)
	{
		// TODO is this a > or < or $?????  Do the thing
		currCommand->numArgs++;
		strcpy(currCommand->arguments[argIndex], argument);
		argument = strtok_r(NULL, ";", &argPtr);
		argIndex++;
	}

	// Last argument should be null for exec calls
	// TODO:
	// currCommand->arguments[argIndex] = NULL;

	return currCommand;
}

void freeCommand(struct command *oldCommand)
{
	// TODO: Free an old command struct
}