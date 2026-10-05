#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>

#define MAXLINES 1000
#define BUFSIZE 512
#define TIMEOUT 5

volatile sig_atomic_t timed_out = 0;   // ставится в обработчике, проверяется в main

// обработчик SIGALRM, только ставит флаг (printf тут вызывать нельзя)
void on_alarm(int sig)
{
    timed_out = 1;
}

int main(int argc, char *argv[])
{
    int fd;
    off_t offs[MAXLINES];
    int lens[MAXLINES];
    int n = 0;
    char buf[BUFSIZE];
    ssize_t got;
    off_t pos = 0;
    off_t start = 0;
    int i;
    long num;
    char *line;
    char in[64];        // сюда читаем то что ввел пользователь
    char *end;
    struct sigaction sa;

    if (argc != 2) {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    while ((got = read(fd, buf, BUFSIZE)) > 0) {
        for (i = 0; i < got; i++) {
            if (buf[i] == '\n') {
                if (n >= MAXLINES) {
                    printf("слишком много строк\n");
                    return 1;
                }
                offs[n] = start;
                lens[n] = pos - start;
                n++;
                start = pos + 1;
            }
            pos++;
        }
    }
    if (got == -1) {
        perror("read");
        return 1;
    }
    if (start < pos && n < MAXLINES) {
        offs[n] = start;
        lens[n] = pos - start;
        n++;
    }

    // sa_flags = 0, без SA_RESTART: нужно чтобы read прервался по сигналу, а не перезапустился
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    while (1) {
        printf("Номер строки (0 - выход), есть %d секунд: ", TIMEOUT);
        fflush(stdout);   // без этого приглашение застрянет в буфере, т.к. нет '\n'

        alarm(TIMEOUT);                        // завели таймер
        got = read(0, in, sizeof(in) - 1);     // 0 это стандартный ввод
        alarm(0);                              // успел ввести - таймер снимаем

        if (got == -1) {
            if (errno == EINTR && timed_out) {   // read прервал наш сигнал
                printf("\nвремя вышло, весь файл:\n");
                fflush(stdout);
                lseek(fd, 0, SEEK_SET);
                while ((got = read(fd, buf, BUFSIZE)) > 0)
                    write(1, buf, got);
                break;
            }
            perror("read");
            return 1;
        }
        if (got == 0)   // ctrl+D
            break;

        in[got] = '\0';
        num = strtol(in, &end, 10);
        if (end == in || (*end != '\n' && *end != '\0')) {
            printf("нужно число\n");
            continue;
        }
        if (num == 0)
            break;
        if (num < 0 || num > n) {
            printf("в файле строк: %d\n", n);
            continue;
        }

        line = malloc(lens[num - 1] + 1);
        if (line == NULL) {
            perror("malloc");
            return 1;
        }
        lseek(fd, offs[num - 1], SEEK_SET);
        if (read(fd, line, lens[num - 1]) != lens[num - 1]) {
            perror("read");
            free(line);
            return 1;
        }
        line[lens[num - 1]] = '\0';
        printf("%s\n", line);
        free(line);
    }

    close(fd);
    return 0;
}
