#include "shell.h"

#define CONTINUE_SEARCH NULL

#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h> 


char *resolve_executable(const char *command_name, Variable *path){

    if (command_name == NULL || path == NULL){
        return NULL;
    }

    if (strcmp(command_name, CD) == 0){
        return strdup(CD);
    }

    if (strcmp(path->name, PATH_VAR_NAME) != 0){
        ERR_PRINT(ERR_NOT_PATH);
        return NULL;
    }

    char *exec_path = NULL;

    if (strchr(command_name, '/')){
        exec_path = strdup(command_name);
        if (exec_path == NULL){
            perror("resolve_executable");
            return NULL;
        }
        return exec_path;
    }

    // we create a duplicate so that we can mess it up with strtok
    char *path_to_toke = strdup(path->value);
    if (path_to_toke == NULL){
        perror("resolve_executable");
        return NULL;
    }
    char *current_path = strtok(path_to_toke, ":");

    do {
        DIR *dir = opendir(current_path);
        if (dir == NULL){
            ERR_PRINT(ERR_BAD_PATH, current_path);
            closedir(dir);
            continue;
        }

        struct dirent *possible_file;

        while (exec_path == NULL) {
            // rare case where we should do this -- see: man readdir
            errno = 0;
            possible_file = readdir(dir);
            if (possible_file == NULL) {
                if (errno > 0){
                    perror("resolve_executable");
                    closedir(dir);
                    goto res_ex_cleanup;
                }
                // end of files, break
                break;
            }

            if (strcmp(possible_file->d_name, command_name) == 0){
                // +1 null term, +1 possible missing '/'
                size_t buflen = strlen(current_path) +
                    strlen(command_name) + 1 + 1;
                exec_path = (char *) malloc(buflen);
                // also sets remaining buf to 0
                strncpy(exec_path, current_path, buflen);
                if (current_path[strlen(current_path)-1] != '/'){
                    strncat(exec_path, "/", 2);
                }
                strncat(exec_path, command_name, strlen(command_name)+1);
            }
        }
        closedir(dir);

        // if this isn't null, stop checking paths
        if (possible_file) break;

    } while ((current_path = strtok(CONTINUE_SEARCH, ":")));

res_ex_cleanup:
    free(path_to_toke);
    return exec_path;
}
// RESOLVE_EXECUTABLE ENDS ON THE LINE ABOVE

// helper for parse_line
int isValidVarChar(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

//helper to create a Variable struct: 

Variable *createVariable(char *name, char *value) {
    // Allocate memory for the new variable
    Variable *newVar = (Variable *)malloc(sizeof(Variable));
    if (newVar == NULL) {
        fprintf(stderr, "Failed to allocate memory for new variable.\n");
        return NULL;
    }

    // Allocate and copy the name
    newVar->name = strdup(name);
    if (newVar->name == NULL) {
        fprintf(stderr, "Failed to allocate memory for variable name.\n");
        free(newVar); // Clean up previously allocated memory
        return NULL;
    }

    // Allocate and copy the value
    newVar->value = strdup(value);
    if (newVar->value == NULL) {
        fprintf(stderr, "Failed to allocate memory for variable value.\n");
        free(newVar->name);
        free(newVar); // Clean up previously allocated memory
        return NULL;
    }

    // Set the next pointer to NULL
    newVar->next = NULL;

    return newVar;
}

// MAKE SURE TO SET PATH TO BE THE HEAD OF THE LINKEDLIST FIRST
// Function to add or update a variable in a list:
int addOrUpdateVariable(Variable **variables, char *name, char *value) {

    if (variables == NULL || name == NULL || value == NULL) {
        ERR_PRINT(ERR_NOT_PATH);
        return -1;

    }

    Variable *current = *variables;
    Variable *prev = NULL;

    // Handle empty list
    if (current == NULL) {
        Variable *newVar = createVariable(name, value);
        if (newVar == NULL) {
            return -1; // Memory allocation failed
        }
        *variables = newVar; // Update the head of the list // Simulate delay
        return 0; // Success
    }


    if (strcmp(name, "PATH") == 0) {
        if (strcmp(current->name, "PATH") != 0) {
            Variable *newVar = createVariable(name, value);
            if (newVar == NULL) {
                return -1;
            }
            newVar->next = *variables;
            return 0;
        } else {
            // Here, the head of the list is the PATH variable
            free(current->value);
            current->value = strdup(value);
            return current->value ? 0 : -1;
        }
    }
    //empty variables list

    // Traverse the list to find if the variable already exists
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // Variable found, update its value
            free(current->value); // Free the old value
            current->value = strdup(value); // Assign the new value
            return current->value ? 0 : -1; // Check strdup success
        }
        prev = current; // Keep track of the previous node
        current = current->next; // Move to the next node in the list
    }

    // Variable not found, add a new variable to the list
    Variable *newVar = createVariable(name, value);
    if (newVar == NULL) {
        return -1;
    }
    if (prev) { // If list was not empty
        prev->next = newVar;
    } else { // List was empty, should not happen due to earlier check, but added for robustness
        *variables = newVar;
    }

    return 0;

}

