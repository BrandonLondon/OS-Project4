#include <stdio.h>
#include <sys/time.h>

const int CHANCE_TO_DIE_PERCENT = 10;
const int CHANCE_TO_USE_ALL_TIME_PERCENT = 60;

int main(int argc, int argv)
{
    srand(time(NULL));
    printf("IM ALIVE!..but not for long");

    if((rand() % 100) <= CHANCE_TO_DIE_PERCENT)
        exit(21);
    
    if((rand() % 100) <= CHANCE_TO_USE_ALL_TIME_PERCENT)
    {
        //signal OSS all time used
        exit(21);
    }
    else
    {
        //wait on some task and block
        exit(21);
    }
}