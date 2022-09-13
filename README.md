# smallsh
This was the portfolio project for CS344 Operating Systems. We were tasked to write our own shell in C, while providing it with some basic / expected features of a shell program.

## Program Functionality
Below are the requirements for the program as outlined by the portfolio assignment. My implemention meets each requirement.

### 1. Command Prompt
Use of a colon (:) symbol as a prompt for each command line. The general syntax of a command line is:

    command [arg1 arg2 ...] [< input_file] [>output_file] [&]
    
Items in brackets are considered optional. The "&" symbol is used to denote if a program should operate in the background. Program supports a maximum of 512 arguments.

### 2. Comments & Blank Lines
The program must support blank lines and lines commented out with "#". After inputing either, shell with re-prompt for a command.

### 3. Expansion of Variable $$
In the shell, the variable $$ returns the process ID of the shell itself.

### 4. Built-in Commands
There are three built-in commands: "exit", "cd", and "status". They work how expected:
  - "exit" takes no arguments and exits the shell
  - "cd" takes 0 or 1 argument. If given no arguments, it changes directory to the HOME environment variable. If given 1 argument, it changes path to the given directory.
    - supports both relative and absolute paths
  - "status" prints the exit status or the terminating signal of the last foreground process run by the shell

### 5. Execute Other Commands
Shell supports running any other program a shell would be able to run through the use of fork() and exec(). Anytime a program is called, the parent process will fork() a child who will then call the function using exec(). On error, sets exit status to 1. After finishing its program, a child process must terminate.

### 6. Input & Output Redirection
As mentioned earlier with the syntax of the command line.

### 7. Execute Commands in Foreground & Background
After a program is specified to run in the background, the control is immediately returned to the user to operate another command. The application regularely loops over child processes scanning for finished programs and removing them.

### 8. Signals SIGINT & SIGTSTP
The shell includes the ability to trigger a SIGINT signal by hitting CTRL-C. This signal is ignored by the parent (shell) process and all background child processes, and terminates whatever foreground child process is happening. The program can also trigget SIGTSTP with CTRL-Z which turns on a "no background processes" mode. Hitting CTRL-Z again will restore the ability to run background processes.
