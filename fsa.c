#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

//Function Prototypes
void genFileSysInfo(struct ext2_super_block *);
void indivGroupInfo(int fd, struct ext2_super_block *, char *);
unsigned int calcBlockSizeBytes(struct ext2_super_block *);
unsigned int calcNumbInodeBlocksPGroup(struct ext2_super_block *, unsigned int ); 
void calcFreeBlockIDRanges(int fd, unsigned int, int, struct ext2_super_block *, struct ext2_group_desc *, unsigned int, unsigned int);
void calcFreeInodeIDRanges(int fd, unsigned int, int, struct ext2_super_block *, struct ext2_group_desc *, unsigned int, unsigned int);
void rootDirEntries(int, unsigned int, struct ext2_super_block *, struct ext2_group_desc *);


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
  unsigned int lseekRV = lseek(fd, 1024, SEEK_CUR);
  if (lseekRV != 1024) {
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

    genFileSysInfo(superBlock);
    indivGroupInfo(fd, superBlock, argv[1]);

  }    
  
  free(superBlock);
  close(fd);
  
  return 0;
}


void genFileSysInfo(struct ext2_super_block *superBlock) {

  printf("--General File System Information--\n");

  printf("Block Size in Bytes: %u\n", calcBlockSizeBytes(superBlock));
  printf("Total Number of Blocks: %u\n", superBlock->s_blocks_count);
  printf("Disk Size in Bytes: %u\n", superBlock->s_blocks_count * calcBlockSizeBytes(superBlock));

  //NOTE: the last group will most probably not have this many blocks but all the groups 
  //execpt the last one will definitely have this many blocks
  printf("Maximum Number of Blocks Per Group: %u\n", superBlock->s_blocks_per_group);

  printf("Inode Size in Bytes: %u\n", superBlock->s_inode_size);
  printf("Number of Inodes Per Group: %u\n", superBlock->s_inodes_per_group);
  printf("Number of Inode Blocks Per Group: %u\n", calcNumbInodeBlocksPGroup(superBlock, calcBlockSizeBytes(superBlock)));

  //Assuming that the virtual disk provided has enough blocks in its last group to fit all of the
  //necessary data structures and some data blocks
  printf("Number of Groups: %u\n", superBlock->s_blocks_count / superBlock->s_blocks_per_group);

}

void indivGroupInfo(int fd, struct ext2_super_block *superBlock, char *filePath) {

  //GENERAL 
  unsigned int numberOfGroups = 1 + (superBlock->s_blocks_count - 1) / superBlock->s_blocks_per_group;
  unsigned int sizeOfGroupDiscriptor = numberOfGroups * sizeof(struct ext2_group_desc);
  struct ext2_group_desc *groupDescriptors = NULL;
  int rv = 0;
  
  //INODES
  //unsigned int numberOfInodeGroups = 1 + (superBlock->s_inodes_count - 1)  / superBlock->s_inodes_per_group;
  unsigned int totalNumbInodes = superBlock->s_inodes_count;
  unsigned int numberOfInodesPerGroup = superBlock->s_inodes_per_group;
  unsigned int beginningInodeID = 1;
  unsigned int endingInodeID = totalNumbInodes < numberOfInodesPerGroup ? totalNumbInodes : numberOfInodesPerGroup;
  unsigned int remainingInodes = totalNumbInodes - numberOfInodesPerGroup;
  
  //BLOCKS
  unsigned int blockSize =  calcBlockSizeBytes(superBlock);
  unsigned int numberOfBlocksPerGroup = superBlock->s_blocks_per_group;
  unsigned int firstDataBlockID = superBlock->s_first_data_block;
  unsigned int totalNumbBlocks = superBlock->s_blocks_count;
  unsigned int beginningBlockID = firstDataBlockID;
  unsigned int endingBlockID = totalNumbBlocks < numberOfBlocksPerGroup ? totalNumbBlocks-1 : firstDataBlockID + numberOfBlocksPerGroup-1;
  unsigned int remainingBlocks = totalNumbBlocks - numberOfBlocksPerGroup;

  groupDescriptors = malloc(sizeOfGroupDiscriptor);
  if (groupDescriptors == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "groupDescriptors");
    exit(1);
  }

  unsigned int lseekRV = lseek(fd, blockSize, SEEK_SET);
  if (lseekRV != blockSize) {
    perror("idivGroupInfo: File seek failed");
    exit(1);
  }

  rv = read(fd, groupDescriptors, sizeOfGroupDiscriptor);

  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }

  printf("\n--Individual Group Information--\n");

  for(int i = 0; i < numberOfGroups; i++) {

    printf("-Group %d-\n", i);
    printf("Block IDs: %u-%u\n", beginningBlockID, endingBlockID);
    printf("Block Bitmap Block ID: %u\n", groupDescriptors[i].bg_block_bitmap);
    printf("Inode Bitmap Block ID: %u\n", groupDescriptors[i].bg_inode_bitmap);
    printf("Inode Table Block ID: %u\n", groupDescriptors[i].bg_inode_table);
    printf("Number of Free Blocks: %u\n", groupDescriptors[i].bg_free_blocks_count);
    printf("Number of Free Inodes: %u\n", groupDescriptors[i].bg_free_inodes_count);
    printf("Number of Dictionaries: %u\n", groupDescriptors[i].bg_used_dirs_count);

    printf("Free Block IDs: ");
    calcFreeBlockIDRanges(fd, blockSize, i, superBlock, groupDescriptors, beginningBlockID, endingBlockID);

    printf("Free Inode IDs: ");
    calcFreeInodeIDRanges(fd, blockSize, i, superBlock, groupDescriptors, beginningInodeID, endingInodeID);

    

    if (remainingBlocks >= numberOfGroups) {

      beginningBlockID = endingBlockID + 1;
      endingBlockID += numberOfBlocksPerGroup;
      remainingBlocks -= numberOfBlocksPerGroup;

    } else if(remainingBlocks > 0) {

      beginningBlockID = endingBlockID + 1;
      endingBlockID += numberOfBlocksPerGroup;
      remainingBlocks -= numberOfBlocksPerGroup;

    }

    if(remainingInodes >= numberOfGroups) {

      beginningInodeID = endingInodeID + 1;
      endingInodeID += numberOfInodesPerGroup;
      remainingInodes -= numberOfInodesPerGroup;

    } else if(remainingInodes > 0) {

      beginningInodeID = endingInodeID + 1;
      endingInodeID += numberOfInodesPerGroup;
      remainingInodes -= numberOfInodesPerGroup;

    }

  }

  rootDirEntries(fd, blockSize, superBlock, groupDescriptors);

  free(groupDescriptors);
}


