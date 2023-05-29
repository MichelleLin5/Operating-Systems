#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <openssl/sha.h> 

#define SHA_DIGEST_LENGTH 20
int ** permutations_list;
int global_index = 0;
long long int number_of_permutations;


#pragma pack(push,1)
typedef struct BootEntry {
  unsigned char  BS_jmpBoot[3];     // Assembly instruction to jump to boot code
  unsigned char  BS_OEMName[8];     // OEM Name in ASCII
  unsigned short BPB_BytsPerSec;    // Bytes per sector. Allowed values include 512, 1024, 2048, and 4096
  unsigned char  BPB_SecPerClus;    // Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller
  unsigned short BPB_RsvdSecCnt;    // Size in sectors of the reserved area
  unsigned char  BPB_NumFATs;       // Number of FATs
  unsigned short BPB_RootEntCnt;    // Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32
  unsigned short BPB_TotSec16;      // 16-bit value of number of sectors in file system
  unsigned char  BPB_Media;         // Media type
  unsigned short BPB_FATSz16;       // 16-bit size in sectors of each FAT for FAT12 and FAT16. For FAT32, this field is 0
  unsigned short BPB_SecPerTrk;     // Sectors per track of storage device
  unsigned short BPB_NumHeads;      // Number of heads in storage device
  unsigned int   BPB_HiddSec;       // Number of sectors before the start of partition
  unsigned int   BPB_TotSec32;      // 32-bit value of number of sectors in file system. Either this value or the 16-bit value above must be 0
  unsigned int   BPB_FATSz32;       // 32-bit size in sectors of one FAT
  unsigned short BPB_ExtFlags;      // A flag for FAT
  unsigned short BPB_FSVer;         // The major and minor version number
  unsigned int   BPB_RootClus;      // Cluster where the root directory can be found
  unsigned short BPB_FSInfo;        // Sector where FSINFO structure can be found
  unsigned short BPB_BkBootSec;     // Sector where backup copy of boot sector is located
  unsigned char  BPB_Reserved[12];  // Reserved
  unsigned char  BS_DrvNum;         // BIOS INT13h drive number
  unsigned char  BS_Reserved1;      // Not used
  unsigned char  BS_BootSig;        // Extended boot signature to identify if the next three values are valid
  unsigned int   BS_VolID;          // Volume serial number
  unsigned char  BS_VolLab[11];     // Volume label in ASCII. User defines when creating the file system
  unsigned char  BS_FilSysType[8];  // File system type label in ASCII
} BootEntry;
#pragma pack(pop)


#pragma pack(push,1)
typedef struct DirEntry {
  unsigned char  DIR_Name[11];      // File name
  unsigned char  DIR_Attr;          // File attributes
  unsigned char  DIR_NTRes;         // Reserved
  unsigned char  DIR_CrtTimeTenth;  // Created time (tenths of second)
  unsigned short DIR_CrtTime;       // Created time (hours, minutes, seconds)
  unsigned short DIR_CrtDate;       // Created day
  unsigned short DIR_LstAccDate;    // Accessed day
  unsigned short DIR_FstClusHI;     // High 2 bytes of the first cluster address
  unsigned short DIR_WrtTime;       // Written time (hours, minutes, seconds
  unsigned short DIR_WrtDate;       // Written day
  unsigned short DIR_FstClusLO;     // Low 2 bytes of the first cluster address
  unsigned int   DIR_FileSize;      // File size in bytes. (0 for directories)
} DirEntry;
#pragma pack(pop)


void printarray(int arr[], int size)
{
    for(int i = 0; i < size; i++)
    {
        permutations_list[global_index][i] = arr[i];
    }
    global_index++;
}

void swap(int *a, int *b)
{
    int temp;
    temp = *a;
    *a = *b;
    *b = temp;
}

void permutation(int *arr, int start, int end)
{
    if(start==end)
    {
        printarray(arr, end+1);
        return;
    }
    int i;
    for(i=start;i<=end;i++)
    {
        //swap numbers
        swap((arr+i), (arr+start));
        
        //have one fixed digits and get permutation of all other digits
        permutation(arr, start+1, end);
        swap((arr+i), (arr+start));
    }
}


void combination_helper(int * arr, int data[], int start, int end, int index, int r)
{
    // base case: get permutation after each combination is made
    if (index == r)
    {
        permutation(data, 0, r-1);
        return;
    }
 
    //get combinations
    for (int i=start; i<=end && end-i+1 >= r-index; i++)
    {
        data[index] = arr[i];
        combination_helper(arr, data, i+1, end, index+1, r);
    }
}                     
 
void fill_permutations_list_combination_driver (int * arr, int n, int r)
{
    // temporary array to store combinations
    int data[r];
 
    // helper function to get combinations
    combination_helper(arr, data, 0, n-1, 0, r);
}

