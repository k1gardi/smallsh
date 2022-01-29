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

#define MAX_NUM_LANGS 5
#define MAX_LANG_LENGTH 20
#define PREFIX "movies_"
#define FILETYPE ".csv"
#define ONID "gardik"

int movieCount; // Points to the number of movies pulled from the data-file

/* This struct holds movie data as a node in part of a linked list*/
struct movie
{
	char *title;
	int year;
	char languages[MAX_NUM_LANGS][MAX_LANG_LENGTH + 1];
	char *rating;
	struct movie *next;
};

// Function prototypes
void sortedInsert(struct movie **, struct movie *);
void sortByYear(struct movie **);
struct movie *createMovie(char *);
struct movie *compileMoviesFromFile(char *);
void freeMovies(struct movie *);
char *findLargestFile(void);
char *findSmallestFile(void);
void processFile(char *fileName);
char *generateDirName(void);
void writeMoviesByYear(struct movie *, char *);
bool verifyFilename(char *);

/* This program reads movie data from a csv and loads it into a linked list.
* The user is then given multiple options to filter/display the data
*/
int main(void)
{
	int menuSelection;
	char *fileName = calloc(256, sizeof(char));
	while (true)
	{
		// Present initial options and get choice
		printf("1. Select file to process\n");
		printf("2. Exit the program\n");
		printf("\nEnter a choice 1 or 2: ");
		scanf("%d", &menuSelection);

		// Validate input
		if (menuSelection < 1 || menuSelection > 2)
		{
			printf("Bad Input. Please enter a valid integer choice of 1 or 2.\n\n");
			continue;
		}

		// Exit program
		if (menuSelection == 2)
			break;

		// Present second menu and get choice
		menuSelection = 0;
		while (true)
		{
			printf("\nWhich file you want to process?\n");
			printf("Enter 1 to pick the largest file\n");
			printf("Enter 2 to pick the smallest file\n");
			printf("Enter 3 to specify the name of a file\n");
			printf("\nEnter a choice from 1 to 3: ");
			scanf("%d", &menuSelection);


			// Load largest file
			if (menuSelection == 1)
				fileName = findLargestFile();

			// Load smallest file
			else if (menuSelection == 2)
				fileName = findSmallestFile();

			// Get filename from user and verify
			else if (menuSelection == 3)
			{
				printf("Enter the complete file name: ");
				char *buf = NULL;
				size_t buflen;
				getchar();		// Clear newline thats waiting in stdin
				getline(&buf, &buflen, stdin);
				int inputlen = strlen(buf);

				if (buf[inputlen-1] == '\n')
					buf[inputlen-1]  = '\0';

				if (!verifyFilename(buf))
				{
					printf("The file %s was not found. Try again\n", buf);
					continue;
				}
				fileName = buf;
			}

			// Validate filename
			if (strlen(fileName) == 0)
			{
				printf("No file found matching that request. Try again\n");
				free(fileName);
				continue;
			}

			printf("\nNow processing the chosen file named %s\n", fileName);
			processFile(fileName);
			break;
		}
	}
	free(fileName);
	return 0;
}

// ########################################## END OF MAIN FUNCITON ######################################################################

/* Inserts a movie node into the LL in its correct sorted position.
***		Source Citation:
*				Code adapted from https://www.techiedelight.com/given-linked-list-change-sorted-order/ function "sortedInsert"
*				On date: 01/08/2022
*/
void sortedInsert(struct movie **head, struct movie *newMovie)
{
	struct movie temp;
	struct movie *current = &temp;
	temp.next = *head;

	// Traverse list until we find the position for the node
	while (current->next != NULL && current->next->year < newMovie->year)
	{
		current = current->next;
	}

	newMovie->next = current->next;
	current->next = newMovie;
	*head = temp.next;
}

/* Sort the given list by movie year 
***		Source Citation:
*				Code adapted from https://www.techiedelight.com/given-linked-list-change-sorted-order/ function "insertSort"
*				On date: 01/08/2022
*/
void sortByYear(struct movie **head)
{
	struct movie *result = NULL;   // build the answer here
	struct movie *current = *head; // iterate over the original list
	struct movie *next;

	// Keeping track of the head, add each node to the new LL in sorted order
	while (current != NULL)
	{
		next = current->next;
		sortedInsert(&result, current);
		current = next;
	}

	*head = result;
}

/* Takes a line of movie data and returns a populated movie struct of the data */
struct movie *createMovie(char *movieData)
{
	struct movie *currMovie = malloc(sizeof(struct movie));
	char *linePtr;
	char *languagePtr;