unsigned int calcBlockSizeBytes(struct ext2_super_block *superBlock) {

  return 1024 << superBlock->s_log_block_size;
}

unsigned int calcNumbInodeBlocksPGroup(struct ext2_super_block *superBlock, unsigned int blockSizeBytes) {

  unsigned int totalNumbBlocks = superBlock->s_blocks_count;
  unsigned int inodeSizeBytes = superBlock->s_inode_size;

  return (totalNumbBlocks * inodeSizeBytes) / blockSizeBytes;
}

void calcFreeBlockIDRanges(
    int fd, 
    unsigned int blockSize, 
    int groupNumber, 
    struct ext2_super_block *superBlock, 
    struct ext2_group_desc * groupDescriptions, 
    unsigned int baseBit,
    unsigned int endBit
  ) {
  
  unsigned char *blockBitmap = NULL;
  int rv = 0, lseekRV = 0;
  unsigned int blockOffset = 0;
  int currentBit = 0;
  //CRUCIAL THAT PREVIOUSBIT IS INITIALIZE TO 1
  int previousBit = 1;
  int rangeLength = 0;
  bool activeRange = false;
  bool freeBlockAlreadyExists = false;
  //unsigned int blocksPerGroup = superBlock->s_blocks_per_group;
  unsigned int bgBlockBitmap = groupDescriptions[groupNumber].bg_block_bitmap;
  
  blockOffset = bgBlockBitmap * blockSize;

  
  blockBitmap = malloc(blockSize);
  if (blockBitmap == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "blockBitmap");
    exit(1);
  }

  lseekRV = lseek(fd, blockOffset, SEEK_SET);

  if(lseekRV != blockOffset) {
    perror("calcFreeBlockIDRanges: File seek failed");
    exit(1);
  }

  rv = read(fd, blockBitmap, blockSize);
  
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }

  for (int i = 0; i < blockSize; i++) {

    for(int j = 0; j < 8; j++) {

      currentBit = ((blockBitmap[i] & (1 << j)) != 0);

      rangeLength = currentBit == 0 ? rangeLength + 1 : 0;

      if(currentBit == 0 && previousBit == 1) {
        
          if(freeBlockAlreadyExists) printf(", ");

          freeBlockAlreadyExists = true;
          activeRange = true;
          

        printf("%d", (i * 8) + j + baseBit);

      } 


      if((((currentBit && currentBit != previousBit && freeBlockAlreadyExists ) || ((i == blockSize -1) && j == 7)) && activeRange) && rangeLength == 1) {

        activeRange = 0;
        rangeLength = 0;

      } else if(((currentBit && currentBit != previousBit && freeBlockAlreadyExists ) || ((i == blockSize -1) && j == 7)) && activeRange) {
          
        if(j == 0) {

          printf("-%d", ((i - 1) * 8) + 7 + baseBit);

        } else {

          printf("-%d", (i * 8) + j + baseBit);

        }

        activeRange = false;
        rangeLength = 0;

      }

      previousBit = currentBit;
    }
  }
  
  printf("\n");

  free(blockBitmap);
}

