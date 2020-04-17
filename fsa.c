#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

//Function Prototypes
void genFileSysInfo(struct ext2_super_block **);
void indivGroupInfo(struct ext2_super_block **, int );
void rootDirEntries(struct ext2_super_block **);
unsigned int calcBlockSizeBytes(struct ext2_super_block ***);
unsigned int calcNumbInodeBlocksPGroup(struct ext2_super_block ***, unsigned int ); 

int main(int argc, char *argv[]) {

  int rv, fd;
  struct ext2_super_block *superBlock = NULL;
  
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
  
  superBlock = malloc(sizeof(struct ext2_super_block));

  if (superBlock == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "sb");
    exit(1);
  }
  
  /*read the whole superblock and load into the ext2_super_block struct*/
  /*assumes the struct fields are laid out in the order they are defined*/
  rv = read(fd, superBlock, sizeof(struct ext2_super_block));
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }
  if (rv == 1024) {

    genFileSysInfo(&superBlock);
    indivGroupInfo(&superBlock, fd);
    rootDirEntries(&superBlock);

    printf("Total Number of Inodes: %u\n", superBlock->s_inodes_count);

  }    
  
  free(superBlock);
  close(fd);
  
  return 0;
}


void genFileSysInfo(struct ext2_super_block **superBlock) {

    printf("--General File System Information--\n");

    printf("\tBlock Size in Bytes: %u\n", calcBlockSizeBytes(&superBlock));
    printf("\tTotal Number of Blocks: %u\n", (*superBlock)->s_blocks_count);
    printf("Disk Size in Bytes: %u\n", (*superBlock)->s_blocks_count * calcBlockSizeBytes(&superBlock));

    //NOTE: the last group will most probably not have this many blocks but all the groups 
    //execpt the last one will definitely have this many blocks
    printf("\tMaximum Number of Blocks Per Group: %u\n", (*superBlock)->s_blocks_per_group);

    printf("\tInode Size in Bytes: %u\n", (*superBlock)->s_inode_size);
    printf("\tNumber of Inodes Per Group: %u\n", (*superBlock)->s_inodes_per_group);
    printf("\tNumber of Inode Blocks Per Group: %u\n", calcNumbInodeBlocksPGroup(&superBlock, calcBlockSizeBytes(&superBlock)));// COMEBACK

    //Assuming that the virtual disk provided has enough blocks in its last group to fit all of the
    //necessary data structures and some data blocks
    printf("\tNumber of Groups: %u\n", (*superBlock)->s_blocks_count / (*superBlock)->s_blocks_per_group);

}

void indivGroupInfo(struct ext2_super_block **superBlock, int fd) {
    
    unsigned int numberOfGroups = 1 + ((**superBlock).s_blocks_count - 1) / (**superBlock).s_blocks_per_group;
    //unsigned int sizeOfGroupDiscriptor = numberOfGroups * sizeof(struct ext2_group_desc);
    unsigned int groupDiscrByteLocation = calcBlockSizeBytes(&superBlock); //At this point fd should alreadt be at 1024
    struct ext2_group_desc *groupDescriptors = NULL;
    unsigned int numberOfBlocksPerGroup = (**superBlock).s_blocks_per_group;
    unsigned int firstDataBlockID = (**superBlock).s_first_data_block;
    unsigned int totalNumbBlocks = (**superBlock).s_blocks_count;
    unsigned int beginningID = firstDataBlockID;
    unsigned int endingID = totalNumbBlocks < numberOfBlocksPerGroup ? totalNumbBlocks-1 : firstDataBlockID + numberOfBlocksPerGroup-1;
    unsigned int remainingBlocks = totalNumbBlocks - numberOfBlocksPerGroup;
    int rv = 0;

    //moves the head to the position of the group discriptor block
    lseek(fd, groupDiscrByteLocation, SEEK_SET);

    groupDescriptors = malloc(sizeof(struct ext2_group_desc));

    if (groupDescriptors == NULL) {
      fprintf (stderr, "%s: Error in malloc\n", "sb");
      exit(1);
    }
    
    rv = read(fd, groupDescriptors, sizeof(struct ext2_group_desc));
    if (rv == -1) {
      perror("File read failed");
      exit(1);
    }

    if(rv == groupDiscrByteLocation) {

      printf("\n--Individual group Information--\n");
      for(int i = 0; i <= numberOfGroups; i++) {
        printf("\t-Group %d-\n", i);
        printf("\t\tBlock IDs: %u - %u\n", beginningID, endingID);
        printf("\t\tBlock Bitmap Block ID: %u\n", groupDescriptors->bg_block_bitmap);
        printf("\t\tInode Bitmap Block ID: %u\n", groupDescriptors->bg_inode_bitmap);
        printf("\t\tInode Table Block ID: %u\n", groupDescriptors->bg_inode_table);
        printf("\t\tNumber of Free Blocks: %u\n", groupDescriptors->bg_free_blocks_count);
        printf("\t\tNumber of Free Inodes: %u\n", groupDescriptors->bg_free_inodes_count);
        printf("\t\tNumber of Dictionaries: %u\n", groupDescriptors->bg_used_dirs_count);
        
        //printf("\t\tFree Block IDs: %u\n"); //COMEBACK
        // printf("\t\tFree Inode IDs: %u\n"); //COMEBACK

        if (remainingBlocks >= numberOfGroups) {

          beginningID = endingID + 1;
          endingID += numberOfBlocksPerGroup;
          remainingBlocks -= numberOfBlocksPerGroup;

        } else if(remainingBlocks > 0) {

          beginningID = endingID + 1;
          endingID += numberOfBlocksPerGroup;
          remainingBlocks -= numberOfBlocksPerGroup;

        } else {
          printf("ERROR: There are more groups than there is remaining blocks!");
        }

    } else {
      perror("Could not read discriptors block from disk!");
    } 
      



    }

    //printf("Number of Free Inodes: %u\n", (*superBlock)->s_free_inodes_count)

}

void rootDirEntries(struct ext2_super_block **superBlock) {
    printf("--Root Directory Entries--");


}

unsigned int calcBlockSizeBytes(struct ext2_super_block ***superBlock) {

    return 1024 << (**superBlock)->s_log_block_size;
}

unsigned int calcNumbInodeBlocksPGroup(struct ext2_super_block ***superBlock, unsigned int blockSizeBytes) {

    unsigned int totalNumbBlocks = (**superBlock)->s_blocks_count;
    unsigned int inodeSizeBytes = (**superBlock)->s_inode_size;

    return (totalNumbBlocks * inodeSizeBytes) / blockSizeBytes;
}