	// **************************************************
	// The first token is the movie title
	char *token = strtok_r(movieData, ",", &linePtr);
	currMovie->title = calloc(strlen(token) + 1, sizeof(char));
	strcpy(currMovie->title, token);

	// **************************************************
	// The next token is the year
	token = strtok_r(NULL, ",", &linePtr);
	currMovie->year = atoi(token);

	// **************************************************
	// The next token is the languages list
	// For the first language strip the leading '['
	linePtr = linePtr + sizeof(char);
	token = strtok_r(NULL, "]", &linePtr);
	char *language = strtok_r(token, ";", &languagePtr);

	// Build languages array
	int langIndex = 0;
	while (language != NULL || langIndex > 5)
	{
		strcpy(currMovie->languages[langIndex], language);
		language = strtok_r(NULL, ";", &languagePtr);
		langIndex++;
	}
	// strip the trailing ',' after languages
	linePtr = linePtr + sizeof(char);

	// **************************************************
	// The last token is the rating
	token = strtok_r(NULL, "\r", &linePtr);
	currMovie->rating = calloc(strlen(token) + 1, sizeof(char));
	strcpy(currMovie->rating, token);

	// **************************************************
	// Set the next node to NULL in the newly created movie entry
	currMovie->next = NULL;

	return currMovie;
}

/* Frees all memory allocated for each movie node */
void freeMovies(struct movie *currMovie)
{
	struct movie *temp;

	while (currMovie != NULL)
	{
		temp = currMovie;
		currMovie = currMovie->next;

		// Free all calloc'd fields then free node
		free(temp->title);
		free(temp->rating);
		free(temp);
	}
}

/*
* Return a linked list of movies by parsing data from each line of the specified file.
*** 	Source Citation:
*				Code adapted from the provided sample code at https://replit.com/@cs344/studentsc#student_info1.txt
*				On date: 01/07/2022
*/
struct movie *compileMoviesFromFile(char *fileName)
{
	FILE *moviesFile = fopen(fileName, "r");

	// Validate file
	if (moviesFile == NULL)
	{
		printf("Error reading the provided filename.\nPlease enter a valid filename\n");
		return NULL;
	}

	char *currLine = NULL;
	size_t len = 0;
	ssize_t nread;

	// The head of the linked list
	struct movie *head = NULL;
	// The tail of the linked list
	struct movie *tail = NULL;

	// Advance our pointer past 1st line before entering loop
	getline(&currLine, &len, moviesFile);

	// Read the file line by line
	while ((nread = getline(&currLine, &len, moviesFile)) != -1)
	{
		// Get a new student node corresponding to the current line
		struct movie *newNode = createMovie(currLine);

		// Is this the first node in the linked list?
		if (head == NULL)
		{
			// This is the first node in the linked link
			// Set the head and the tail to this node
			head = newNode;
			tail = newNode;
		}
		else
		{
			// This is not the first node.
			// Add this node to the list and advance the tail
			tail->next = newNode;
			tail = newNode;
		}
		movieCount++;
	}
	free(currLine);
	fclose(moviesFile); // Close file
	return head;
}

/* Checks if a string ends with ".csv" */
bool isCSV(char *fileName)
{
	size_t fileNameLength = strlen(fileName);
	size_t fileTypeLength = strlen(FILETYPE);
	if (fileTypeLength > fileNameLength)
	{
		return false;
	}
	return strncmp(fileName + fileNameLength - fileTypeLength, FILETYPE, fileTypeLength) == 0;
}
/* Returns the string name of the largest csv file in the current directory with the prefix "movies_" 
* Citation:
*	Code adapted from exploration example at https://replit.com/@cs344/35statexamplec
*/
char *findLargestFile(void)
{
	// Prepare to iterate through fileNames
	int largestSize = 0;
	char entryName[256];
	DIR *currDir = opendir(".");
	struct dirent *aDir;
	struct dirent *readdir(DIR * dirp);
	struct stat dirStat;
	int i = 0;

	while ((aDir = readdir(currDir)) != NULL)
	{
		// Verify filetype and prefix
		if (strncmp(PREFIX, aDir->d_name, strlen(PREFIX)) == 0 && isCSV(aDir->d_name))
		{
			// Get meta-data for the current entry
			stat(aDir->d_name, &dirStat);

			// Remember this fileName if its size is greater than the current greatest size
			if (i == 0 || dirStat.st_size > largestSize)
			{
				largestSize = dirStat.st_size;
				memset(entryName, '\0', sizeof(entryName));
				strcpy(entryName, aDir->d_name);
			}
			i++;
		}
	}
	char *fileName = calloc(256, sizeof(char));
	strcpy(fileName, entryName);
	closedir(currDir);
	return fileName;
}

