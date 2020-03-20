// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/eDisk.h"
#include <stdio.h>
#include <string.h>

BYTE eFile_directory[SIZE_DIR_ENTRIES * BYTE_PER_DIR_ENTRY]; // total 512B
BYTE eFile_fat[SIZE_FAT_ENTRIES * BYTE_PER_FAT_ENTRY]; // total 4KB

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Init(void){ // initialize file system
	DSTATUS status;
	status = eDisk_Status(DRIVE_NUM);
	if(status!=STA_NOINIT){
		return 0;
	}
	// init eDisk
	status = eDisk_Init(DRIVE_NUM);
	if(status){
		return 1;
	}
	return 0;
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	memset(eFile_directory, 0, sizeof(eFile_directory));
	memset(eFile_directory, 0, sizeof(eFile_fat));
	
	// set the first directory entry
//	WORD first_dir_entry = 0;
//	memcpy(&eFile_directory[6], &first_dir_entry, 2);
	
	// format the fat
	int index = 0;
	for(int i = 0; i<(SIZE_FAT_ENTRIES-1); ){
		index = i*2;
		i++;
		memcpy(&eFile_fat[index], &i, 2);
	}
	
	// write back, we don't need to format the actual data
	DSTATUS status = eDisk_WriteBlock(eFile_directory, 0);
	if(status){
		return 1;
	}
	// second -> FAT, 8
	status = eDisk_Write(DRIVE_NUM, eFile_fat, 1, (SIZE_FAT_ENTRIES * BYTE_PER_FAT_ENTRY)/512);
	if(status){
		return 1;
	}
  return 0;   // replace
}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
	// read the disk and init FAT and directory
	// first -> directorty, 1
	DSTATUS status = eDisk_ReadBlock(eFile_directory, 0);
	if(status){
		return 1;
	}
	// second -> FAT, 8
	status = eDisk_Read(DRIVE_NUM, eFile_fat, 1, (SIZE_FAT_ENTRIES * BYTE_PER_FAT_ENTRY)/512);
	if(status){
		return 1;
	}
  return 0;
}


//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 

  return 1;   // replace
}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 

  return 1;   // replace  
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){
  
    return 1;   // replace
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
  
  return 1;   // replace
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 

  return 1;   // replace   
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 
  
  return 1;   // replace
}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
  
  return 1;   // replace
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 

  return 1;   // replace
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
   
  return 1;   // replace
}
  
//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
   
  return 1;   // replace
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
   
  return 1;   // replace
}


//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){ 
   
  return 1;   // replace
}
