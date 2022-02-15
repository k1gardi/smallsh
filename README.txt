Name: Kaewan Gardi
Course: CS344
Assignment: a3: smallsh
Description:
This program is a small C shell with following functionality:
*   Provide a prompt for running commands
*   Handle blank lines and comments (lines beginning with the '#' character)
*   Provide expansion for the variable $$ (this will expand to smallsh's process ID)
*	Execute 3 commands exit, cd, and status via code built into the shell
*	Execute other commands by creating new child processes
*	Support input and output redirection
*	Support running commands in foreground and background processes
*	Implement custom handlers for 2 signals, SIGINT and SIGTSTP

Instructions:
    run `make` to build smallsh.
    run `./smallsh` to run the program

Note: GNU99 standard was used