/* Returns the string name of the smallest csv file in the current directory with the prefix "movies_" 
* Citation:
*	Code adapted from exploration example at https://replit.com/@cs344/35statexamplec
*/
char *findSmallestFile(void)
{
	// Prepare to iterate through fileNames
	int smallestSize = 0;
	char entryName[256];
	DIR *currDir = opendir(".");
	struct dirent *aDir;
	struct dirent *readdir(DIR * dirp);
	struct stat dirStat;
	int i = 0;

	while ((aDir = readdir(currDir)) != NULL)
	{
		// Verify filetype and prefix
		if (strncmp(PREFIX, aDir->d_name, strlen(PREFIX)) == 0 && isCSV(aDir->d_name))
		{
			// Get meta-data for the current entry
			stat(aDir->d_name, &dirStat);

			// Remember this fileName if its size is greater than the current greatest size
			if (i == 0 || dirStat.st_size < smallestSize)
			{
				smallestSize = dirStat.st_size;
				memset(entryName, '\0', sizeof(entryName));
				strcpy(entryName, aDir->d_name);
			}
			i++;
		}
	}
	char *fileName = calloc(256, sizeof(char));
	strcpy(fileName, entryName);
	closedir(currDir);
	return fileName;
}

/* Verifies that user provided filename exists in the current directory */
bool verifyFilename(char *filename)
{
	FILE *fp;
	if ((fp = fopen(filename, "r")))
	{
		fclose(fp);
		return true;
	}
	return false;
}

/* Processes the data in the infile and saves movie data by year into a new directory.
*		Creates a new directory with name "ONID.movies.XXXXX" where XXXXX is a random int in [0,99999].
*		Reads data from given csv one line at a time.
*		Each movie is written into a file in the new directory corresponding to its release year.
*/
void processFile(char *infileName)
{
	// Load and sort data from csv
	struct movie *movieListHead = compileMoviesFromFile(infileName);
	sortByYear(&movieListHead);

	// Create new directory
	char *outDirName = generateDirName();
	int verifyDir = mkdir(outDirName, 0750);
	if (verifyDir != 0)
	{
		printf("failed to create directory with name: %s\n", outDirName);
		exit(-1);
	}

	// write movie data to new directory
	writeMoviesByYear(movieListHead, outDirName);
	printf("Created directory with name %s\n\n", outDirName);
	free(outDirName);
	freeMovies(movieListHead);
}

/* Generates a new random directory name*/
char *generateDirName(void)
{
	// Get random string in range [0, 99999]
	srandom(time(NULL));
	int randInt = random() % 100000;
	char randString[6];
	sprintf(randString, "%05d", randInt);

	// Build outdir name
	char *outdirName = calloc(strlen(ONID) + strlen(".movies.") + strlen(randString) + 1, sizeof(char));
	strcpy(outdirName, ONID);
	strcat(outdirName, ".movies.");
	strcat(outdirName, randString);

	return outdirName;
}

/* Writes all movie titles into files organized by release year.
	Files are placed into previously created directory "outDirName"
*/
void writeMoviesByYear(struct movie *node, char *outDirName)
{
	int currYear = 9999;
	FILE *outfile;
	bool isOpenFile = false;

	while (node != NULL)
	{
		// Do we need to open a new file?
		if (node->year != currYear)
		{
			// Build new file name
			currYear = node->year;
			char yearStr[5];
			sprintf(yearStr, "%04d", currYear);
			char *fileName = calloc(strlen(outDirName) + 1 + strlen(yearStr) + strlen(".txt") + 1, sizeof(char));
			strcat(fileName, outDirName);
			strcat(fileName, "/");
			strcat(fileName, yearStr);
			strcat(fileName, ".txt");

			// Close any open file
			if (isOpenFile)
			{
				fclose(outfile);
			}

			// Open new file with appropriate permissions
			outfile = fopen(fileName, "a");
			chmod(fileName, 0640);
			isOpenFile = true;
			free(fileName);
		}

		// Keep writing to open file
		fprintf(outfile, "%s\n", node->title);
		node = node->next;
	}

	// Close any outstanding files and directories
	if (isOpenFile)
	{
		fclose(outfile);
	}
}