// Checks to see if a line starts with an equal sign for variable name
int startsWithEqualSign(char *line) {
    // Check if the line is not NULL and the first character is '='
    if (line != NULL && line[0] == '=') {
        return 1; // True, line starts with '='
    }
    return 0; // False, line does not start with '='
}


int defines_path_variable(char *line) {
    // Look for "PATH=" at the start of the line
    const char *pathPrefix = "PATH=";
    
    // Check if the line starts with "PATH="
    if (strncmp(line, pathPrefix, strlen(pathPrefix)) == 0) {
        // Line defines the PATH variable
        return 1;
    }
    
    // Line does not define the PATH variable
    return 0;
}



Command *parse_line(char *line, Variable **variables){

// Check for empty string
if (line[0] == '\0') {
    return NULL;
}

// The string only has white spaces
// Check to see if the line is exclusively a comment
int j = 0;
while (line[j] != '\0' && isspace((unsigned char)line[j])) {
        j++;
    }

    // Check if the line is empty or a comment
    if (line[j] == '\0' || line[j] == '#') {
        // It's either an empty line or a comment line, return NULL as specified
        return NULL;
    }

// Check to see if a line is a variable assignment:
if (startsWithEqualSign(line)) {
    printf(ERR_VAR_START);
    return (Command *)-1;
}

//Defining PATH Variable
if (defines_path_variable(line) == 1) {
    const char prefix[] = "PATH=";
    char*s = "PATH";
    char *pathValue = line + strlen(prefix); // Get the substring after "PATH="
    int check = addOrUpdateVariable(variables, s, pathValue);
    if (check == -1) {
        return (Command *)-1;
    }
}

char *equalsPtr = strchr(line, '=');
if (equalsPtr != NULL) {
    char *temp = line;

    // Check to see if the variable name is valid
    while (temp != equalsPtr) {
        if (!isValidVarChar(*temp)) {
            printf(ERR_VAR_NAME, temp); // Invalid character in variable name
            return (Command *)-1; 
        }
        temp++;
    }

    *equalsPtr = '\0'; // Temporarily terminate the string to isolate the name
    char *name = line;
    char *value = equalsPtr + 1;


    // Create new Variable and add new variable to linked-list.
    int check2 = addOrUpdateVariable(variables, name, value);
    if (check2 == -1) {
        return (Command*)-1;
    }

    return NULL;
}
// If there is a list of commands: 
// Head is type Command** resresenting the whole linked list, current should point to a single command
Command *head = NULL;
Command **current = &head;
char* new_line = replace_variables_mk_line(line, *variables);
if (new_line == NULL || new_line == (char*)-1) {
    fprintf(stderr, "There was an error with replace_variables");
    free(new_line);
    return (Command *)-1;
}
//curr points to the beginning of the new line
char* curr = new_line;

char* pipe_index = curr;

// main while loop
while (*curr != '\0') {

    int arg_count = 0;

    Command *cmd = calloc(1, sizeof(Command));
    if (!cmd) {
        perror("Failed to allocate memory for Command");
        return (Command*)-1;
    }
    *current = cmd;

    while (*curr && isspace((unsigned char)*curr)) curr++;
    if (!(isValidVarChar(*curr)) || *curr == '|' || *curr == '>' || *curr == '<') {
        ERR_PRINT(ERR_PARSING_LINE);
        return (Command *)-1;
}
// Curr points to the first letter of a executable

    pipe_index = curr;
    while (*pipe_index != '|' && *pipe_index != '\0') {
        pipe_index++;
    }
    // pipe_index now points to a pipe symbol or null terminator

    char* exec_start = curr;

    while (*curr != ' ' && *curr != '>' && *curr != '<') {
        curr++;
    }
    //curr now points to the char after the last char of a executable
    char* exec_name = strndup(exec_start, (curr - exec_start));
    if (exec_name == NULL) {
        perror("strndup");
        return (Command*)-1;
    }

    cmd->exec_path = resolve_executable(exec_name, variables[0]);
    if (cmd->exec_path == NULL) {
        ERR_PRINT(ERR_BAD_PATH, exec_name);
        return (Command*)-1;
    }
    // check if the call above is NULL and handle it
    cmd->args = calloc(2, sizeof(char*));
    if (cmd->args == NULL) {
        perror("calloc");
        return (Command*)-1;
    }
    cmd->args[0] = strdup(exec_name);
    cmd->stdin_fd = STDIN_FILENO;
    cmd->stdout_fd = STDOUT_FILENO;
    cmd->redir_in_path = NULL;
    cmd->redir_out_path = NULL;
    cmd->redir_append = 0;
    cmd->next = NULL;

    arg_count = 1;  
    // per command loop
    // After one iteration of the following loop, both curr and pipe_index should
    // point to the space after a pipe symbol
    
    while (curr < pipe_index) {

    if (*curr == '>') {
        if (*(curr + 1) == '>') {
            curr += 2;
        while (*curr && isspace((unsigned char)*curr)) curr++;
        if (*curr == '\0' || curr == pipe_index || *curr == '<' || *curr == '#' || *curr == '>') {
            ERR_PRINT(ERR_PARSING_LINE);
            return (Command*)-1;
        }
        char* redirout_start = curr;
        // curr points to a valid character of a filename
        while (*curr != ' ' && *curr != '>' && *curr != '<') {
        curr++;
    }
    if (curr == pipe_index) {
        ERR_PRINT(ERR_PARSING_LINE);
        return (Command*)-1;
    }
        char* outputredir_name = strndup(redirout_start, (curr - redirout_start));
        if (outputredir_name == NULL) {
            perror("strndup");
            return (Command*)-1;
        }

        cmd->redir_append = (uint8_t)1;
        cmd->redir_out_path = outputredir_name;

            
        } else {
        curr += 1;
        while (*curr && isspace((unsigned char)*curr)) curr++;
        if (*curr == '\0' || curr == pipe_index || *curr == '<' || *curr == '#' || *curr == '>') {
            ERR_PRINT(ERR_PARSING_LINE);
            return (Command*)-1;
        }
        char* redir_out_start = curr;
        // curr points to a valid first character of a filename
        while (*curr != ' ' && *curr != '>' && *curr != '<' && curr != pipe_index) {
        curr++;
    }
    if (curr == pipe_index) {
        ERR_PRINT(ERR_PARSING_LINE);
        return (Command*)-1;
    }
    // curr now points to the character after the last char of a filename
    char* outputredir_name = strndup(redir_out_start, (curr - redir_out_start));
    if (outputredir_name == NULL) {
            perror("strndup");
            return (Command*)-1;
        }

    cmd->redir_out_path = outputredir_name;
    }
    } else if (*curr == '<') {
        curr += 1;
        while (*curr && isspace((unsigned char)*curr)) curr++;
        if (*curr == '\0' || curr == pipe_index || *curr == '<' || *curr == '#' || *curr == '>') {
            ERR_PRINT(ERR_PARSING_LINE);
            return (Command*)-1;
        }

        char* redirin_start = curr;

        // curr points to a valid character of a filename
        while (*curr != ' ' && *curr != '>' && *curr != '<') {
        curr++;
        }
        if (curr == pipe_index) {
            ERR_PRINT(ERR_PARSING_LINE);
            return (Command*)-1;
        }

        // curr now points to the character after the last char of a filename
        char* inputredir_name = strndup(redirin_start, (curr - redirin_start));
        if (inputredir_name == NULL) {
            perror("strndup");
            return (Command*)-1;
        }

        cmd->redir_in_path = inputredir_name;
        
    } else if (*curr == '#') {
        *curr = '\0'; 
        return head;
    } else if (*curr != ' ') {
        // This case takes care of args, curr is now pointing to the first char of an arg
        char* arg_start = curr;

        while (*curr != ' ' && *curr != '>' && *curr != '<') {
        curr++;
        }

        // curr now points to the character after the last char of an arg
        // it could be the pipe_index

        char* arg_name = strndup(arg_start, (curr - arg_start));
        if (arg_name == NULL) {
            perror("strndup");
            return (Command*)-1;
        }

        cmd->args = realloc(cmd->args, (arg_count + 2) * sizeof(char*));
        if (cmd->args == NULL) {
            perror("realloc");
            return (Command*)-1;
        }

        cmd->args[arg_count] = arg_name;
        arg_count += 1;

    }

    curr++;
}
// does args have to be null-terminated?
cmd->args[arg_count] = NULL;

current = &((*current)->next);
curr = pipe_index + 1;

}
    sleep(1);
    return head;

}

