#include <stdio.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"
int main() {
    struct disk_t* structdisk = disk_open_from_file("fat12_volume.img");
    setvbuf(stdout, NULL, _IONBF, 0);
    assert(sizeof(struct BOOT_SECTOR) == BYTES_PER_SECTOR);
    printf("Hello, World!\n");
    disk_close(structdisk);
    return 0;
}


