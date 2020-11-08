#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BACKGROUND_EXECUTION 1
#define FOREGROUND_EXECUTION 0

#define TOKEN_SIZE 64
#define TOKEN_DELIMITER " \t\r\n\a"
#define MAX_JOBS 20

const char *STATUS_STRING[] = {
    "EXECUTANDO",
    "EXECUTADO",
    "SUSPENSO",
    "EM ANDAMENTO",
    "PARADO",
};

struct job
{
    int id;
    char *command;
    pid_t pid;
    int mode;
};

struct shell_info
{
    struct job *jobs[MAX_JOBS];
};

struct shell_info *shell;

int builtin_cd(char **args);
int builtin_exit(char **args);
int builtin_jobs();

char *command_list[] = {
    "cd",
    "exit",
    "jobs",
};

int (*builtin_functions[])(char **) = {
    &builtin_cd,
    &builtin_exit,
    &builtin_jobs,
};

int commands_length()
{
    return sizeof(command_list) / sizeof(char *);
}

int builtin_cd(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "Sintaxe correta: cd nome_diretorio\n");
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("brash");
        }
    }
    return 1;
}

int builtin_exit(char **args)
{
    return 0;
}

void update_process_mode(int pid, int index)
{
    int status = 0;

    waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status))
    {
        shell->jobs[index]->mode = 1;
    }
    else if (WIFSIGNALED(status))
    {
        shell->jobs[index]->mode = 4;
    }
    else if (WSTOPSIG(status))
    {
        shell->jobs[index]->mode = 2;
    }
}

int builtin_jobs()
{
    int i;
    for (i = 0; i < MAX_JOBS; i++)
    {
        if (shell->jobs[i] != NULL)
        {
            update_process_mode(shell->jobs[i]->pid, i);
            printf("[%d]   [%d]    [%s]   [%s]", shell->jobs[i]->id, shell->jobs[i]->pid, STATUS_STRING[shell->jobs[i]->mode], shell->jobs[i]->command);
        }
    }
}

int get_next_id()
{
    int i;
    for (i = 1; i < MAX_JOBS + 1; i++)
    {
        if (shell->jobs[i] == NULL)
        {
            return i;
        }
    }
}

void insert_job(struct job *job)
{
    shell->jobs[job->id - 1] = job;
}

int launch_process(char **args)
{
    pid_t pid;
    int status;
    int bg;
    int current_cmd_length;

    current_cmd_length = 0;
    int i;
    for (i = 0; args[i] != NULL; i++)
    {
        current_cmd_length++;
    }

    //checa se e para o processo rodar em background
    if ((bg = (*args[current_cmd_length - 1] == '&')) != 0)
        args[--current_cmd_length] = NULL;

    pid = fork();
    if (pid == 0)
    {
        if (execvp(args[0], args) < 0)
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
        struct job *job = (struct job *)malloc(sizeof(struct job));
        job->command = args[0];
        job->id = get_next_id();
        job->mode = bg;
        job->pid = pid;
        insert_job(job);

        if (bg)
        {
            printf("\n%d %s\n", pid, args[0]);
        }
        else
        {
            do
            {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }

    return 1;
}

/*Recebe comandos separados em vetor de strings como argumento*/
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
            /*Se for comando builtin, chama funcao builtin*/
            return (*builtin_functions[i])(args);
        }
    }
    /*Se nao for comando builtin chama funcao nao builtin*/
    return launch_process(args);
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

void shell_loop(void)
{
    char *input;
    char **args;
    struct job *job;
    int status;

    do
    {
        printf("brash> ");
        input = read_input();
        args = handle_args(input);
        status = execute(args);

        free(input);
        free(args);
    } while (status);
}

void sigint_handler(int signal)
{
    printf("\n");
}

void init_shell()
{
    struct sigaction sigint_action = {
        .sa_handler = &sigint_handler,
        .sa_flags = 0};

    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    // pid_t pid = getpid();
    // setpgid(pid, pid);
    // tcsetpgrp(0, pid);

    // aloca a memoria pra shell
    shell = (struct shell_info *)malloc(sizeof(struct shell_info));

    // limpa a lista de jobs
    int i;
    for (i = 0; i < MAX_JOBS; i++)
    {
        shell->jobs[i] = NULL;
    }
}

int main(int argc, char **argv)
{
    init_shell();
    shell_loop();

    return EXIT_SUCCESS;
}