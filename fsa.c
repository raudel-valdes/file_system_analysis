#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

//Function Prototypes
void genFileSysInfo(struct ext2_super_block **);
void indivGroupInfo(struct ext2_super_block **);
void rootDirEntries(struct ext2_super_block **);
unsigned int calcBlockSizeBytes(struct ext2_super_block ***);
unsigned int calcNumbInodeBlocksPGroup(struct ext2_super_block ***, unsigned int ); 

int main(int argc, char *argv[]) {

  int rv, fd;
  char *sb1 = NULL;
  struct ext2_super_block *sb2 = NULL;
  
  if (argc != 2) {
    fprintf (stderr, "%s: Usage: ./sb disk_image_file\n", "sb");
    exit(1);
  }
  
  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror("disk_image_file open failed");
    exit(1);
  }
  
  /*skip the boot info - the first 1024 bytes - using the lseek*/
  if (lseek(fd, 1024, SEEK_CUR) != 1024) {
    perror("File seek failed");
    exit(1);
  }
  
  sb2 = malloc(sizeof(struct ext2_super_block));

  if (sb2 == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "sb");
    exit(1);
  }
  
  /*read the whole superblock and load into the ext2_super_block struct*/
  /*assumes the struct fields are laid out in the order they are defined*/
  rv = read(fd, sb2, sizeof(struct ext2_super_block));
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }
  if (rv == 1024) {

    genFileSysInfo(&sb2);
    indivGroupInfo(&sb2);
    rootDirEntries(&sb2);

    printf("Total Number of Inodes: %u\n", sb2->s_inodes_count);

  }    
  
  free(sb1);
  free(sb2);
  close(fd);
  
  return 0;
}


void genFileSysInfo(struct ext2_super_block **sb2) {

    printf("--General File System Information--\n");

    printf("Block Size in Bytes: %u\n", calcBlockSizeBytes(&sb2));
    printf("Total Number of Blocks: %u\n", (*sb2)->s_blocks_count);
    printf("Disk Size in Bytes: %u\n", (*sb2)->s_blocks_count * calcBlockSizeBytes(&sb2));

    //NOTE: the last group will most probably not have this many blocks but all the groups 
    //execpt the last one will definitely have this many blocks
    printf("Maximum Number of Blocks Per Group: %u\n", (*sb2)->s_blocks_per_group);

    printf("Inode Size in Bytes: %u\n", (*sb2)->s_inode_size);
    printf("Number of Inodes Per Group: %u\n", (*sb2)->s_inodes_per_group);
    printf("Number of Inode Blocks Per Group: %u\n", calcNumbInodeBlocksPGroup(&sb2, calcBlockSizeBytes(&sb2)));// COMEBACK

    //Assuming that the virtual disk provided has enough blocks in its last group to fit all of the
    //necessary data structures and some data blocks
    printf("Number of Groups: %u\n", (*sb2)->s_blocks_count / (*sb2)->s_blocks_per_group);

}

void indivGroupInfo(struct ext2_super_block **sb2) {
    
    int numbGroups = (**sb2)->s_blocks_count / (**sb2)->s_blocks_per_group);

    for(int i=0; i <= numbGroups; i++) {
        printf("-Group %d-\n", i);

        printf("Block IDs: %u\n"); //COMEBACK
        printf("Block Bitmap Block ID: %u\n"); //COMEBACK
        printf("Inode Bitmap Block ID: %u\n"); //COMEBACK
        printf("Inode Table Block ID: %u\n"); //COMEBACK
        printf("Number of Free Blocks: %u\n"); //COMEBACK
        printf("Number of Free Inodes: %u\n"); //COMEBACK
        printf("Number of Dictionaries: %u\n"); //COMEBACK
        printf("Free Block IDs: %u\n"); //COMEBACK
        printf("Free Inode IDs: %u\n"); //COMEBACK

    }

    printf("--Individual group Information--");

    printf("Number of Free Inodes: %u\n", (*sb2)->s_free_inodes_count);



}

void rootDirEntries(struct ext2_super_block **sb2) {
    printf("--Root Directory Entries--");


}

unsigned int calcBlockSizeBytes(struct ext2_super_block ***sb2) {

    return 1024 << (**sb2)->s_log_block_size;
}

unsigned int calcNumbInodeBlocksPGroup(struct ext2_super_block ***sb2, unsigned int blockSizeBytes) {

    unsigned int totalNumbBlocks = (**sb2)->s_blocks_count;
    unsigned int inodeSizeBytes = (**sb2)->s_inode_size;

    return (totalNumbBlocks * inodeSizeBytes) / blockSizeBytes;
}