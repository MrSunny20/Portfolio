#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

char* arg_list[512];
char current_directory[100];
int num_processes = 0;
int current_processes[1000];
int last_fgp_signal = 0;
int last_fgp_status = 0;
char input_file[256];
int input_file_bool = 0;
char output_file[256];
int output_file_bool = 0;
int foreground_bool = 0;

struct sigaction SIGINT_handler;
struct sigaction SIGTSTP_handler;

void expand_variable(int num_args);
int tokenize_input(char* arg_string);
void cd_command(int num_args);
void exit_command();
void status_command();
void foreground_command(int num_args);
void background_command(int num_args);
void print_background_processes();
void io_redirection(int* num_args);
int validate_input_file();
void SIGTSTP_handler_function(int signo);


void SIGTSTP_handler_function(int signo) {
    (void)signo;
    if (foreground_bool == 0) {
        foreground_bool = 1;
        write(STDOUT_FILENO, "Entering foreground-only mode (& is now ignored)\n",
              strlen("Entering foreground-only mode (& is now ignored)\n"));
    } else {
        foreground_bool = 0;
        write(STDOUT_FILENO, "Exiting foreground-only mode\n",
              strlen("Exiting foreground-only mode\n"));
    }
}

void expand_variable(int num_args) {
    char pid_str[32];
    sprintf(pid_str, "%d", (int)getpid());
    for(int i = 0; i < num_args; i++){
        if (!arg_list[i]){
            continue;
        }
        if (strstr(arg_list[i], "$$") == NULL){
            continue;
        }
        char buffer[2048] = {0};
        char *src = arg_list[i];
        char *dst = buffer;
        while(*src && (dst - buffer) < (int)sizeof(buffer) - 1){
            if(src[0] == '$' && src[1] == '$'){
                size_t L = strlen(pid_str);
                if ((dst - buffer) + (int)L >= (int)sizeof(buffer) - 1) break;
                memcpy(dst, pid_str, L);
                dst += L;
                src += 2;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
        arg_list[i] = strdup(buffer);
    }
}

int tokenize_input(char* arg_string){
    for(int i = 0; i < 32; i++) arg_list[i] = NULL;
    size_t n = strlen(arg_string);
    while(n > 0 && (arg_string[n - 1] == '\n' || arg_string[n - 1] == '\r')){
        arg_string[n - 1] = '\0';
        n--;
    }

    if(arg_string[0] == '\0'){
        return 0;
    }
    if(arg_string[0] == '#'){
        return 0;
    }

    int argc = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(arg_string, " ", &saveptr);

    while(tok != NULL){
        if (argc < 31){
            arg_list[argc++] = tok;
        }
        tok = strtok_r(NULL, " ", &saveptr);
    }
    arg_list[argc] = NULL;
    return argc;
}

void cd_command(int num_args){
    const char *path = NULL;
    if(num_args < 2 || arg_list[1] == NULL){
        path = getenv("HOME");
    }
    else{
        path = arg_list[1];
    }
    if(!path){
        path = ".";
    }
    if(chdir(path) != 0){
        perror("cd");
        fflush(stderr);
    }
}

void exit_command(){
    for(int i = 0; i < num_processes; i++){
        kill((pid_t)current_processes[i], SIGTERM);
    }
    for (int i = 0; i < num_processes; i++){
        int st = 0;
        waitpid((pid_t)current_processes[i], &st, 0);
    }
    exit(0);
}

void status_command(){
    if (last_fgp_signal == 0) {
        printf("exit value %d\n", last_fgp_status);
    } else {
        printf("terminated by signal %d\n", last_fgp_status);
    }
    fflush(stdout);
}

void io_redirection(int* num_args){
    input_file_bool = 0;
    output_file_bool = 0;
    input_file[0] = '\0';
    output_file[0] = '\0';
    int argc = *num_args;
    for(int i = 0; i < argc; i++){
        if (arg_list[i] == NULL){
            break;
        }
        if(strcmp(arg_list[i], "<") == 0 && (i + 1) < argc && arg_list[i + 1]){
            strncpy(input_file, arg_list[i + 1], sizeof(input_file) - 1);
            input_file[sizeof(input_file) - 1] = '\0';
            input_file_bool = 1;
            for (int k = i; k + 2 <= argc; k++){
                arg_list[k] = arg_list[k + 2];
            }
            argc -= 2;
            i--;
        }
        else if(strcmp(arg_list[i], ">") == 0 && (i + 1) < argc && arg_list[i + 1]){
            strncpy(output_file, arg_list[i + 1], sizeof(output_file) - 1);
            output_file[sizeof(output_file) - 1] = '\0';
            output_file_bool = 1;
            for(int k = i; k + 2 <= argc; k++){
                arg_list[k] = arg_list[k + 2];
            }
            argc -= 2;
            i--;
        }
    }

    arg_list[argc] = NULL;
    *num_args = argc;
}

int validate_input_file() {
    if (!input_file_bool) return 1;

    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        fprintf(stderr, "cannot open %s for input\n", input_file);
        fflush(stderr);
        last_fgp_signal = 0;
        last_fgp_status = 1;
        return 0;
    }
    fclose(fp);
    return 1;
}

void print_background_processes(){
    int st = 0;
    pid_t pid;

    while((pid = waitpid(-1, &st, WNOHANG)) > 0){
        printf("background pid %d is done: ", pid);

        if(WIFEXITED(st)){
            printf("exit value %d\n", WEXITSTATUS(st));
        }else if (WIFSIGNALED(st)){
            printf("terminated by signal %d\n", WTERMSIG(st));
        }else {
            printf("exit value 0\n");
        }
        fflush(stdout);

        for(int i = 0; i < num_processes; i++){
            if(current_processes[i] == (int)pid){
                current_processes[i] = current_processes[num_processes - 1];
                num_processes--;
                break;
            }
        }
    }
}

void foreground_command(int num_args){
    (void)num_args;

    if (!validate_input_file()){
        return;
    }
    pid_t pid = fork();
    if(pid == -1){
        perror("fork");
        fflush(stderr);
        last_fgp_signal = 0;
        last_fgp_status = 1;
        return;
    }

    if(pid == 0){
        struct sigaction sa_tstp = {
            0
        };
        sa_tstp.sa_handler = SIG_IGN;
        sigaction(SIGTSTP, &sa_tstp, NULL);

        struct sigaction sa_int = {
            0
        };
        sa_int.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sa_int, NULL);

        if(input_file_bool){
            FILE *fp = fopen(input_file, "r");
            if(!fp){
                fprintf(stderr, "cannot open %s for input\n", input_file);
                fflush(stderr);
                exit(1);
            }
            dup2(fileno(fp), STDIN_FILENO);
            fclose(fp);
        }
        if(output_file_bool){
            FILE *fp = fopen(output_file, "w");
            if(!fp){
                fprintf(stderr, "cannot open %s for output\n", output_file);
                fflush(stderr);
                exit(1);
            }
            dup2(fileno(fp), STDOUT_FILENO);
            fclose(fp);
        }
        execvp(arg_list[0], arg_list);
        fprintf(stderr, "%s: no such file or directory\n", arg_list[0]);
        fflush(stderr);
        exit(1);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    if(WIFEXITED(st)){
        last_fgp_signal = 0;
        last_fgp_status = WEXITSTATUS(st);
    }
    else if(WIFSIGNALED(st)){
        last_fgp_signal = 1;
        last_fgp_status = WTERMSIG(st);
        status_command();
    }
    else{
        last_fgp_signal = 0;
        last_fgp_status = 0;
    }
}

void background_command(int num_args) {
    (void)num_args;

    if (!validate_input_file()){
        return;
    }

    pid_t pid = fork();
    if (pid == -1){
        perror("fork");
        fflush(stderr);
        last_fgp_signal = 0;
        last_fgp_status = 1;
        return;
    }

    if (pid == 0){
        struct sigaction sa_tstp = {0};
        sa_tstp.sa_handler = SIG_IGN;
        sigaction(SIGTSTP, &sa_tstp, NULL);

        struct sigaction sa_int = {0};
        sa_int.sa_handler = SIG_IGN;
        sigaction(SIGINT, &sa_int, NULL);

        if(!input_file_bool){
            FILE *fp = fopen("/dev/null", "r");
            if(fp){
                dup2(fileno(fp), STDIN_FILENO); fclose(fp);
            }
        }
        if(!output_file_bool){
            FILE *fp = fopen("/dev/null", "w");
            if(fp){
                dup2(fileno(fp), STDOUT_FILENO); fclose(fp);
            }
        }
        if(input_file_bool){
            FILE *fp = fopen(input_file, "r");
            if(!fp){
                fprintf(stderr, "cannot open %s for input\n", input_file);
                fflush(stderr);
                exit(1);
            }
            dup2(fileno(fp), STDIN_FILENO);
            fclose(fp);
        }
        if(output_file_bool){
            FILE *fp = fopen(output_file, "w");
            if(!fp){
                fprintf(stderr, "cannot open %s for output\n", output_file);
                fflush(stderr);
                exit(1);
            }
            dup2(fileno(fp), STDOUT_FILENO);
            fclose(fp);
        }
        execvp(arg_list[0], arg_list);
        fprintf(stderr, "%s: no such file or directory\n", arg_list[0]);
        fflush(stderr);
        exit(1);
    }
    printf("background pid is %d\n", pid);
    fflush(stdout);
    if(num_processes < 1000){
        current_processes[num_processes++] = (int)pid;
    }
}

int main(void){
    memset(&SIGINT_handler, 0, sizeof(SIGINT_handler));
    SIGINT_handler.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_handler.sa_mask);
    SIGINT_handler.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_handler, NULL);

    memset(&SIGTSTP_handler, 0, sizeof(SIGTSTP_handler));
    SIGTSTP_handler.sa_handler = SIGTSTP_handler_function;
    sigfillset(&SIGTSTP_handler.sa_mask);
    SIGTSTP_handler.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_handler, NULL);

    last_fgp_signal = 0;
    last_fgp_status = 0;

    char line[512 + 2];

    while (1) {
        print_background_processes();

        printf(": ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            exit_command();
        }

        int num_args = tokenize_input(line);
        if (num_args == 0) continue;

        expand_variable(num_args);

        if (strcmp(arg_list[0], "exit") == 0) {
            exit_command();
        } else if (strcmp(arg_list[0], "cd") == 0) {
            cd_command(num_args);
            continue;
        } else if (strcmp(arg_list[0], "status") == 0) {
            status_command();
            continue;
        }

        io_redirection(&num_args);

        int bg = 0;
        if (num_args > 0 && arg_list[num_args - 1] &&
            strcmp(arg_list[num_args - 1], "&") == 0) {
            arg_list[num_args - 1] = NULL;
            num_args--;
            if (foreground_bool == 0) bg = 1;
        }

        if (bg) background_command(num_args);
        else foreground_command(num_args);
    }

    return 0;
}