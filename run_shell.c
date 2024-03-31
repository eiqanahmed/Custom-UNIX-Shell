
#include "shell.h"

#include <unistd.h> 

int cd_cscshell(const char *target_dir) {
    if (target_dir == NULL) {
        uid_t uid = getuid();
        if (uid < 0) {
            perror("run_command");
            return -1;
        }
        struct passwd *pw_data = getpwuid(uid);
        if (pw_data == NULL) {
            perror("run_command");
            return -1;
        }
        target_dir = pw_data->pw_dir;
    }

    if (chdir(target_dir) < 0) {
        perror("cd_cscshell");
        return -1;
    }
    return 0;
}



int *execute_line(Command *head){

    if (head == NULL) {
        return NULL; // No commands to execute.
    }

    int *result = malloc(sizeof(int));
    if (result == NULL) {
        perror("malloc");
        return NULL; 
    }
    *result = 0; // Initialize result

    int lastInput = STDIN_FILENO, fd[2];

    for (Command *current = head; current; current = current->next) {
        if (current->next && pipe(fd) == -1) {
            perror("pipe");
            *result = -1;
            break;
        }

        // Special handling for "cd" command, if present
        if (strcmp(current->exec_path, "cd") == 0) {
            *result = cd_cscshell(current->args[1]); 
            continue; // Move to next command or finish.
        }

        current->stdin_fd = lastInput;

        if (current->next) {
            current->stdout_fd = fd[1];
        } else {
            current->stdout_fd = STDOUT_FILENO;
        }

        *result = run_command(current);

        if (lastInput != STDIN_FILENO) {
            close(lastInput);
        }

        lastInput = fd[0];

        if (!current->next || *result != 0) {
            if (current->next) {
                close(fd[0]);
                close(fd[1]);
            }
            break;
        } else {
            close(fd[1]);
        }
    }

    if (lastInput != STDIN_FILENO) {
        close(lastInput);
    }

    return result; 

    
    #ifdef DEBUG
    printf("\n***********************\n");
    printf("BEGIN: Executing line...\n");
    #endif

    #ifdef DEBUG
    printf("All children created\n");
    #endif

    // Wait for all the children to finish

    #ifdef DEBUG
    printf("All children finished\n");
    #endif

    #ifdef DEBUG
    printf("END: Executing line...\n");
    printf("***********************\n\n");
    #endif

    // return NULL;
}


/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
int run_command(Command *command){

    int pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) { // Child process
        if (command->stdin_fd != STDIN_FILENO) {
            dup2(command->stdin_fd, STDIN_FILENO);
            close(command->stdin_fd);
        }

        if (command->stdout_fd != STDOUT_FILENO) {
            dup2(command->stdout_fd, STDOUT_FILENO);
            close(command->stdout_fd);
        }

        execv(command->exec_path, command->args);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    // Parent process
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1; 


    #ifdef DEBUG
    printf("Running command: %s\n", command->exec_path);
    printf("Argvs: ");
    if (command->args == NULL){
        printf("NULL\n");
    }
    else if (command->args[0] == NULL){
        printf("Empty\n");
    }
    else {
        for (int i=0; command->args[i] != NULL; i++){
            printf("%d: [%s] ", i+1, command->args[i]);
        }
    }
    printf("\n");
    printf("Redir out: %s\n Redir in: %s\n",
           command->redir_out_path, command->redir_in_path);
    printf("Stdin fd: %d | Stdout fd: %d\n",
           command->stdin_fd, command->stdout_fd);
    #endif


    #ifdef DEBUG
    printf("Parent process created child PID [%d] for %s\n", pid, command->exec_path);
    #endif
}


int run_script(char *file_path, Variable **root){
    //file_path = "./cscshell_init";
    //handle case where no path is defined in init script

    FILE *file = fopen(file_path, "r"); // Open the script file for reading
    if (file == NULL) {
        ERR_PRINT(ERR_INIT_SCRIPT, file_path);
        return -1; // Return error if the file cannot be opened
    }

    char line[MAX_SINGLE_LINE]; 
    int *exec_result;

    while (fgets(line, sizeof(line), file) != NULL) { // Read the file line by line
        Command *commands = parse_line(line, root);

        if (commands == (Command *) -1){
            ERR_PRINT(ERR_PARSING_LINE);
            fclose(file);
            return -1;
    }

    if (commands != NULL) { // If there are commands to execute
        exec_result = execute_line(commands);

        if (exec_result == (int*)-1 || exec_result == NULL) {
            ERR_PRINT(ERR_EXECUTE_LINE);
            free(exec_result);
                fclose(file); // Close the file before returning
                return -1;
        }
        free(exec_result); // Free the allocated result
    }

    free_command(commands);
    }
    fclose(file); // Close the file after processing all lines
    return 0;
}

void free_command(Command *command){
    
    if (command == NULL) {
        return;
    }

    free(command->exec_path);

    // Free each argument in the args array
    if (command->args != NULL) {
        char **arg = command->args;
        while (*arg) {
            free(*arg); // Free each argument string
            arg++; // Move to the next argument
        }
        free(command->args); // Free the array of pointers itself
    }

    free(command->redir_in_path);

    free(command->redir_out_path);

    free(command);
}
