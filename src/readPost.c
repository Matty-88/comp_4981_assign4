#include <ctype.h>
#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void printSafe(const char *label, datum d);

void printSafe(const char *label, datum d)
{
    printf("%s (size %d): ", label, d.dsize);
    for(int i = 0; i < d.dsize; ++i)
    {
        unsigned char c = (unsigned char)d.dptr[i];

        if(isprint(c))
        {
            putchar(c);
        }
        else
        {
            printf("\\x%02x", c);
        }
    }
    putchar('\n');


}


int main(void)
{
    DBM  *db;
    datum key;
    datum nextkey;
    datum value;

    // cppcheck-suppress constVariable
    char dbname[] = "postRequests";

    db = dbm_open(dbname, O_RDONLY, 0);
    if(!db)
    {
        perror("dbm_open failed");
        return 1;
    }

    printf("reading post request:\n");



    for(key = dbm_firstkey(db); key.dptr != NULL; key = nextkey)
    {

        nextkey = dbm_nextkey(db);
        value   = dbm_fetch(db, key);

        printSafe("Key", key);
        printSafe("Value", value);
        printf("-----------\n");
    }

    dbm_close(db);
    return 0;
}
