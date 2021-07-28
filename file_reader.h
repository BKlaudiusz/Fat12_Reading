//
// Created by root on 12/10/20.
//

#ifndef PROJEKT2_FILE_READER_H
#define PROJEKT2_FILE_READER_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define FAT_DIRECTORY 0x18
#define BYTES_PER_SECTOR 512
typedef uint16_t fat_date_t;
typedef uint16_t fat_time_t;
typedef uint32_t lba_t;
struct BOOT_SECTOR
{
    char jump_code[3]; //Assembly code instructions to jump to boot code (mandatory in bootable partition)
    char name[8]; //OEM name in ASCII

    uint16_t bytes_per_sector; //Bytes per sector (512, 1024, 2048, or 4096)
    uint8_t sectors_per_cluster; //Sectors per cluster (Must be a power of 2 and cluster size must be <=32 KB)
    uint16_t size_of_reserved_area; //Size of reserved area, in sectors
    uint8_t number_of_fats; //Number of FATs (usually 2)
    uint16_t maximum_number_of_files_in_root_directory; //Maximum number of files in the root directory (FAT12/16; 0 for FAT32)
    uint16_t number_of_sectors; //Number of sectors in the file system; if 2 B is not large enough, set to 0 and use 4 B value in bytes 32-35 below
    uint8_t media_type; //Media type (0xf0=removable disk, 0xf8=fixed disk)
    uint16_t size_of_fat; //Size of each FAT, in sectors, for FAT12/16; 0 for FAT32
    uint16_t sectors_per_track; //Sectors per track in storage device
    uint16_t number_of_heads; //Number of heads in storage device
    uint32_t number_of_sectors_before_partition; //Number of sectors before the start partition
    uint32_t number_of_sectors_in_filesystem; //Number of sectors in the file system; this field will be 0 if the 2B field above(bytes 19 - 20) is non - zero

    uint8_t drive_number; //BIOS INT 13h(low level disk services) drive number
    uint8_t unused_1; //Not used
    uint8_t boot_signature; //Extended boot signature to validate next three fields (0x29)
    uint32_t serial_number; //Volume serial number
    char label[11];  //Volume label, in ASCII
    char type[8];  //File system type level, in ASCII. (Generally "FAT", "FAT12", or "FAT16")
    uint8_t unused_2[448]; //Not used
    uint16_t signature; //Signature value (0xaa55)
} __attribute__(( packed ));

struct date_t
{
    uint16_t day : 5;
    uint16_t month : 4;
    uint16_t year : 7;
}__attribute__(( packed ));

struct my_time_t
{
    uint16_t seconds : 5;
    uint16_t minutes : 6;
    uint16_t hours : 5;
}__attribute__(( packed ));
struct SFN
{
    char filename[8];
    char fileextension[3];
    uint8_t file_attributes;
    uint8_t reserved;
    uint8_t file_creation_time;
    struct my_time_t creation_time;
    struct date_t creation_date;
    uint16_t access_date;
    uint16_t high_order_address_of_first_cluster;
    struct my_time_t modified_time;
    struct date_t modified_date;
    uint16_t low_order_address_of_first_cluster;
    uint32_t size;
} __attribute__(( packed ));
struct disk_t
{
    FILE *fvolume;
    uint32_t size;
};
struct volume_t
{
    struct BOOT_SECTOR bootSector;
    lba_t volume_start ;
    lba_t fat1_position;
    lba_t fat2_position;
    lba_t rootdir_position;
    lba_t data_start;
    struct disk_t* pdisk;
} __attribute__(( packed ));
struct file_t
{
    struct SFN sfn;
    struct  volume_t * volumet;
    uint64_t ptr;
};
struct fat_sfn_t{
    char full_name[8 + 3];
    uint8_t dir_attributes;
    uint8_t __reserved0;
    uint8_t cration_time_ms;
    fat_time_t creation_time;
    fat_date_t creation_date;
    fat_time_t last_access_time;
    fat_date_t high_chain_index;
    fat_time_t last_modification_time;
    fat_date_t last_modification_date;
    uint16_t low_chain_index;
    uint32_t size;};
struct dir_t
{
    struct SFN* root_directory;
    struct  volume_t * volumet;
    uint64_t  index;
};
struct dir_entry_t
{
    char name[18];
    size_t size;
    uint8_t is_archived;
    uint8_t is_readonly;
    uint8_t is_hidden;
    uint8_t is_system;
    uint8_t is_directory;

};
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int validteboot(struct BOOT_SECTOR* temp);
int fat_close(struct volume_t* pvolume);

struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);
#endif //PROJEKT2_FILE_READER_H
