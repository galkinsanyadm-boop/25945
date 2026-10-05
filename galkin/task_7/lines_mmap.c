#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define MAXLINES 1000
#define TIMEOUT 5

volatile sig_atomic_t timed_out = 0;

void on_alarm(int sig)
{
    timed_out = 1;
}

int main(int argc, char *argv[])
{
    int fd;
    struct stat st;
    char *map;          // весь файл как массив байт в памяти
    off_t size;
    off_t offs[MAXLINES];
    int lens[MAXLINES];
    int n = 0;
    off_t pos;
    off_t start = 0;
    ssize_t got;
    long num;
    char in[64];
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

    // mmap нужен размер файла, берем через fstat
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        return 1;
    }
    size = st.st_size;
    if (size == 0) {   // пустой файл отобразить нельзя, mmap вернет ошибку
        printf("файл пустой\n");
        return 0;
    }

    map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {   // при ошибке не NULL, а MAP_FAILED
        perror("mmap");
        return 1;
    }
    close(fd);   // отображение живет и после close, дескриптор больше не нужен

    // таблица строится так же, только вместо read просто идем по массиву
    for (pos = 0; pos < size; pos++) {
        if (map[pos] == '\n') {
            if (n >= MAXLINES) {
                printf("слишком много строк\n");
                return 1;
            }
            offs[n] = start;
            lens[n] = pos - start;
            n++;
            start = pos + 1;
        }
    }
    if (start < size && n < MAXLINES) {
        offs[n] = start;
        lens[n] = size - start;
        n++;
    }

    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    while (1) {
        printf("Номер строки (0 - выход), есть %d секунд: ", TIMEOUT);
        fflush(stdout);

        alarm(TIMEOUT);
        got = read(0, in, sizeof(in) - 1);   // это ввод с клавиатуры, а не чтение файла
        alarm(0);

        if (got == -1) {
            if (errno == EINTR && timed_out) {
                printf("\nвремя вышло, весь файл:\n");
                fwrite(map, 1, size, stdout);   // весь файл уже в памяти, печатаем одним вызовом
                if (map[size - 1] != '\n')
                    printf("\n");
                break;
            }
            perror("read");
            return 1;
        }
        if (got == 0)
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

        // %.*s печатает ровно lens байт начиная с адреса, '\0' в конце не нужен
        printf("%.*s\n", lens[num - 1], map + offs[num - 1]);
    }

    munmap(map, size);
    return 0;
}
