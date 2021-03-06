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

int PID;

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
int builtin_fg(char **args);

char *command_list[] = {
    "cd",
    "exit",
    "jobs",
    "fg",
};

int (*builtin_functions[])(char **) = {
    &builtin_cd,
    &builtin_exit,
    &builtin_jobs,
    &builtin_fg,
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

void remove_job(int index)
{
    shell->jobs[index] = NULL;
}

int get_pid(int id)
{
    if (shell->jobs[id - 1] != NULL)
    {
        return shell->jobs[id - 1]->pid;
    }
    return -1;
}

int builtin_jobs()
{
    int i;
    for (i = 0; i < MAX_JOBS; i++)
    {
        if (shell->jobs[i] != NULL)
        {
            printf("[%d]   [%d]    %s   %s\n", shell->jobs[i]->id, shell->jobs[i]->pid, STATUS_STRING[shell->jobs[i]->mode], shell->jobs[i]->command);
        }
    }
    return 1;
}

int set_job_status(int id, int status)
{
    if (id > MAX_JOBS || shell->jobs[id] == NULL)
    {
        return -1;
    }

    int i;

    return 0;
}

int builtin_fg(char **args)
{
    int job_id;
    pid_t pid;

    if (args[1] == NULL)
    {
        fprintf(stderr, "Sintaxe correta: fg %%id\n");
    }

    if (args[1][0] == '%')
    {
        job_id = atoi(args[1] + 1);
        pid = get_pid(job_id);
        if (pid < 0)
        {
            printf("brash: fg %d: Job não encontrado\n", job_id);
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Sintaxe correta: fg %%id\n");
        return 1;
    }

    if (kill(pid, SIGCONT) < 0)
    {
        printf("brash: Job não encontrado\n");
        return 0;
    }
    remove_job(job_id - 1);
    return 1;
}

int get_next_id()
{
    int i;
    for (i = 0; i < MAX_JOBS; i++)
    {
        if (shell->jobs[i] == NULL)
        {
            return i + 1;
        }
    }
}

void insert_job(struct job *job)
{
    shell->jobs[job->id - 1] = job;
}

void stopped()
{
    if (PID == 0)
    {
        kill(PID, SIGTSTP);
    }
}

void interrupted()
{
    if (PID == 0)
    {
        kill(PID, SIGINT);
    }
}

int launch_process(char **args)
{
    pid_t pid;
    int status;
    int bg;
    int current_cmd_length;
    int erro = 0;

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
    PID = pid;
    if (pid == 0)
    {
        signal(SIGINT, interrupted);
        signal(SIGTSTP, stopped);
        if (execvp(args[0], args) < 0)
        {
            perror("Erro ao executar processo");
            erro = 1;
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("Não foi possível criar o fork");
    }
    else
    {
        int job_id = get_next_id();
        struct job *job = (struct job *)malloc(sizeof(struct job));
        job->command = strdup(args[0]);
        job->id = job_id;
        job->mode = bg;
        job->pid = pid;
        if (!erro)
        {
            insert_job(job);
        }

        if (bg)
        {
            printf("\n%d %s\n", pid, args[0]);
        }
        else
        {
            do
            {
                waitpid(pid, &status, WUNTRACED);
                if (WIFSTOPPED(status))
                {
                    shell->jobs[job_id - 1]->mode = 2;
                    printf("%s %s\n", shell->jobs[job_id - 1]->command, STATUS_STRING[shell->jobs[job_id - 1]->mode]);
                }
                else if (WIFSIGNALED(status))
                {
                    shell->jobs[job_id - 1]->mode = 4;
                    printf("%d %s\n", shell->jobs[job_id - 1]->pid, STATUS_STRING[shell->jobs[job_id - 1]->mode]);
                }
                else if (WIFEXITED(status))
                {
                    remove_job(job_id - 1);
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
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

    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);

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