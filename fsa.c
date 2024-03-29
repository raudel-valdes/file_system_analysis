#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <stdbool.h>

//Function Prototypes
void genFileSysInfo(struct ext2_super_block **);
void indivGroupInfo(struct ext2_super_block **, char *);
void rootDirEntries(struct ext2_super_block **);
unsigned int calcBlockSizeBytes(struct ext2_super_block ***);
unsigned int calcNumbInodeBlocksPGroup(struct ext2_super_block ***, unsigned int ); 
void calcFreeBlockIDRanges(unsigned int, unsigned int);
void calcFreeInodeIDRanges(unsigned int, unsigned int);


int main(int argc, char *argv[]) {

  int rv, fd;
  struct ext2_super_block *superBlock = NULL;
  off_t currentPosition;
  
  if (argc != 2) {
    fprintf (stderr, "%s: Usage: ./sb disk_image_file\n", "sb");
    exit(1);
  }
  
  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror("disk_image_file open failed");
    exit(1);
  }

  currentPosition = lseek(fd, 0, SEEK_CUR);
  printf("Current position #1: %lu\n", currentPosition);
  
  /*skip the boot info - the first 1024 bytes - using the lseek*/
  unsigned int lseekRV = lseek(fd, 1024, SEEK_CUR);
  if (lseekRV != 1024) {
    perror("File seek failed");
    exit(1);
  }

  printf("lseekRv after superblock lseek: %u\n", lseekRV);
  
  superBlock = malloc(sizeof(struct ext2_super_block));

  if (superBlock == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "sb");
    exit(1);
  }
  currentPosition = lseek(fd, 0, SEEK_CUR);

  /*read the whole superblock and load into the ext2_super_block struct*/
  /*assumes the struct fields are laid out in the order they are defined*/
  printf("Current position #2: %lu\n", currentPosition);
  rv = read(fd, superBlock, sizeof(struct ext2_super_block));

    currentPosition = lseek(fd, 0, SEEK_CUR);
    printf("Current position #3: %lu\n", currentPosition);

  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }
  if (rv == 1024) {

    genFileSysInfo(&superBlock);
    indivGroupInfo(&superBlock, argv[1]);
    rootDirEntries(&superBlock);

  }    
  
  free(superBlock);
  close(fd);
  
  return 0;
}


void genFileSysInfo(struct ext2_super_block **superBlock) {

  printf("--General File System Information--\n");

  printf("\tBlock Size in Bytes: %u\n", calcBlockSizeBytes(&superBlock));
  printf("\tTotal Number of Blocks: %u\n", (*superBlock)->s_blocks_count);
  printf("\tDisk Size in Bytes: %u\n", (*superBlock)->s_blocks_count * calcBlockSizeBytes(&superBlock));

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

void indivGroupInfo(struct ext2_super_block **superBlock, char *filePath) {
    
  unsigned int numberOfGroups = 1 + ((**superBlock).s_blocks_count - 1) / (**superBlock).s_blocks_per_group;
  //unsigned int sizeOfGroupDiscriptor = numberOfGroups * sizeof(struct ext2_group_desc);
  unsigned int blockSize =  calcBlockSizeBytes(&superBlock);
  struct ext2_group_desc *groupDescriptors = NULL;
  unsigned int numberOfBlocksPerGroup = (**superBlock).s_blocks_per_group;
  unsigned int firstDataBlockID = (**superBlock).s_first_data_block;
  unsigned int totalNumbBlocks = (**superBlock).s_blocks_count;
  unsigned int beginningID = firstDataBlockID;
  unsigned int endingID = totalNumbBlocks < numberOfBlocksPerGroup ? totalNumbBlocks-1 : firstDataBlockID + numberOfBlocksPerGroup-1;
  unsigned int remainingBlocks = totalNumbBlocks - numberOfBlocksPerGroup;
  int rv = 0;
  int fd = 0;


  fd = open(filePath, O_RDONLY);
  if (fd == -1) {
    perror("disk_image_file open failed");
    exit(1);
  }

  groupDescriptors = malloc(sizeof(struct ext2_group_desc));
  if (groupDescriptors == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "groupDescriptors");
    exit(1);
  }
  
    off_t currentPosition;
    currentPosition = lseek(fd, 0, SEEK_CUR);
  printf("Current position #3.5: %lu\n", currentPosition);

  //moves the head to the position of the group discriptor block
  unsigned int lseekRV = lseek(fd, blockSize, SEEK_CUR);
  if (lseekRV != blockSize) {
    perror("File seek failed");
    exit(1);
  }
  
   
  currentPosition = lseek(fd, 0, SEEK_CUR);
  printf("Current position #3.6: %lu\n", currentPosition);

  rv = read(fd, groupDescriptors, sizeof(struct ext2_group_desc));

  currentPosition = lseek(fd, 0, SEEK_CUR);
  printf("Current position #3.7: %lu\n", currentPosition);
 
  printf("FD after groupDiscriptor read: %u\n", fd);
  
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }

  printf("\n--Individual group Information--\n");

  for(int i = 0; i < numberOfGroups; i++) {

    printf("\t-Group %d-\n", i);
    printf("\t\tBlock IDs: %u - %u\n", beginningID, endingID);
    printf("\t\tBlock Bitmap Block ID: %u\n", groupDescriptors->bg_block_bitmap);
    printf("\t\tInode Bitmap Block ID: %u\n", groupDescriptors->bg_inode_bitmap);
    printf("\t\tInode Table Block ID: %u\n", groupDescriptors->bg_inode_table);
    printf("\t\tNumber of Free Blocks: %u\n", groupDescriptors->bg_free_blocks_count);
    printf("\t\tNumber of Free Inodes: %u\n", groupDescriptors->bg_free_inodes_count);
    printf("\t\tNumber of Dictionaries: %u\n", groupDescriptors->bg_used_dirs_count);
      
    off_t currentPosition;
    currentPosition = lseek(fd, 0, SEEK_CUR);
    printf("Current position #4: %lu\n", currentPosition);


    printf("\t\tFree Block IDs: \n");
    calcFreeBlockIDRanges(blockSize, groupDescriptors->bg_block_bitmap);

    printf("\t\tFree Inode IDs: \n");
    calcFreeInodeIDRanges(blockSize, groupDescriptors->bg_inode_bitmap);

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
  }

  //close(fd); MAYBE
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

