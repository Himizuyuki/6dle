#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

unsigned int randomNumber()
{
    unsigned int random_number;
    int random_data = open("/dev/urandom", O_RDONLY);

    if (random_data < 0)
    {
        perror("Failed to open /dev/urandom");
        return 1;
    }

    if (read(random_data, &random_number, sizeof(random_number))
        != sizeof(random_number))
    {
        perror("Failed to read /dev/urandom");
        close(random_data);
        return 1;
    }

    close(random_data);

    return random_number;
}