void print_permutations_list(int number_of_clusters) 
{
    printf("-------------------------- PRINTING PERMUTATIONS LIST --------------------------\n");
    for (int i = 0; i < number_of_permutations; i++)
    {
        for (int j = 0; j < number_of_clusters; j++)
        {
            printf("%d", permutations_list[i][j]);
        }
        printf("\n");
    }
    printf("-------------------------- PERMUTATIONS LIST END --------------------------\n");
} 


void invalid_command_error ()
{
    printf("Usage: ./nyufile disk <options>\n");
    printf("  -i                     Print the file system information.\n");
    printf("  -l                     List the root directory.\n");
    printf("  -r filename [-s sha1]  Recover a contiguous file.\n");
    printf("  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
}

void option_i (char * disk)
{

    BootEntry * boot_entry = (BootEntry *) disk;

    printf("Number of FATs = %d\n", boot_entry->BPB_NumFATs);
    printf("Number of bytes per sector = %d\n", boot_entry->BPB_BytsPerSec);
    printf("Number of sectors per cluster = %d\n", boot_entry->BPB_SecPerClus);
    printf("Number of reserved sectors = %d\n", boot_entry->BPB_RsvdSecCnt); 


}

DirEntry * get_entry (char * disk, BootEntry * boot_entry, int clusters) {
    DirEntry * directory_entry;
    directory_entry = (DirEntry *) (disk + boot_entry->BPB_BytsPerSec * (boot_entry->BPB_RsvdSecCnt
                         + boot_entry->BPB_FATSz32*boot_entry->BPB_NumFATs
                         + boot_entry->BPB_SecPerClus*(clusters-2)));

    return directory_entry;
}

int get_entry_count_per_cluster (BootEntry * boot_entry)
{
    return boot_entry->BPB_BytsPerSec*boot_entry->BPB_SecPerClus/32;
}

void option_l (char * disk) 
{

    BootEntry * boot_entry = (BootEntry *) disk;

    DirEntry * directory_entry = get_entry(disk, boot_entry, boot_entry->BPB_RootClus);

    int root_cluster_count = 0; 
    int entry_count = 0; 
    uint8_t * fat1;
    uint32_t next_cluster; 

    while (next_cluster < 0x0ffffff8)
    {

        root_cluster_count++;

        int entry_per_cluster; 
        entry_per_cluster = get_entry_count_per_cluster(boot_entry); //get the number of entries per cluster
        
        int i;

        for (i = 0; i < entry_per_cluster; i++) //for each entry in the cluster
        {
            unsigned char current_directory_entry_name;
            current_directory_entry_name = * directory_entry->DIR_Name;

            if (current_directory_entry_name == 0x00) //no more entries allocated
            {
                break;
            }
            else if (current_directory_entry_name == 0xe5) //entry is deleted
            { 
                directory_entry++;
                continue;
            }

            int j; 

            //print file name
            for (j = 0; j < 8; j++) 
            {
                current_directory_entry_name = *(directory_entry->DIR_Name + j);
                if (current_directory_entry_name != ' ') //print name is not space
                {
                    printf("%c", current_directory_entry_name);
                }
                else //break if space
                {
                    break;
                } 
        
            }

            // print file extension 
            if ((directory_entry->DIR_Attr & 0x10) != 0x10) //entry is not a directory
            {
                
                int k;
                for (k = 0; k < 3; k++)
                {
                    if (directory_entry->DIR_Name[8+k] != ' ')
                    {
                        if (k == 0)
                        {
                            printf(".");
                            printf("%c", directory_entry->DIR_Name[8+k]);
                        }
                        else
                        {
                            printf("%c", directory_entry->DIR_Name[8+k]);
                        }
                        
                    }
                }
            }
            else //if entry is a directory
            {
                printf("/");
            }

            printf(" (size = %d, starting cluster = %d)\n", directory_entry->DIR_FileSize, directory_entry->DIR_FstClusLO+directory_entry->DIR_FstClusHI*65536);

            directory_entry++;
            entry_count++;
        }

        //locate the next cluster
        if (root_cluster_count >= 1)
        {
            if (root_cluster_count == 1) 
            {
                fat1 = (uint8_t *)(disk + boot_entry->BPB_RsvdSecCnt*boot_entry->BPB_BytsPerSec + boot_entry->BPB_RootClus*4);
            }
            else
            {
                fat1 = (uint8_t *)(disk + boot_entry->BPB_RsvdSecCnt*boot_entry->BPB_BytsPerSec + next_cluster*4);
            }
            memcpy(&next_cluster, fat1, 4);
        }

        directory_entry = get_entry(disk, boot_entry, next_cluster);
    }

    //print out the total number of entries
    printf("Total number of entries = %d\n", entry_count);

}

char * getfilename (DirEntry * directory_entry){
    char *file_name = malloc(12 * sizeof(unsigned char *));
    int index = 0;
    for(int i = 0; i < 8; i++)
    {
        if(directory_entry->DIR_Name[i] != ' ')
        {
            file_name[index] = directory_entry->DIR_Name[i];
            index++;
        }
        else
        {
            break;
        }
    }

    if(directory_entry->DIR_Name[8]!=' ')
    {
        file_name[index] = '.';
        index++;
    }

    for(int i = 8; i < 12; i++)
    {
        if(directory_entry->DIR_Name[i] != ' ')
        {
            file_name[index] = directory_entry->DIR_Name[i];
            index++;
        }
        else
        {
            break;
        }
    }

    file_name[index] = '\0';
    return file_name;
}


void option_r (int fd, BootEntry * disk, char * file_name) //recover contiguous file when no sha1 hash is given
{
    unsigned int number_of_entries = 0;
    unsigned int current_cluster = disk->BPB_RootClus;
    unsigned int possible_entries = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry);

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        fprintf(stderr, "Failed to get file size\n");
        close(fd);
    }
    unsigned char * file_content = mmap(NULL , sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int file_count = 0;

    int number_of_entries_temp = 0, current_cluster_temp = 0;
    DirEntry * directory_entry_temp;

    while (1)
    {

        DirEntry * directory_entry;
        unsigned int root_cluster_offset = ((disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (current_cluster - 2) * disk->BPB_SecPerClus) * disk->BPB_BytsPerSec;
        directory_entry = (DirEntry*)(file_content + root_cluster_offset);

        for(int i = 0; i < possible_entries; i++)
        {
            if (directory_entry->DIR_Attr == 0x00)
            {        
                break;
            }
            if(directory_entry->DIR_Attr != 0x10 && directory_entry->DIR_Name[0] == 0xe5) //if not directory and deleted
            {  
                if (directory_entry->DIR_Name[1] == file_name[1])
                {
                    char *recorded_filename = (char *)getfilename(directory_entry);
                    if(strcmp(recorded_filename+1,file_name+1)==0)
                    {
                        number_of_entries_temp = number_of_entries;
                        directory_entry_temp = directory_entry;
                        current_cluster_temp = current_cluster;
                        file_count++;
                    }
                }
            }
            directory_entry++;
            number_of_entries+=sizeof(DirEntry);
        }

        //locate next cluster
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*current_cluster); 

        if(*fat >= 0x0ffffff8) 
        {
            break;
        }
        if (*fat == 0x00)
        {
            break;
        }
        current_cluster=*fat;
    }

    if (file_count == 1) //only one match, proceed to recover file
    {

        directory_entry_temp->DIR_Name[0] = (unsigned char) file_name[0]; //change the first character of the file name back

        int number_of_clusters = (directory_entry_temp->DIR_FileSize)/(disk->BPB_SecPerClus * disk->BPB_BytsPerSec); //get the number of clusters of the file waiting to be recovered
        if ((directory_entry_temp->DIR_FileSize) % (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)) //如果不能整除就得+1
        {
            number_of_clusters++;
        }

        if (number_of_clusters == 0) //empty file
        {
            ;
        }
        else if (number_of_clusters == 1) //non-empty file with only one cluster
        {
        
            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec; //pointer to where FAT starts
            unsigned int *FAT;

            FAT = (unsigned int *) mmap(NULL, (sb.st_size - end_of_reserved_sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, end_of_reserved_sector);

            int starting_cluster = (directory_entry_temp->DIR_FstClusHI * 65536 + directory_entry_temp->DIR_FstClusLO); //find the first cluster of the file to recover 

            FAT[starting_cluster] = starting_cluster+1;
            FAT[starting_cluster + 1] = 0xfffffff;
            
            munmap(FAT, (sb.st_size - end_of_reserved_sector));

        }
        else //non-empty file with contiguous cluster(s)
        {

            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec; //pointer to where FAT starts
            unsigned int * FAT;

            FAT = (unsigned int *) mmap(NULL, (sb.st_size - end_of_reserved_sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, end_of_reserved_sector);
            
            int starting_cluster = (directory_entry_temp->DIR_FstClusHI * 65536 + directory_entry_temp->DIR_FstClusLO); //find the first cluster of the file to recover 
            unsigned int fat_size = (disk->BPB_FATSz32 * disk->BPB_BytsPerSec)/4;

            // for(int j = 1; j < number_of_clusters; j++) //update the next clusters for the file in FAT
            // {
            //     for (int i = 0; i < disk->BPB_NumFATs; i++, starting_cluster++)
            //     {
            //         FAT[starting_cluster] = starting_cluster + 1;
            //     }
      
            //     FAT[starting_cluster]=0x0fffffff;
            // }

            for (int i = 0; i < disk->BPB_NumFATs; i++)
            {
                for (int j = starting_cluster; j < starting_cluster + number_of_clusters; j++)
                {
                    FAT[j + i * (fat_size)] = j + 1;
                }
                FAT[starting_cluster + number_of_clusters] = 0x0fffffff;
            }


            munmap(FAT, (sb.st_size - end_of_reserved_sector));
            
        }

        munmap(file_content, sb.st_size);
        
        close(fd);
        printf("%s: successfully recovered\n",file_name);
       
    } 
    else if (file_count < 1) //no matches
    {
        printf("%s: file not found\n", file_name);
    }
    else if (file_count > 1) //multiple matches
    {
        printf("%s: multiple candidates found\n",file_name);
    }
  
}

void option_s (int fd, BootEntry * disk, char * file_name, char * sha1_hash_given) //recover contiguous file when a sha1 hash is given
{
    unsigned int number_of_entries = 0;
    unsigned int current_cluster = disk->BPB_RootClus;
    unsigned int possible_entries = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry);
    DirEntry * file_entry_to_recover;
    int found = 0;

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        fprintf(stderr, "Failed to get file size\n");
        close(fd);
    }
    unsigned char * file_content = mmap(NULL , sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int file_count = 0;

    int number_of_entries_temp = 0, current_cluster_temp = 0;
    DirEntry* directory_entry_temp = NULL;
    

    while (1)
    {
    
        DirEntry * directory_entry;
        unsigned int root_cluster_offset = ((disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (current_cluster - 2) * disk->BPB_SecPerClus) * disk->BPB_BytsPerSec;
        directory_entry = (DirEntry*)(file_content + root_cluster_offset);

        for(int i = 0; i < possible_entries; i++)
        {
            if (directory_entry->DIR_Attr == 0x00)
            {        
                break;
            }
            if(directory_entry->DIR_Attr != 0x10 && directory_entry->DIR_Name[0] == 0xe5) //if not directory and deleted
            {  
                if (directory_entry->DIR_Name[1] == file_name[1])
                {
                    char *recorded_filename = (char *)getfilename(directory_entry);
                    if(strcmp(recorded_filename+1,file_name+1)==0)
                    {
                        // fprintf(stderr, "file_name_matched: %s", file_name);
                        int start_byte = (((*disk).BPB_FATSz32*(*disk).BPB_NumFATs)+(*disk).BPB_RsvdSecCnt)*(*disk).BPB_BytsPerSec;
                        
                        if (strcmp (sha1_hash_given, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0)
                        {
                            if (directory_entry->DIR_FileSize == 0) //if it is actually 0 
                            {
                                found = 1;
                                // fprintf(stderr, "found SHA1 empty file!\n");
                                file_count++;
                                // number_of_entries_temp = number_of_entries;
                                directory_entry_temp = directory_entry;
                                // current_cluster_temp = current_cluster;
                            }
                        
                        }
                        else 
                        {
                            char * content = malloc(sizeof(char) * (directory_entry->DIR_FileSize + 1)); //file content
                            unsigned char * hashed = malloc(sizeof(unsigned char) * (SHA_DIGEST_LENGTH + 1)); //original hashed (length = 20)
                            char * result_string = malloc(sizeof(char) * 41); //hexadecimal hashed (length = 40)
                            
                            int file_size = directory_entry->DIR_FileSize; //size of the file
                            uint32_t fileStartClu = (directory_entry->DIR_FstClusLO + directory_entry->DIR_FstClusHI * 65536); //starting cluster of the file
                            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec;
                            uint32_t fat_end = end_of_reserved_sector + (disk->BPB_NumFATs * disk->BPB_FATSz32 * disk->BPB_BytsPerSec);

                            char * file_head = (char *)(file_content + (fat_end + ((fileStartClu - 2) * disk->BPB_SecPerClus * disk->BPB_BytsPerSec))); //pointer to file head at starting cluster
                            // char * file_head = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (fat_end + ((fileStartClu - 2) * disk->BPB_SecPerClus * disk->BPB_BytsPerSec)));

                            int number_of_clusters; //get the number of clusters of the file waiting to be recovered
                            number_of_clusters = (directory_entry->DIR_FileSize)/(disk->BPB_SecPerClus * disk->BPB_BytsPerSec);
                            if ((directory_entry->DIR_FileSize) % (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)) //如果不能整除就得+1
                            {
                                number_of_clusters++;
                            }

                            //put the file content in
                            int iterator = 0;
                            for (int i = 0; i < number_of_clusters; i++)
                            {
                                for (int j = 0; j < (disk->BPB_SecPerClus * disk->BPB_BytsPerSec); j++)
                                {
                                    if (iterator < file_size)
                                    {
                                        iterator = (i * disk->BPB_SecPerClus * disk->BPB_BytsPerSec) + j;
                                        content[iterator] = (file_head[iterator]);
                                    }
                                    else
                                    {
                                        break;
                                    }
                
                                }
                            }

                            SHA1(content, directory_entry->DIR_FileSize, hashed); //hash it

                            for (int i = 0; i < 20; i++)
                            {
                                sprintf(&result_string[i*2], "%02x", hashed[i]); //put the hashed 20 bytes into hexadecimals to create the 40 hexademical string
                            }

                            result_string[40] = '\0'; //since the result is a string, add null value at the end

                            // fprintf(stderr, "result string = %s", result_string);

                            if (strcmp(result_string, sha1_hash_given) == 0) //if two strings compare to be the same
                            {
                                found = 1;
                                // fprintf(stderr, "found SHA1 non empty file!\n");
                                file_count++;
                                // fprintf(stderr, "found it through hash1!");
                                directory_entry_temp = directory_entry;
                
                            }

                            free(content);
                            free(hashed);
                            free(result_string);
                        }
                        number_of_entries_temp = number_of_entries;
                        current_cluster_temp = current_cluster;
                        
                    } 

                }             
            }
            directory_entry++;
            number_of_entries+=sizeof(DirEntry);
        }
        

        //locate next cluster
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*current_cluster); 

        if(*fat >= 0x0ffffff8) 
        {
            break;
        }
        if (*fat == 0x00)
        {
            break;
        }
        current_cluster=*fat;
        if (found = 1) //if already found 
        {
    
            break;
        }
    }

    if (found == 0) //no matches
    {
        printf("%s: file not found\n", file_name);
    }
    else if (directory_entry_temp != NULL)  //found
    {
        // fprintf(stderr, "It came to file recovery part!\n");
        directory_entry_temp->DIR_Name[0] = (unsigned char) file_name[0]; //change the first character of the file name back

        
        int number_of_clusters; //get the number of clusters of the file waiting to be recovered

        if (strcmp (sha1_hash_given, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0)
        {
            number_of_clusters = 0;
        }
        else
        {
            number_of_clusters = (directory_entry_temp->DIR_FileSize)/(disk->BPB_SecPerClus * disk->BPB_BytsPerSec);
            if ((directory_entry_temp->DIR_FileSize) % (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)) //如果不能整除就得+1
            {
                number_of_clusters++;
            }

        }

        if (number_of_clusters == 0) //empty file, so we are done
        {
            ;
        }
        else if (number_of_clusters == 1) //non-empty file with only one cluster
        {
        
            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec; //pointer to where FAT starts
            unsigned int *FAT;

            FAT = (unsigned int *) mmap(NULL, (sb.st_size - end_of_reserved_sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, end_of_reserved_sector);

            int starting_cluster = (directory_entry_temp->DIR_FstClusHI * 65536 + directory_entry_temp->DIR_FstClusLO); //find the first cluster of the file to recover 
            unsigned int fat_size = (disk->BPB_FATSz32 * disk->BPB_BytsPerSec)/4;

            FAT[starting_cluster] = starting_cluster+1;
            FAT[starting_cluster + 1] = 0xfffffff;
            
            munmap(FAT, (sb.st_size - end_of_reserved_sector));

        }
        else //non-empty file with contiguous cluster(s)
        {

            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec; //pointer to where FAT starts
            unsigned int * FAT;

            FAT = (unsigned int *) mmap(NULL, (sb.st_size - end_of_reserved_sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, end_of_reserved_sector);
            
            int starting_cluster = (directory_entry_temp->DIR_FstClusHI * 65536 + directory_entry_temp->DIR_FstClusLO); //find the first cluster of the file to recover 
            unsigned int fat_size = (disk->BPB_FATSz32 * disk->BPB_BytsPerSec)/4;
            unsigned int size_of_file = directory_entry_temp->DIR_FileSize;

            // for(int j = 1; j < number_of_clusters; j++) //update the next clusters for the file in FAT
            // {
            //     for (int i = 0; i < disk->BPB_NumFATs; i++, starting_cluster++)
            //     {
            //         FAT[starting_cluster + (i * fat_size)] = starting_cluster + 1;
            //     }
      
                
            // }
            // FAT[starting_cluster]=0x0fffffff;


            // for (int j = 1; j < number_of_clusters; j++) //update the next clusters for the file in FAT
            // {
            //     for (int i = 0; i < disk->BPB_NumFATs; i++, starting_cluster++)
            //     {
            //         FAT[starting_cluster] = starting_cluster + 1;
            //     }
      
            //     FAT[starting_cluster] = 0x0fffffff;
            // }


            for (int i = 0; i < disk->BPB_NumFATs; i++)
            {
                for (int j = starting_cluster; j < starting_cluster + number_of_clusters; j++)
                {
                    FAT[j + i * (fat_size)] = j + 1;
                }
                FAT[starting_cluster + number_of_clusters] = 0x0fffffff;
            }

            munmap(FAT, (sb.st_size - end_of_reserved_sector));
            
        }

        munmap(file_content, sb.st_size);
        
        close(fd);
        printf("%s: successfully recovered with SHA-1\n",file_name);

    }

}   


char * map_disk (char * argv[])
{
    char *file_in_memory;
    struct stat sb;
    int fd;

    fd = open(argv[1], O_RDWR);

    if (fd == -1)
    {
        fprintf(stderr, "Failed to open file\n");
    }
    
    if (fstat(fd, &sb) == -1)
    {
        fprintf(stderr, "Failed to get file size\n");
        close(fd);
    }

    file_in_memory = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (file_in_memory == MAP_FAILED)
    {
        fprintf(stderr, "Map failed\n");
        close(fd);
    }

    close(fd);
    
    return file_in_memory;
}

long long int get_number_of_permutations (int number_of_clusters, int number_of_unallocated_clusters)
{
    //get the numerator (n!)
    long long int numerator = 1;
    for (int i = number_of_unallocated_clusters; i > 0; i--)
    {
        numerator = numerator * i;
    }

    // printf("numerator = %lld\n", numerator);

    //get the denominator ((n-r)!)
    long long int denominator = 1;
    for (int i = (number_of_unallocated_clusters - number_of_clusters); i > 0; i--)
    {
        denominator = denominator * i;
    }
    // printf("denominator = %lld\n", denominator);

    long long int result;
    result = numerator/denominator;
    
    return result;

}

void option_R (int fd, BootEntry * disk, char * file_name, char * sha1_hash_given) //recover non-contiguous file when a sha1 hash is given
{
    unsigned int number_of_entries = 0;
    unsigned int current_cluster = disk->BPB_RootClus;
    unsigned int possible_entries = (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)/sizeof(DirEntry);
    DirEntry * file_entry_to_recover;
    int found = 0;

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        fprintf(stderr, "Failed to get file size\n");
        close(fd);
    }
    unsigned char * file_content = mmap(NULL , sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int file_count = 0;

    int number_of_entries_temp = 0, current_cluster_temp = 0;
    DirEntry* directory_entry_temp = NULL;
    int index_in_permutations_list = NULL;

    while (1)
    {
    
        DirEntry * directory_entry;
        unsigned int root_cluster_offset = ((disk->BPB_RsvdSecCnt + disk->BPB_NumFATs*disk->BPB_FATSz32) + (current_cluster - 2) * disk->BPB_SecPerClus) * disk->BPB_BytsPerSec;
        directory_entry = (DirEntry*)(file_content + root_cluster_offset);

        for(int i = 0; i < possible_entries; i++)
        {
            if (directory_entry->DIR_Attr == 0x00)
            {        
                break;
            }
            if(directory_entry->DIR_Attr != 0x10 && directory_entry->DIR_Name[0] == 0xe5) //if not directory and deleted
            {  
                if (directory_entry->DIR_Name[1] == file_name[1])
                {
                    char *recorded_filename = (char *)getfilename(directory_entry);
                    if(strcmp(recorded_filename+1,file_name+1)==0)
                    {
                        // fprintf(stderr, "file_name_matched: %s", file_name);
                        int start_byte = (((*disk).BPB_FATSz32*(*disk).BPB_NumFATs)+(*disk).BPB_RsvdSecCnt)*(*disk).BPB_BytsPerSec;
                        
                        if (strcmp (sha1_hash_given, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0)
                        {
                            if (directory_entry->DIR_FileSize == 0) //if it is actually 0 
                            {
                                found = 1;
                                // fprintf(stderr, "found SHA1 empty file!\n");
                                file_count++;
                                // number_of_entries_temp = number_of_entries;
                                directory_entry_temp = directory_entry;
                                // current_cluster_temp = current_cluster;
                            }
                        
                        }
                        else 
                        {
                            char * content = malloc(sizeof(char) * (directory_entry->DIR_FileSize + 1)); //file content
                            unsigned char * hashed = malloc(sizeof(unsigned char) * (SHA_DIGEST_LENGTH + 1)); //original hashed (length = 20)
                            char * result_string = malloc(sizeof(char) * 41); //hexadecimal hashed (length = 40)
                            
                            int file_size = directory_entry->DIR_FileSize; //size of the file
                            uint32_t fileStartClu = (directory_entry->DIR_FstClusLO + directory_entry->DIR_FstClusHI * 65536); //starting cluster of the file
                            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec;
                            uint32_t fat_end = end_of_reserved_sector + (disk->BPB_NumFATs * disk->BPB_FATSz32 * disk->BPB_BytsPerSec);

                            // char * file_head = (char *)(file_content + (fat_end + ((fileStartClu - 2) * disk->BPB_SecPerClus * disk->BPB_BytsPerSec))); //pointer to file head at starting cluster
                            // char * file_head = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (fat_end + ((fileStartClu - 2) * disk->BPB_SecPerClus * disk->BPB_BytsPerSec)));

                            int number_of_clusters; //get the number of clusters of the file waiting to be recovered
                            number_of_clusters = (directory_entry->DIR_FileSize)/(disk->BPB_SecPerClus * disk->BPB_BytsPerSec);
                            if ((directory_entry->DIR_FileSize) % (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)) //如果不能整除就得+1
                            {
                                number_of_clusters++;
                            }


                            unsigned int * FAT;

                            FAT = (unsigned int *) mmap(NULL, (sb.st_size - end_of_reserved_sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, end_of_reserved_sector);
                            // int arr[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21}; //cluster numbers that the file can be at 

                            int * arr;
                            int number_of_unallocated_clusters = 0;

                            for (int i = 2; i < 22; i++) //只留下unallocated cluster numbers in arr
                            {
                                if (FAT[i] == 0)
                                {
                                    number_of_unallocated_clusters++;
                                }
                            }

                            arr = malloc(number_of_unallocated_clusters * (sizeof(int))); //这个gdb 确认过了没问题
                            int arr_index = 0;

                            for (int i = 2; i < 22; i++)
                            {
                                if (FAT[i] == 0)
                                {
                                    arr[arr_index] = i;
                                    arr_index++;
                                }
                            }

                            number_of_permutations = get_number_of_permutations(number_of_clusters, number_of_unallocated_clusters); //这个没问题

                            permutations_list = malloc(sizeof(int *) * number_of_permutations);

                            for (int i = 0; i < number_of_permutations; i++)
                            {
                                permutations_list[i] = malloc(sizeof(int) * number_of_clusters);
                            }

                            int r = number_of_clusters;
                            // int n = sizeof(arr)/sizeof(arr[0]);
                            int n = number_of_unallocated_clusters;
                            fill_permutations_list_combination_driver(arr, n, r);

                            //print out permutations list
                            // print_permutations_list(number_of_clusters);


                            int index_in_permutations_list = NULL;

                            for (int perms = 0; perms < number_of_permutations; perms++)
                            {
                                if (permutations_list[perms][0] == fileStartClu)
                                {
                                    //put the file content in
                                    int iterator = 0;
                                    for (int i = 0; i < number_of_clusters; i++)
                                    {
                                        int current_cluster_to_put_in = permutations_list[perms][i];
                                        char * current_cluster_to_put_in_head = (char *)(file_content + (fat_end + ((current_cluster_to_put_in - 2) * disk->BPB_SecPerClus * disk->BPB_BytsPerSec)));
                

                                        for (int j = 0; j < (disk->BPB_SecPerClus * disk->BPB_BytsPerSec); j++)
                                        {
                                            if (iterator < file_size)
                                            {
                                                iterator = (i * disk->BPB_SecPerClus * disk->BPB_BytsPerSec) + j;
                                                content[iterator] = current_cluster_to_put_in_head[j];
                                            }
                                            else
                                            {
                                                break;
                                            }
                        
                                        }
                                    }

                                    SHA1(content, directory_entry->DIR_FileSize, hashed); //hash it

                                    for (int i = 0; i < 20; i++)
                                    {
                                        sprintf(&result_string[i*2], "%02x", hashed[i]); //put the hashed 20 bytes into hexadecimals to create the 40 hexademical string
                                    }

                                    result_string[40] = '\0'; //since the result is a string, add null value at the end

                                    // fprintf(stderr, "result string = %s", result_string);

                                    if (strcmp(result_string, sha1_hash_given) == 0) //if two strings compare to be the same
                                    {
                                        found = 1;
                                        // fprintf(stderr, "found SHA1 non empty file!\n");
                                        file_count++;
                                        // fprintf(stderr, "found it through hash1!");
                                        directory_entry_temp = directory_entry;
                                        index_in_permutations_list = perms;
                        
                                    }
                                }
                                if (index_in_permutations_list != NULL)
                                {
                                    break;
                                }
                            }

                            free(content);
                            free(hashed);
                            free(result_string);
                        }
                        number_of_entries_temp = number_of_entries;
                        current_cluster_temp = current_cluster;
                        
                    } 

                }             
            }
            if (index_in_permutations_list != NULL)
            {
                break;
            }
            directory_entry++;
            number_of_entries+=sizeof(DirEntry);
        }
        

        //locate next cluster
        unsigned int *fat = (unsigned int*)(file_content + disk->BPB_RsvdSecCnt*disk->BPB_BytsPerSec + 4*current_cluster); 

        if(*fat >= 0x0ffffff8) 
        {
            break;
        }
        if (*fat == 0x00)
        {
            break;
        }
        current_cluster=*fat;
        if (found = 1) //if already found 
        {
    
            break;
        }
    }

    if (found == 0) //no matches
    {
        printf("%s: file not found\n", file_name);
    }
    else if (directory_entry_temp != NULL)  //found
    {
        // fprintf(stderr, "It came to file recovery part!\n");
        directory_entry_temp->DIR_Name[0] = (unsigned char) file_name[0]; //change the first character of the file name back

        
        int number_of_clusters; //get the number of clusters of the file waiting to be recovered

        if (strcmp (sha1_hash_given, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0)
        {
            number_of_clusters = 0;
        }
        else
        {
            number_of_clusters = (directory_entry_temp->DIR_FileSize)/(disk->BPB_SecPerClus * disk->BPB_BytsPerSec);
            if ((directory_entry_temp->DIR_FileSize) % (disk->BPB_SecPerClus * disk->BPB_BytsPerSec)) //如果不能整除就得+1
            {
                number_of_clusters++;
            }

        }

        if (number_of_clusters == 0) //empty file, so we are done
        {
            ;
        }
        else if (number_of_clusters == 1) //non-empty file with only one cluster
        {
        
            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec; //pointer to where FAT starts
            unsigned int *FAT;

            FAT = (unsigned int *) mmap(NULL, (sb.st_size - end_of_reserved_sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, end_of_reserved_sector);

            int starting_cluster = (directory_entry_temp->DIR_FstClusHI * 65536 + directory_entry_temp->DIR_FstClusLO); //find the first cluster of the file to recover 

            FAT[starting_cluster] = starting_cluster+1;
            FAT[starting_cluster + 1] = 0xfffffff;
            
            munmap(FAT, (sb.st_size - end_of_reserved_sector));

        }
        else //non-empty file with non-contiguous cluster(s)
        {

            unsigned int end_of_reserved_sector = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec; //pointer to where FAT starts
            unsigned int * FAT;

            FAT = (unsigned int *) mmap(NULL, (sb.st_size - end_of_reserved_sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, end_of_reserved_sector);
            
            int starting_cluster = (directory_entry_temp->DIR_FstClusHI * 65536 + directory_entry_temp->DIR_FstClusLO); //find the first cluster of the file to recover 
            unsigned int fat_size = (disk->BPB_FATSz32 * disk->BPB_BytsPerSec)/4;

            // for (int j = 0; j < number_of_clusters-1; j++) //update the next clusters for the file in FAT
            // {
            //     int cluster_recovering = permutations_list[index_in_permutations_list][j];
            //     for (int i = 0; i < disk->BPB_NumFATs; i++)
            //     {
            //         FAT[cluster_recovering] = permutations_list[index_in_permutations_list][j+1];
            //     }
      
                
            // }


            // FAT[permutations_list[index_in_permutations_list][number_of_clusters-1]] = 0xfffffff;
            

            for (int i = 0; i < disk->BPB_NumFATs; i++)
            {
                for (int j = 0; j < number_of_clusters; j++)
                {
                    int cluster_recovering = permutations_list[index_in_permutations_list][j];
                    FAT[cluster_recovering + i * (fat_size)] = permutations_list[index_in_permutations_list][j+1];
                }
                FAT[permutations_list[index_in_permutations_list][number_of_clusters] + i * (fat_size)] = 0x0fffffff;
            }

            munmap(FAT, (sb.st_size - end_of_reserved_sector));
            
        }

        munmap(file_content, sb.st_size);
        
        close(fd);
        printf("%s: successfully recovered with SHA-1\n",file_name);

    }

}

int main (int argc, char * argv[])
{
    // validate usage: if not, call invalid_command_error and exit 
    if (argv[2] != NULL)
    {

        char * disk;

        if (argc == 3)
        {
            if (strcmp(argv[2], "-i") == 0) //print file system information
            {
                disk = map_disk(argv);
                option_i(disk);
            }
            else if (strcmp(argv[2], "-l") == 0)
            {
                disk = map_disk(argv);
                option_l(disk);
            }
            else
            {
                invalid_command_error();
            }
        }
       
        else if (argc == 4)
        {
            if (strcmp(argv[2], "-r") == 0)
            {
            
                int fd = open(argv[1], O_RDWR);
                BootEntry *disk = mmap(NULL, sizeof(BootEntry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                option_r(fd, disk, argv[3]);
            }
            else
            {
                invalid_command_error();
            }
        }
        else if (argc == 6)
        {
            if (strcmp(argv[2], "-r") == 0 && strcmp(argv[4], "-s") == 0)
            {
                int fd = open(argv[1], O_RDWR);
                BootEntry *disk = mmap(NULL, sizeof(BootEntry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                option_s(fd, disk, argv[3], argv[5]);
            }
            else if (strcmp(argv[2], "-s") == 0 && strcmp(argv[4], "-r") == 0)
            {
                int fd = open(argv[1], O_RDWR);
                BootEntry *disk = mmap(NULL, sizeof(BootEntry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                option_s(fd, disk, argv[5], argv[3]);
            }
            else if (strcmp(argv[2], "-R") == 0 && strcmp(argv[4], "-s") == 0)
            {
                int fd = open(argv[1], O_RDWR);
                BootEntry *disk = mmap(NULL, sizeof(BootEntry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                option_R(fd, disk, argv[3], argv[5]);
            }
            else if (strcmp(argv[2], "-s") == 0 && strcmp(argv[4], "-R") == 0)
            {
                int fd = open(argv[1], O_RDWR);
                BootEntry *disk = mmap(NULL, sizeof(BootEntry), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                option_R(fd, disk, argv[5], argv[3]);
            }
            else
            {
                invalid_command_error();
            }
        }
        else
        {
            invalid_command_error();
        }
    }
    else
    {
        invalid_command_error();
    }
}

