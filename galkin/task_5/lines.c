#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#define MAXLINES 1000
#define BUFSIZE 512

int main(int argc, char *argv[])
{
    int fd;
    off_t offs[MAXLINES];   // с какого байта начинается строка
    int lens[MAXLINES];     // длина строки без '\n'
    int n = 0;              // сколько строк нашли
    char buf[BUFSIZE];
    ssize_t got;
    off_t pos = 0;          // позиция текущего байта в файле
    off_t start = 0;        // начало текущей строки
    int i, num, r;
    char *line;

    if (argc != 2) {
        printf("Использование: %s файл\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    // читаем файл кусками и ищем '\n', так строим таблицу
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
                start = pos + 1;   // следующая строка начинается сразу после '\n'
            }
            pos++;
        }
    }
    if (got == -1) {
        perror("read");
        return 1;
    }
    // если в конце файла нет '\n', последняя строка все равно считается
    if (start < pos && n < MAXLINES) {
        offs[n] = start;
        lens[n] = pos - start;
        n++;
    }

    // таблица для отладки, как в подсказке
    printf("строка  отступ  длина\n");
    for (i = 0; i < n; i++)
        printf("%6d  %6ld  %5d\n", i + 1, (long)offs[i], lens[i]);

    while (1) {
        printf("Номер строки (0 - выход): ");
        r = scanf("%d", &num);
        if (r == EOF)   // ввод закрыли (ctrl+D)
            break;
        if (r != 1) {
            printf("нужно число\n");
            while (getchar() != '\n')   // выкидываем мусор из ввода, иначе scanf зациклится
                ;
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
        lseek(fd, offs[num - 1], SEEK_SET);   // прыгаем на начало нужной строки
        if (read(fd, line, lens[num - 1]) != lens[num - 1]) {
            perror("read");
            free(line);
            return 1;
        }
        line[lens[num - 1]] = '\0';   // read не ставит конец строки, ставим сами
        printf("%s\n", line);
        free(line);
    }

    close(fd);
    return 0;
}