void calcFreeInodeIDRanges(
    int fd, 
    unsigned int blockSize, 
    int groupNumber, 
    struct ext2_super_block *superBlock, 
    struct ext2_group_desc * groupDescriptions, 
    unsigned int baseBit,
    unsigned int endBit
  ) {

  unsigned char *inodeBitmap = NULL;
  int rv = 0, lseekRV = 0;
  unsigned int blockOffset = 0;
  int currentBit = 0;
  //CRUCIAL THAT PREVIOUSBIT IS INITIALIZE TO 1
  int previousBit = 1;
  int rangeLength = 0;
  bool activeRange = false;
  bool freeBlockAlreadyExists = false;
  //unsigned int blocksPerGroup = superBlock->s_blocks_per_group;
  unsigned int bgBlockBitmap = groupDescriptions[groupNumber].bg_inode_bitmap;
  
  blockOffset = bgBlockBitmap * blockSize;

  inodeBitmap = malloc(blockSize);

  if (inodeBitmap == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "inodeBitmap");
    exit(1);
  }
  
  lseekRV = lseek(fd, blockOffset, SEEK_SET);
 
  if(lseekRV != blockOffset) {
    perror("calcFreeBlockIDRanges: File seek failed");
    exit(1);
  }

  rv = read(fd, inodeBitmap, blockSize);
  
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }

  for (int i = 0; i < blockSize; i++) {

    for(int j = 0; j < 8; j++) {
      
      currentBit = ((inodeBitmap[i] & (1 << j)) != 0);

      rangeLength = currentBit == 0 ? rangeLength + 1 : 0;

      if(currentBit == 0 && previousBit == 1) {
        
          if(freeBlockAlreadyExists) printf(", ");

          freeBlockAlreadyExists = true;
          activeRange = true;
          

        printf("%d", (i * 8) + j + baseBit);

      } 


      if((((currentBit && currentBit != previousBit && freeBlockAlreadyExists ) || ((i == blockSize -1) && j == 7)) && activeRange) && rangeLength == 1) {

        activeRange = 0;
        rangeLength = 0;

      } else if(((currentBit && currentBit != previousBit && freeBlockAlreadyExists ) || ((i == blockSize -1) && j == 7)) && activeRange) {
          
        if(j == 0) {

          printf("-%d", ((i - 1) * 8) + 7 + baseBit);

        } else {

          printf("-%d", (i * 8) + j + baseBit);

        }

        activeRange = false;
        rangeLength = 0;

      }

      previousBit = currentBit;
    }
  }
  
  printf("\n\n");

  free(inodeBitmap);
}

void rootDirEntries(int fd, unsigned int blockSize, struct ext2_super_block *superBlock, struct ext2_group_desc * groupDescriptions) {

  unsigned int firstOffset = ((groupDescriptions->bg_inode_table ) * blockSize) + sizeof(struct ext2_inode);
  unsigned int secondOffset = 0;
  unsigned int lseekRV = 0;
  struct ext2_inode *rootInode = NULL;
  struct ext2_dir_entry_2 *rootEntry = NULL;
  unsigned char *blockFound = NULL;
  int rv = 0;
  unsigned int currentSize = 0;


  blockFound = malloc(blockSize);
  rootInode = malloc(sizeof(struct ext2_inode));

  if (blockFound == NULL || rootInode == NULL ) {
    fprintf (stderr, "%s: Error in malloc\n", "blockFound");
    exit(1);
  }

  lseekRV = lseek(fd, firstOffset, SEEK_SET);
 
  if(lseekRV != firstOffset) {
    perror("rootDirEntries: File seek 1 failed");
    exit(1);
  }

  rv = read(fd, rootInode, sizeof(struct ext2_inode));
  
  if (rv == -1) {
    perror("rootDirEntries: File read 1 failed");
    exit(1);
  }

  secondOffset = ((rootInode->i_block[0]) *  blockSize);

  if (S_ISDIR(rootInode->i_mode)) {

		lseekRV = lseek(fd, secondOffset, SEEK_SET);

    if(lseekRV != secondOffset) {
      perror("rootDirEntries: File seek 2 failed");
      exit(1);
    }

		read(fd, blockFound, blockSize);

    if (rv == -1) {
      perror("rootDirEntries: File read 2 failed");
      exit(1);
    }

		rootEntry = (struct ext2_dir_entry_2 *) blockFound;
  
    printf("--Root Directory Entries-- \n");
		while((currentSize < rootInode->i_size) && rootEntry->inode) {

      printf("Inode: %u \n", rootEntry->inode);
      printf("Entry Length: %u \n", rootEntry->rec_len);
      printf("Name Length: %u \n", rootEntry->name_len);
      printf("File Type: %u \n", rootEntry->file_type);
      printf("Name: %s \n\n", rootEntry->name);

			currentSize = currentSize + rootEntry->rec_len;
			rootEntry = rootEntry->rec_len + (void*) rootEntry;

		}

		free(blockFound);
    free(rootInode);
	}
}