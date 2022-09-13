// Matthew Roberts
// Smallsh Portfolio Project
// 1/29/2022
// This is a program for running my own version of a shell complete
// with similar (but basic) functionality to the existing shell.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

// Global variable needed to keep track of allowed background commands
bool backgroundAllowed = true;

typedef struct commandPackage {
	/// <summary>
	/// Struct used to hold the package of information that the user entered.
	/// </summary>
	char* command;
	char* arguments[512];
	int numArgs;
	char* inputFile;
	char* outputFile;
	bool runBackground;
} commandPackage;

void handle_SIGTSTP(int signo) {
	/// <summary>
	/// Custom signal handler for SIGTSTP used in the shell process.
	/// </summary>
	/// <param name="signo">Signal number corresponding to SIGTSTP.</param>
	fflush(stdout);
	if (backgroundAllowed == true) {
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 51);
		backgroundAllowed = false;
	}
	else {
		char* message = "\nExiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 31);
		backgroundAllowed = true;
	}
	fflush(stdout);
}

void updateChildSIGINT() {
	/// <summary>
	/// Updates child process SIGINT catch to just be default.
	/// </summary>
	
	// Initialize SIGINT_child struct to be empty
	struct sigaction SIGINT_child = { { 0 } };

	// Fill out the SIGINT_child struct
	// Register handle_SIGINT as the signal handler
	SIGINT_child.sa_handler = SIG_DFL;
	// Block all catchable signals while this is processing
	sigfillset(&SIGINT_child.sa_mask);
	// No flags set
	SIGINT_child.sa_flags = 0;
	// Install our signal handler
	sigaction(SIGINT, &SIGINT_child, NULL);
}

void updateChildSIGTSTP() {
	/// <summary>
	/// Updates child process SIGTSTP catch to ignore.
	/// </summary>
	
	// Initialize SIGTSTP_child struct to be empty
	struct sigaction SIGTSTP_child = { { 0 } };

	// Fill out the SIGTSTP_child struct
	// Register SIG_IGN as the signal handler
	SIGTSTP_child.sa_handler = SIG_IGN;
	// Block all catchable signals while this is processing
	sigfillset(&SIGTSTP_child.sa_mask);
	// No flags set
	SIGTSTP_child.sa_flags = 0;
	// Install our signal handler
	sigaction(SIGTSTP, &SIGTSTP_child, NULL);
}

