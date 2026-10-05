#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define FNAME "data.txt"

// печатает uid-ы и пробует открыть файл, вызывается два раза
void try_open(void)
{
    FILE *f;

    printf("uid=%d euid=%d\n", (int)getuid(), (int)geteuid());

    f = fopen(FNAME, "r");
    if (f == NULL) {
        perror("fopen");   // права проверяются по euid, а не по uid
    } else {
        printf("файл открыт\n");
        fclose(f);
    }
}

int main()
{
    try_open();
    if (setuid(getuid()) == -1)
        perror("setuid");

    try_open();
    return 0;
}
