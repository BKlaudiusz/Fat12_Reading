#include <errno.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"

struct disk_t* disk_open_from_file(const char* volume_file_name)
{
    if(volume_file_name!=NULL)
    {
        FILE *fvolume = fopen(volume_file_name, "rb");
        if(fvolume)
        {
            struct disk_t* toReturn = (struct disk_t*)malloc(sizeof(struct disk_t));
            if(toReturn)
            {
                toReturn->fvolume = fvolume;
                fseek(fvolume, 0, SEEK_END);
                toReturn->size = (unsigned long)ftell(fvolume) / BYTES_PER_SECTOR;
                return toReturn;
            }else
            errno=ENOMEM;
        }else
        errno=ENOENT;
    }else
    errno=EFAULT;

    return NULL;
}
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read)
{
    if(pdisk!=NULL)
    {
        if(first_sector>=0 && (uint32_t )sectors_to_read< pdisk->size)
        {
            fseek(pdisk->fvolume, first_sector * BYTES_PER_SECTOR, SEEK_SET);
            int read_block = fread(buffer, BYTES_PER_SECTOR, sectors_to_read, pdisk->fvolume);
            return read_block;
        }else
            errno=EFAULT;
    }else
        errno=ERANGE ;
    return  -1;
}
int disk_close(struct disk_t* pdisk)
{
    if(pdisk!=NULL)
    {

        fclose(pdisk->fvolume);
        free(pdisk);
        return 0;
    }
    errno = EFAULT;
    return -1;
}
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector)
{
    if(pdisk != NULL && first_sector == 0)
    {
        struct  volume_t* toreturn = (struct volume_t*)calloc(1,sizeof(struct volume_t));
        if(toreturn == NULL)
        {
            errno = ENOMEM;
            return NULL;
        }
        toreturn->data_start = 0 ;

        if(disk_read(pdisk,0,&toreturn->bootSector,1)>=1)
        {
            if(validteboot(&toreturn->bootSector)) {
                toreturn->fat1_position = toreturn->volume_start + toreturn->bootSector.size_of_reserved_area;
                toreturn->fat2_position = toreturn->volume_start + toreturn->bootSector.size_of_reserved_area +
                                          toreturn->bootSector.size_of_fat;

                uint8_t *fat1_data = (uint8_t *) malloc(
                        toreturn->bootSector.bytes_per_sector * toreturn->bootSector.size_of_fat);
                uint8_t *fat2_data = (uint8_t *) malloc(
                        toreturn->bootSector.bytes_per_sector * toreturn->bootSector.size_of_fat);

                if (fat1_data == NULL || fat2_data == NULL) {
                    free(fat1_data);
                    free(fat2_data);
                    free(toreturn);
                    printf("Brak pamięci");
                    errno = ENOMEM;
                    return NULL;
                }

                int r1 = disk_read(pdisk, toreturn->fat1_position, fat1_data, 1);
                int r2 = disk_read(pdisk, toreturn->fat2_position, fat2_data, 1);
                if(r1 == 0 || r2 == 0)
                {
                    free(fat1_data);
                    free(fat2_data);
                    free(toreturn);
                    errno = ENOMEM;
                    return NULL;
                }

                if (memcmp(fat1_data, fat2_data,
                           toreturn->bootSector.bytes_per_sector * toreturn->bootSector.size_of_fat) != 0) {
                   // free(fat1_data);
                  //  free(fat2_data);
                  //  free(toreturn);
                  //  errno = EINVAL;
                  //  return NULL;
                }
                toreturn->rootdir_position = toreturn->volume_start + toreturn->bootSector.size_of_reserved_area
                                             + toreturn->bootSector.number_of_fats * toreturn->bootSector.size_of_fat;
                toreturn->data_start =
                        toreturn->rootdir_position + toreturn->bootSector.maximum_number_of_files_in_root_directory*sizeof(struct SFN)/toreturn->bootSector.bytes_per_sector;
                toreturn->pdisk = pdisk;
                free(fat1_data);
                free(fat2_data);
                return toreturn;
            } else
                free(toreturn);
        }
    }

    errno = EFAULT;
    return NULL;
}
int validteboot(struct BOOT_SECTOR* temp)
{
    int validate = 0;
    for (unsigned i = 1; i <= 128; i *= 2)
        if (temp->sectors_per_cluster == i)
        {
            validate = 1;
            break;
        }
    if(validate)
    {
        if(temp->size_of_reserved_area>0)
            if(temp->number_of_fats ==1 || temp->number_of_fats == 2)
                if(temp->bytes_per_sector == BYTES_PER_SECTOR)
                    return  1;
    }
    return  0;

}
int fat_close(struct volume_t* pvolume) {
    if(pvolume!= NULL)
    {
        free(pvolume);
        return  0 ;
    }
    errno = EFAULT;
    return  -1;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name)
{
if(pvolume== NULL || file_name == NULL)
{
    errno=EFAULT;
    return NULL;
}
    struct SFN* root_directory = (struct SFN *)malloc(pvolume->bootSector.maximum_number_of_files_in_root_directory * sizeof(struct SFN));
    struct file_t* toreturn = (struct file_t *) malloc(1 * sizeof(struct file_t));
    if(root_directory== NULL || toreturn == NULL)
    {
        free(root_directory);
        free(toreturn);
        errno = ENOMEM;
        return  NULL;
    }
    unsigned int mark_id= 0;
    unsigned int exp_id= 0;
    while (*(file_name + mark_id)!= 0)
    {
        if(*(file_name + mark_id)=='.')
            break;
        mark_id++;
    }
    exp_id = mark_id;
    while (*(file_name + exp_id)!= 0)
    {
        exp_id++;
    }
    disk_read(pvolume->pdisk,pvolume->rootdir_position,root_directory,1*pvolume->bootSector.sectors_per_cluster);

    for (int i = 0; i < pvolume->bootSector.maximum_number_of_files_in_root_directory; i++)
    {

        switch ((unsigned char)root_directory[i].filename[0])
        {
            case 0x00: /* Not used */
                break;
            case 0x2e: /* Dot */
                break;
            case 0x05:/* Deleted */
                break;
            default: /* File*/
                printf("%s%s %d\n",root_directory[i].filename,root_directory[i].fileextension,i);
                if((mark_id == exp_id  && !memcmp(file_name,root_directory[i].filename,mark_id)) || (!memcmp(file_name,root_directory[i].filename,mark_id) && !memcmp((file_name+ mark_id+1),root_directory[i].fileextension,exp_id-(mark_id+1))))
                {
                  //  printf("%d\n",root_directory[i].file_attributes & 0x10);
                    if((root_directory[i].file_attributes & 0x10)>0)
                    {
                        printf("folder\n");
                        free(root_directory);
                        free(toreturn);
                        errno = EISDIR;
                        return  NULL;
                    }
                    printf("\nNo. %d\n", i);
                    printf("%s\n",file_name);
                    memcpy(&toreturn->sfn, root_directory + i, sizeof(struct SFN));
                    toreturn->volumet = pvolume;
                    toreturn->ptr = 0;
                    free(root_directory);
                    return toreturn;
                }
                break;
        }
    }
    free(root_directory);
    free(toreturn);
    errno = ENOENT;
    return NULL;
}
int file_close(struct file_t* stream)
{
    if(stream!= NULL) {
        free(stream);
        return 0;
    }
    return 1;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream)
{
    // TODO dodac wskaznik
    //  przesuwanie go
    // dodanie go w szukaniu tablicy
    if(ptr== NULL || stream == NULL || stream->volumet == NULL)
    {
        errno=EFAULT;
        return -1;
    }

    // wez klaster z struktury
    //
    char* primary_fat_allocation_table = (char *)malloc(stream->volumet->bootSector.bytes_per_sector * stream->volumet->bootSector.size_of_fat);
    char * memtoread = (char*)malloc(BYTES_PER_SECTOR* stream->volumet->bootSector.sectors_per_cluster);
    if(memtoread == NULL|| primary_fat_allocation_table == NULL)
    {
        free(primary_fat_allocation_table);
        free(memtoread);
        errno = ENOMEM;
        return -1;
    }
    uint64_t moveinptr = 0;
    if(ptr!= NULL && stream!=NULL)
    {
        disk_read(stream->volumet->pdisk,stream->volumet->fat1_position,primary_fat_allocation_table,1* stream->volumet->bootSector.size_of_fat);

        // to musi byc ruchome sprawdzic gdzie jest ptr
        uint64_t clasterread = 0;
        clasterread = stream->sfn.low_order_address_of_first_cluster;

        uint64_t temp = 1 ;
        if(stream->ptr != 0)
        {
            while (1)
            {
                if(stream->ptr== 2048)
                    printf("A");
                if(stream->ptr>= (temp-1) * BYTES_PER_SECTOR && stream->ptr< stream->volumet->bootSector.sectors_per_cluster*  temp * BYTES_PER_SECTOR )
                    break;

                int fat_offset = clasterread / 2 * 3;
                int cluster_0 = (primary_fat_allocation_table[fat_offset] & 0xff) | (primary_fat_allocation_table[fat_offset + 1] & 0x0f) << 8;
                int cluster_1 = ((primary_fat_allocation_table[fat_offset + 2] & 0xff) << 4) | ((primary_fat_allocation_table[fat_offset + 1] & 0xf0) >> 4);

                if(clasterread%2)
                {
                    clasterread = cluster_1;
                }else
                    clasterread = cluster_0;
                temp++;
            }
        }


        size_t toreturn = nmemb;

        uint64_t sizetoread = size*nmemb;
        if(sizetoread> stream->sfn.size) {
            sizetoread = stream->sfn.size;
            toreturn = stream->sfn.size;
        }

        if(stream->ptr == stream->sfn.size)
        {
            free(primary_fat_allocation_table);
            free(memtoread);
            return 0;
        }
        if(stream->ptr +sizetoread> stream->sfn.size)
        {
            sizetoread = stream->sfn.size - stream->ptr;
            toreturn =0 ;
        }
        int offset = (stream->ptr-((temp-1)*stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR)-moveinptr);
        while (sizetoread>0)
        {
            //unsigned int temp2 =moveinptr %(stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR);
            // trzeba policzyc offeset od poczatku sektora
            if(offset!= 0 )
                offset= (stream->ptr-((temp-1)*stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR)-moveinptr);

            /*
            if(moveinptr%stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR != 0 )
                offset= (stream->ptr-((temp-1)*stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR)-moveinptr);
            else
                offset= (stream->ptr-((temp-1)*stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR));
                */
          //offset = 0;
            disk_read(stream->volumet->pdisk,(clasterread-2)*stream->volumet->bootSector.sectors_per_cluster+stream->volumet->data_start,memtoread,stream->volumet->bootSector.sectors_per_cluster);
            if(sizetoread>=stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR) {
                //tutaj trzeba wstawic offset
                memcpy((char *) ptr + moveinptr, memtoread+offset, stream->volumet->bootSector.sectors_per_cluster * BYTES_PER_SECTOR-offset);
                moveinptr += stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR;
                sizetoread -=stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR +offset;
                // tutaj zmienic rozmiar
                stream->ptr += stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR;
            }else
            {
                //tutaj trzeba wstawic offset
                if(stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR< offset + sizetoread)
                {
                    memcpy((char *) ptr + moveinptr, memtoread +offset, (stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR-offset));
                    moveinptr = (stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR-offset);
                    sizetoread -= (stream->volumet->bootSector.sectors_per_cluster*BYTES_PER_SECTOR-offset);
                    offset =0;
                    stream->ptr += moveinptr;
                }else {
                    memcpy((char *) ptr + moveinptr, memtoread + offset, sizetoread);
                    moveinptr += sizetoread;
                    stream->ptr += sizetoread;
                    sizetoread = 0;

                }
            }
            int cluster_index = clasterread;
            int fat_offset = cluster_index / 2 * 3;
            // printf("%02x %02x %02x\n" ,(unsigned char)primary_fat_allocation_table[fat_offset + 0], (unsigned char)primary_fat_allocation_table[fat_offset + 1], (unsigned char)primary_fat_allocation_table[fat_offset + 2]);
            int cluster_0 = (primary_fat_allocation_table[fat_offset] & 0xff )| ((primary_fat_allocation_table[fat_offset + 1] & 0x0f) << 8);
            int cluster_1 = ((primary_fat_allocation_table[fat_offset + 2] & 0xff) << 4) | (((primary_fat_allocation_table[fat_offset + 1] & 0xf0) >> 4));

            if(clasterread%2)
            {
                clasterread = cluster_1;
            }else
                clasterread = cluster_0;
            if(clasterread>=0xFF8)
                break;

        }
        // ile sie da
        free(primary_fat_allocation_table);
        free(memtoread);
        return  toreturn;
    }
    free(primary_fat_allocation_table);
    free(memtoread);
    return 0;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence)
{
    // nule
    if(stream!= NULL)
    {
        if(whence == SEEK_SET)
        {
            stream->ptr = offset;
        }else if(whence == SEEK_CUR)
        {
            stream->ptr += offset;
        }else if(whence == SEEK_END)
        {
            stream->ptr = stream->sfn.size + offset;
        }else
        {
            errno = EINVAL;
            return -1;
        }
    }
    errno = EFAULT;
    return -1;
    // czu whence ma sens
    // czy mozna o tyle przesunac
    // ustawienie
}
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path)
{
    if(pvolume != NULL && dir_path!= NULL)
    {
        if(!memcmp(dir_path,"\\",1))
        {
            struct SFN* root_directory = (struct SFN *)malloc(pvolume->bootSector.maximum_number_of_files_in_root_directory * sizeof(struct SFN));
            struct dir_t* toreturn = (struct dir_t *) malloc(1 * sizeof(struct dir_t));
            if(root_directory == NULL || toreturn == NULL)
            {
                free(root_directory);
                free(toreturn);
            }else
            {
                disk_read(pvolume->pdisk,pvolume->rootdir_position,root_directory,1*pvolume->bootSector.sectors_per_cluster);
                toreturn->root_directory = root_directory;
                toreturn->volumet = pvolume;
                toreturn->index = 0;
                return toreturn;
            }
        }else
        {
            errno = ENOTDIR;
            return NULL;
        }
    }
    errno =EFAULT;
    return  NULL;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry)
{
    if(pdir!= NULL && pentry!= NULL)
    {
        for (int i = pdir->index; i < pdir->volumet->bootSector.maximum_number_of_files_in_root_directory; i++)
        {
printf("%lu\n",pdir->index);
            switch ((unsigned char)pdir->root_directory[i].filename[0])
            {
                case 0x00:/* Not used */
                    break;
                case 0x2e: /* Dot */
                    pdir->index ++;
                    break;
                case 0xE5:
                    pdir->index ++;/* Deleted */
                    break;
                default:
                    printf("%s%s\n",pdir->root_directory[i].filename,pdir->root_directory[i].fileextension);
                    pdir->index ++;
                    pentry->size = pdir->root_directory[i].size;
                    if((pdir->root_directory[i].file_attributes & 0x2)>0)
                        pentry->is_hidden = 1;
                    else
                        pentry->is_hidden = 0;
                    if((pdir->root_directory[i].file_attributes & 0x4)>0)
                        pentry->is_system = 1;
                    else
                        pentry->is_system = 0;
                    if((pdir->root_directory[i].file_attributes & 0x10)>0)
                    {
                        pentry->is_directory = 1;
                        int nameidex = 0;
                        while (nameidex<=7 && pdir->root_directory[i].filename[nameidex]!= ' ')
                        {
                            pentry->name[nameidex] = pdir->root_directory[i].filename[nameidex];
                            nameidex++;
                        }
                        pentry->name[nameidex]=0;
                    }
                    else {
                        pentry->is_directory = 0;
                        int nameidex = 0;
                        while (nameidex<=7 && pdir->root_directory[i].filename[nameidex]!= ' ')
                        {
                            pentry->name[nameidex] = pdir->root_directory[i].filename[nameidex];
                            nameidex++;
                        }
                        int checkexp = 0;
                        while (checkexp<=2 &&pdir->root_directory[i].fileextension[checkexp]!= ' '  )
                            checkexp++;
                        if(checkexp==0)
                        {
                            pentry->name[nameidex]=0;
                            return 0;
                        }

                        pentry->name[nameidex]='.';
                        nameidex++;
                        int expindex = 0;
                        while (expindex<3 && pdir->root_directory[i].fileextension[expindex]!= ' ')
                        {
                            pentry->name[nameidex] = pdir->root_directory[i].fileextension[expindex];
                            expindex++;
                            nameidex++;
                        }
                        pentry->name[nameidex]=0;
                    }
                    if((pdir->root_directory[i].file_attributes & 0x0F)>0)
                        pentry->is_archived = 1;
                    else
                        pentry->is_archived = 0;

                    return 0;
                    break;/* File */


            }
        }
    }
    return 1;
}
int dir_close(struct dir_t* pdir)
{
    if(pdir)
    {
        free(pdir->root_directory);
        free(pdir);
    }
    return  -1;
}