void installSigCatchers() {
	/// <summary>
	/// Initial setup for signal catchers for parent process.
	/// </summary>
	
	// Initialize SIGINT_action struct to be empty
	struct sigaction SIGINT_action = { { 0 } };

	// Fill out the SIGINT_action struct
	// Register SIG_IGN (ignore signal) as the signal handler
	SIGINT_action.sa_handler = SIG_IGN;
	// Block all catchable signals while this is processing
	sigfillset(&SIGINT_action.sa_mask);
	// No flags set
	SIGINT_action.sa_flags = 0;
	// Install our signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Initialize SIGTSTP_action struct to be empty
	struct sigaction SIGTSTP_action = { { 0 } };

	// Fill out the SIGTSTP_action struct
	// Register SIG_IGN (ignore signal) as the signal handler
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Block all catchable signals while this is processing
	sigfillset(&SIGTSTP_action.sa_mask);
	// No flags set
	SIGTSTP_action.sa_flags = 0;
	// Install our signal handler
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

void freeStruct(commandPackage* parse) {
	/// <summary>
	/// Iterates through the struct and frees any memory it used on the heap.
	/// </summary>
	free(parse->command);
	for (int i = 0; i < 512; i++) {
		if (parse->arguments[i] != NULL) {
			free(parse->arguments[i]);
		}
		else {
			break;
		}
	}
	free(parse->inputFile);
	free(parse->outputFile);
	free(parse);
}

void getTerminalInput(char* input, int size) {
	/// <summary>
	/// Grabs the string entered by user. Updates the variable created in outer scope.
	/// </summary>
	/// <param name="input">Pointer to variable in outer scope.</param>
	/// <param name="size">Max size of array.</param>
	/// <returns>None</returns>
	char temp[size];
	printf(": ");
	fflush(stdout);
	fgets(temp, size, stdin);
	// Treat empty inputs as 1 space inputs for easier parsing.
	if (strlen(temp) == 0) {
		temp[0] = ' ';
		temp[1] = '\0';
	}
	strcpy(input, temp);
}

void clearNewline(char* string, int length) {
	/// <summary>
	/// Clears the new line character from the end of the string.
	/// </summary>
	/// <param name="string">String to be edited.</param>
	/// <param name="length">Length of string</param>
	/// <returns>None</returns>
	if (string[length - 1] == '\n') {
		string[length - 1] = '\0';
	}
	if (strlen(string) < 1) {
		string[0] = '\0';
	}
}

// Citation for easy function to append a character to end of string
// 1/25/2022
// Adapted from
// https://stackoverflow.com/questions/34055713/how-to-add-a-char-int-to-an-char-array-in-c
void appendChar(char* string, char character) {
	/// <summary>
	/// Function used to append a character to the end of a string.
	/// </summary>
	/// <param name="input">Pointer variable to string.</param>
	/// <param name="size">Character to append to the end of string.</param>
	/// <returns>None, but does modify string located out of this functions scope.</returns>
	int length = strlen(string);
	string[length] = character;
	string[length + 1] = '\0';
}

void pidExpansion(char* string, int length) {
	/// <summary>
	/// Parses a word to determine if it has two $'s in a row. If it does, replaces both symbols with the PID of the shell.
	/// Example: Sample$$ = Sample12345 if the PID was 12345
	/// </summary>
	/// <param name="string">Word to be checked.</param>
	/// <param name="length">Length of the word.</param>
	/// <returns>None, but updates the string to have the PID instead of the $ symbols.</returns>

	// Get the process ID ready to work with
	pid_t processID = getpid();
	char processIDString[10];
	sprintf(processIDString, "%d", processID);

	// Iterate through each character in word to check for possible expansion
	char* tempString = calloc(1, 2048);
	for (int i = 0; i < length; i++) {
		if (string[i] != '$') {
			appendChar(tempString, string[i]);
		}
		else {
			if (string[i + 1] != '$') {  // If not a pid expansion
				appendChar(tempString, string[i]);
			}
			else {
				for (int j = 0; j < strlen(processIDString); j++) {
					appendChar(tempString, processIDString[j]);
				}
				i++;
			}
		}
	}
	strcpy(string, tempString);
	free(tempString);
}

commandPackage* parseInput(char* input, int size) {
	/// <summary>
	/// Parses the string entered using strtok. Calls many helper functions to parse and modify the input.
	/// </summary>
	/// <param name="input">String entered by user.</param>
	/// <param name="size">Size of the command entered in characters.</param>
	/// <returns>A pointer to the struct containing the info.</returns>

	// Initalize struct
	commandPackage* parsedInput = calloc(1, sizeof(commandPackage));
	parsedInput->runBackground = false;

	char* token = NULL;  // used to parse the command
	char* tempToken = NULL;  // used to check PID without disrupting token
	int arg = 0;  // used to keep track of # of arguments
	bool argsAllowed = true;  // flag that is flipped if user tries to prevent user from adding arguments after redirection

	if (input[0] != '\n' && input[0] != '#') {
		// First word is the command
		token = strtok(input, " ");
		// If no command present, kick it back out
		if (token == NULL || strcmp(token, "\n") == 0 || token[0] == '#') {
			free(parsedInput);
			return NULL;
		}
		clearNewline(token, strlen(token));
		tempToken = realloc(tempToken, strlen(token));
		strcpy(tempToken, token);
		pidExpansion(tempToken, strlen(token));
		parsedInput->command = strdup(tempToken);

		// Parse the remainder of string
		while ((token = strtok(NULL, " ")) != NULL) {
			clearNewline(token, strlen(token));
			tempToken = realloc(tempToken, strlen(token));
			strcpy(tempToken, token);
			pidExpansion(tempToken, strlen(token));

			// If word is an argument
			// If argument starts with &, <, or >, check to see if it is part of the argument or not
			// No prevention from being able to change input or output and then have more arguments at the moment
			if (*tempToken == '&') {
				if (strlen(tempToken) == 1) {
					if (backgroundAllowed == true) {
						parsedInput->runBackground = true;
					}
					break;
				}
			}
			else if (*tempToken == '<') {
				if (strlen(tempToken) == 1) {
					token = strtok(NULL, " ");
					clearNewline(token, strlen(token));
					tempToken = realloc(tempToken, strlen(token));
					strcpy(tempToken, token);
					pidExpansion(tempToken, strlen(token));
					parsedInput->inputFile = strdup(tempToken);
					argsAllowed = false;
					continue;
				}
			} else if (*tempToken == '>') {
				if (strlen(tempToken) == 1) {
					token = strtok(NULL, " ");
					clearNewline(token, strlen(token));
					tempToken = realloc(tempToken, strlen(token));
					strcpy(tempToken, token);
					pidExpansion(tempToken, strlen(token));
					parsedInput->outputFile = strdup(tempToken);
					argsAllowed = false;
					continue;
				}
			}
			// Record the argument
			if (argsAllowed) {
				parsedInput->arguments[arg] = strdup(tempToken);
				arg++;
			}
		}
		parsedInput->numArgs = arg;
		free(tempToken);
		return parsedInput;
	}
	else {
		free(parsedInput);
		return NULL;
	}
}

void executeCommand(commandPackage* package, int* status, pid_t* pids, int* pidSize) {
	/// <summary>
	/// Executes the command given to the shell by forking children that run the programs.
	/// </summary>
	/// <param name="package">Pointer to struct containing the packaged information from the entered command.</param>
	/// <param name="status">Pointer to most recent known exit status.</param>
	/// <param name="pids">Pointer to array of PIDs.</param>
	/// <param name="pidSize">Pointer to number of elements in array pids.</param>
	
	int result;  // used in input and output redirection
	bool inputChanged = false;
	bool outputChanged = false;
	int sourceFD;
	int targetFD;

	// Citation for reverting redirection of stdin and stdout
	// 1/27/2022
	// Adapted from
	// https://stackoverflow.com/questions/11042218/c-restore-stdout-to-terminal
	int saved_stdout = dup(1);
	int saved_stdin = dup(2);

	// Redirect STDIN
	if (package->inputFile != NULL) {
		sourceFD = open(package->inputFile, O_RDONLY);
		if (sourceFD == -1) {
			perror("source open() :");
			fflush(stdout);
			*status = 1;
			close(saved_stdout);
			close(saved_stdin);
			return;
		}
		result = dup2(sourceFD, 0);
		if (result == -1) {
			perror("source dup2():");
			fflush(stdout);
			*status = 1;
			close(saved_stdout);
			close(saved_stdin);
			return;
		}
		inputChanged = true;
	}

	// Redirect STDOUT
	if (package->outputFile != NULL) {
		targetFD = open(package->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (targetFD == -1) {
			perror("target open() :");
			fflush(stdout);
			*status = 1;
			if (inputChanged) {
				dup2(0, sourceFD);
			}
			close(saved_stdout);
			close(saved_stdin);
			return;
		}
		result = dup2(targetFD, 1);
		if (result == -1) {
			perror("target dup2():");
			fflush(stdout);
			*status = 1;
			if (inputChanged) {
				dup2(0, sourceFD);
			}
			close(saved_stdout);
			close(saved_stdin);
			return;
		}
		outputChanged = true;
	}

	// Built-in Commands
	if (strcmp("exit", package->command) == 0) {
		int temp = *pidSize;
		for (int m; m < temp; m++) {
			kill(pids[m], SIGKILL);
		}
		raise(SIGKILL);
	}
	else if (strcmp("cd", package->command) == 0) {
		if (package->arguments[0] == NULL) {
			chdir(getenv("HOME"));
		}
		else {
			chdir(package->arguments[0]);
		}
	}
	else if (strcmp("status", package->command) == 0) {
		printf("exit value %d\n", *status);
	}

	else {
		// Repackage the arguments into form suitable for execv
		char* newargv[package->numArgs + 2];  // +2 to allow for first arg to be file path and last arg to be NULL
		memset(&newargv[0], 0, sizeof(newargv));
		newargv[0] = strdup(package->command);
		for (int i = 0; i < package->numArgs; i++) {
			if (package->arguments[i] != NULL) {
				newargv[i + 1] = strdup(package->arguments[i]);
			}
		}

		// Fork child then have child execute the command
		int childStatus;
		pid_t childPid = -5;
		childPid = fork();  // spawnPid = 0 in child, and child's PID in parent
		switch (childPid) {
		case -1:
			perror("fork() failed!");
			fflush(stdout);
			exit(1);
			break;
		case 0:
			// Update signal catching for SIGINT
			if (package->runBackground == false) {
				updateChildSIGINT();
			}

			// Update signal catching for SIGSTP
			updateChildSIGTSTP();

			// Redirect STDIN / STDOUT for background processes
			if (package->runBackground == true) {
				if (inputChanged == false) {
					sourceFD = open("/dev/null", O_RDONLY);
					if (sourceFD == -1) {
						perror("source open() :");
						fflush(stdout);
						*status = 1;
						close(saved_stdout);
						close(saved_stdin);
						return;
					}
					result = dup2(sourceFD, 0);
					if (result == -1) {
						perror("source dup2():");
						fflush(stdout);
						*status = 1;
						close(saved_stdout);
						close(saved_stdin);
						return;
					}
					inputChanged = true;
				}
				if (outputChanged == false) {
					targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (targetFD == -1) {
						perror("target open() :");
						fflush(stdout);
						*status = 1;
						if (inputChanged) {
							dup2(0, sourceFD);
						}
						close(saved_stdout);
						close(saved_stdin);
						return;
					}
					result = dup2(targetFD, 1);
					if (result == -1) {
						perror("target dup2():");
						fflush(stdout);
						*status = 1;
						if (inputChanged) {
							dup2(0, sourceFD);
						}
						close(saved_stdout);
						close(saved_stdin);
						return;
					}
					outputChanged = true;
				}
			}

			execvp(newargv[0], newargv);

			// Returns if there is an error
			perror("execvp");
			fflush(stdout);
			exit(1);
			break;
		default:
			if (package->runBackground == false) {
				int loopVar = -1;
				while (loopVar == -1) {
					loopVar = waitpid(childPid, &childStatus, 0);
					}
				if (childStatus == 256) {  // returns from waitpid with exit code 256 for some reason. internet says it really means '1'
					childStatus = 1;
				}
				if (childStatus == 2) {
					printf("terminated by signal %d\n", childStatus);
				}
				*status = childStatus;
				fflush(stdout);
			}
			else {
				printf("background pid is %d\n", childPid);
				fflush(stdout);
				(*pidSize)++;
				childPid = waitpid(childPid, &childStatus, WNOHANG);  // run in the background
			}
			break;
		}
		for (int k = 0; k < package->numArgs + 2; k++) {
			free(newargv[k]);
		}
	}

	// Reset source and target back to stdin and stdout
	if (inputChanged) {
		dup2(saved_stdin, 0);
	}
	if (outputChanged) {
		dup2(saved_stdout, 1);
	}
	close(saved_stdout);
	close(saved_stdin);
	if (package->runBackground == true) {

	}
}


int main(int argc, char* argv[]) {
	char input[2048];  // need 2049 for null terminator???
	memset(input, 0, sizeof(input));
	int status = 0;  // used to keep track of the most recent known exit status
	pid_t* pids = calloc(8, sizeof(pid_t));
	int pidSize = 0;
	int loopVar = 0;  // needed for loop below so as not to cut off early when pidSize is lowered
	pid_t childPid = -5;
	installSigCatchers();

	while (true) {
		// Check for background process that have ended
		childPid = -5;
		loopVar = pidSize;
		int childStatus;
		for (int i = 0; i < loopVar; i++) {
			childPid = waitpid(-1, &childStatus, WNOHANG);  // -1 checks for any child ready to be released
			if (childPid > 0) {
				printf("background pid %d is done: exit value %d\n", childPid, childStatus);
				fflush(stdout);
				pidSize--;
				childPid = -5;
			}
		}

		commandPackage* parse;
		fflush(stdin);
		getTerminalInput(input, sizeof(input));
		parse = parseInput(input, sizeof(input));
		if (parse != NULL) {  // only attempt to execute command if command exists
			executeCommand(parse, &status, pids, &pidSize);
			freeStruct(parse);
		}
		fflush(stdout);
	}

	return 0;
}