void calcFreeBlockIDRanges(unsigned int blockSize, unsigned int bg_block_bitmap) {
  
  unsigned char *blockBitmap = NULL;
  int rv = 0, lseekRV;
  unsigned int blockOffset = bg_block_bitmap * blockSize;
  bool freeBlock = false;

  bool freeBlockAlreadyExists = false;
  printf("blockoffset: %u, bg_blovk_bitmap: %u, blockSize %u\n", blockOffset, bg_block_bitmap, blockSize);
  

  int fd2 = open("ext2disk_100mb.img", O_RDONLY);
  if (fd2 == -1) {
    perror("disk_image_file open failed");
    exit(1);
  }

  
  blockBitmap = malloc(blockSize);
  if (blockBitmap == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "blockBitmap");
    exit(1);
  }

  //moves the head to the position of the group discriptor block
  printf("bg_group_bitmap: %u\n", bg_block_bitmap);
 lseekRV = lseek(fd2, blockOffset, SEEK_SET);
  printf("lseekRv: %d\n", lseekRV);

  if(lseekRV != blockOffset) {
    perror("calcFreeBlockIDRanges: File seek failed");
    exit(1);
  }

  rv = read(fd2, blockBitmap, blockSize);
  
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }

  for (int i = 0; i < blockSize; i++) {

    for(int j = 0; j < 8; j++) {
      //printf("BYTE %d -> BIT %d\n", i, (blockBitmap[i] & (1 << j)) != 0);

      if(!((blockBitmap[i] & (1 << j)) != 0)) {
        
        if(!freeBlock) {
          
          if(freeBlockAlreadyExists) printf(", ");

          freeBlockAlreadyExists = true;
          freeBlock = true;

          if(!i) i = 1;
          if(!j) j = 1;

          printf("%d", (i) * (j) * 8);

        }


      } else {

        if(freeBlock) {

          freeBlock = false;

          if(!i) i =1;
          if(!j) j =1;
          
          printf("-%d", ((j) * (i) * 8) - 1);

        }
      }

    }

  }

  printf("\n");

  free(blockBitmap);
}

void calcFreeInodeIDRanges(unsigned int blockSize, unsigned int bg_inode_bitmap) {

   unsigned char *blockBitmap = NULL;
  int rv = 0, lseekRV;
  unsigned int blockOffset = bg_inode_bitmap * blockSize;
  bool freeBlock = false;
  bool freeBlockAlreadyExists = false;

  int fd2 = open("ext2disk_100mb.img", O_RDONLY);
  if (fd2 == -1) {
    perror("disk_image_file open failed");
    exit(1);
  }

  
  blockBitmap = malloc(blockSize);
  if (blockBitmap == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "blockBitmap");
    exit(1);
  }

  //moves the head to the position of the group discriptor block
 lseekRV = lseek(fd2, blockOffset, SEEK_SET);
  printf("lseekRv: %d\n", lseekRV);

  if(lseekRV != blockOffset) {
    perror("calcFreeBlockIDRanges: File seek failed");
    exit(1);
  }

  rv = read(fd2, blockBitmap, blockSize);
  
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }

  for (int i = 0; i < blockSize; i++) {

    for(int j = 0; j < 8; j++) {
      //printf("BYTE %d -> BIT %d\n", i, (blockBitmap[i] & (1 << j)) != 0);

      if(!((blockBitmap[i] & (1 << j)) != 0)) {
        
        if(!freeBlock) {
          
          if(freeBlockAlreadyExists) printf(", ");

          freeBlockAlreadyExists = true;
          freeBlock = true;

          if(!i) i = 1;
          if(!j) j = 1;

          printf("%d", (i) * (j) * 8);

        }


      } else {

        if(freeBlock) {

          freeBlock = false;

          if(!i) i =1;
          if(!j) j =1;
          
          printf("-%d", ((j) * (i) * 8) - 1);

        }
      }

    }

  }

  printf("\n");

  free(blockBitmap);

}