// Helper that returns the variable value given its name
char* find_value_from_name(char* name, Variable *variables) {
    Variable *current = variables;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current->value;
        }
        current = current->next;
    }
    ERR_PRINT(ERR_VAR_NOT_FOUND, name);
    return NULL;

}

/*
** This function is partially implemented for you, but you may
** scrap the implementation as long as it produces the same result.
**
** Creates a new line on the heap with all named variable *usages*
** replaced with their associated values.
**
** Returns NULL if replacement parsing had an error, or (char *) -1 if
** system calls fail and the shell needs to exit.
*/
char *replace_variables_mk_line(const char *line,
                                Variable *variables){

    if (line == NULL) {
        return NULL;
    }

    int length = strlen(line) + 1;
    char *new_line = (char *)malloc(length);
    if (new_line == NULL) {
        perror("replace_variables_mk_line");
        return (char *) -1;
    }
    new_line[0] = '\0';
    int i = 0;

    while (line[i] != '\0') {
        if (line[i] == '$' && line[i + 1] == '{') {
            if (line[i + 2] == '=') {
                ERR_PRINT(ERR_VAR_START);
                return NULL;
            }

            int j = i + 2;
            while (line[j] != '}' && line[j] != '\0') {
                if (!isValidVarChar(line[j])) {
                    ERR_PRINT(ERR_VAR_NAME, &(line[j]));
                    return NULL;
                }
                j++;
            }

            if (line[j] == '\0') { // Unmatched '{'
                ERR_PRINT(ERR_VAR_USAGE, line);
                free(new_line);
                return NULL;
            }

            char *var_name = strndup(line + i + 2, j - (i + 2));

            if (var_name == NULL) {
                perror("strndup");
                return (char*)-1;
            }
            char *var_value = find_value_from_name(var_name, variables);
            free(var_name);

            if (var_value == NULL) { // Variable not found
                free(new_line);
                return NULL;
            }

            int new_line_length = strlen(new_line) + strlen(var_value) + 1;
            char *temp = realloc(new_line, new_line_length);
            if (temp == NULL) {
                free(new_line);
                perror("realloc failed");
                return (char *)-1;
            }
            new_line = temp;
            strcat(new_line, var_value);

            i = j + 1; // Move past the '}'

        } else if (line[i] == '$') {
            if (line[i + 1] == '=') {
                ERR_PRINT(ERR_VAR_START);
                return NULL;
            }

            int j = i + 1;
            // implement this
            while (line[j] != ' ' && line[j] != '.' && line[j] != '\0') {
                if (!isValidVarChar(line[j])) {
                    ERR_PRINT(ERR_VAR_NAME, &(line[j]));
                    return NULL;
                }
                j++;
            }
            char *var_name = strndup(line + i + 1, j - (i + 1));
            if (var_name == NULL) {
                perror("strndup");
                return (char*)-1;
            }
            char *var_value = find_value_from_name(var_name, variables);
            free(var_name);

            if (var_value == NULL) { // Variable not found
                free(new_line);
                return NULL;
            }

            int new_line_length = strlen(new_line) + strlen(var_value) + 1;
            char *temp = realloc(new_line, new_line_length);
            if (temp == NULL) {
                free(new_line);
                perror("realloc failed");
                return (char *)-1;
            }
            new_line = temp;
            strcat(new_line, var_value);

            i = j + 1; // Move past the white space or dot

        } else {
            int new_line_len = strlen(new_line);
            new_line[new_line_len] = line[i];
            new_line[new_line_len + 1] = '\0';
            i++;
        }
    }
    return new_line;
}


void free_variable(Variable *var, uint8_t recursive){

while (var != NULL) {
        Variable *next = var->next; // Save the next pointer before freeing

        // Free the memory allocated for name and value strings
        free(var->name);
        free(var->value);

        // Free the current Variable structure itself
        free(var);

        if (recursive == 0) {
            break;
        }
        // Move to the next variable in the list
        var = next;
    }

}
