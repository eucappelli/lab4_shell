#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TOKEN_SIZE 64
#define TOKEN_DELIMITER " \t\r\n\a"

int builtin_cd(char **args);
int builtin_exit(char **args);

char *command_list[] = {
    "cd",
    "exit",
};

int (*builtin_functions[])(char **) = {
    &builtin_cd,
    &builtin_exit,
};

int commands_length()
{
    return sizeof(command_list) / sizeof(char *);
}

int builtin_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "É necessário passar um argumento para \"cd\"\n");
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("Não foi possível abrir o diretório");
        }
    }
    return 1;
}

int builtin_exit(char **args)
{
    return 0;
}

int launch_process(char **args)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0)
    {
        if (execvp(args[0], args) == -1)
        {
            perror("Erro ao executar processo");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("Não foi possível criar o fork");
    }
    else
    {
        do
        {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int execute(char **args)
{
    int i;

    if (args[0] == NULL)
    {
        return 1;
    }

    for (i = 0; i < commands_length(); i++)
    {
        if (strcmp(args[0], command_list[i]) == 0)
        {
            return (*builtin_functions[i])(args);
        }
    }

    return launch_process(args);
}

char *read_input(void)
{
    char *input = NULL;
    ssize_t bufsize = 0;

    if (getline(&input, &bufsize, stdin) == -1)
    {
        if (feof(stdin))
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }

    return input;
}

char **handle_args(char *input)
{
    int bufsize = TOKEN_SIZE, index = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token, **tokens_backup;

    if (!tokens)
    {
        fprintf(stderr, "Erro de alocação\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(input, TOKEN_DELIMITER);
    while (token != NULL)
    {
        tokens[index] = token;
        index++;

        if (index >= bufsize)
        {
            bufsize += TOKEN_SIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                free(tokens_backup);
                fprintf(stderr, "Erro de alocação\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOKEN_DELIMITER);
    }
    tokens[index] = NULL;
    return tokens;
}

void shell_loop(void)
{
    char *input;
    char **args;
    int status;

    do
    {
        printf("shellvpower> ");
        input = read_input();
        args = handle_args(input);
        status = execute(args);

        free(input);
        free(args);
    } while (status);
}

int main(int argc, char **argv)
{
    shell_loop();

    return EXIT_SUCCESS;
}