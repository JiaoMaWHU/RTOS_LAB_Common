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
BYTE emptyFileName[BYTE_PER_DIR_ENTRY_NAME];
BYTE eFile_file_buffer[512];
CHAR dir_next_cursor = -1;
WORD FAT_END_FLAG = SIZE_FAT_ENTRIES;

void get_dir_entry(int id, WORD* filename, WORD* pointer){
	memcpy(filename, &eFile_directory[id * BYTE_PER_DIR_ENTRY], BYTE_PER_DIR_ENTRY_NAME);
	memcpy(pointer, &eFile_directory[id * BYTE_PER_DIR_ENTRY + BYTE_PER_DIR_ENTRY_NAME], 2);
}

void set_dir_entry(int id, const BYTE* filename, WORD* pointer){
	memcpy(&eFile_directory[id * BYTE_PER_DIR_ENTRY], filename, BYTE_PER_DIR_ENTRY_NAME);
	memcpy(&eFile_directory[id * BYTE_PER_DIR_ENTRY + BYTE_PER_DIR_ENTRY_NAME], pointer, 2);
}

void get_free_pointer(WORD* pointer){
	memcpy(&pointer, &eFile_directory[SIZE_DIR_ENTRIES*BYTE_PER_DIR_ENTRY-2], 2);
}

void set_free_pointer(WORD* pointer){
	memcpy(&eFile_directory[SIZE_DIR_ENTRIES*BYTE_PER_DIR_ENTRY-2], &pointer, 2);
}

int cmp_dir_entry_filename(int id, const BYTE* cmp_filename){
	return memcmp(&eFile_directory[id*(BYTE_PER_DIR_ENTRY)], cmp_filename, BYTE_PER_DIR_ENTRY_NAME);
}

void get_fat_pointer(int id, WORD* pointer){
	memcpy(pointer, &eFile_directory[id * BYTE_PER_FAT_ENTRY], BYTE_PER_FAT_ENTRY);
}

void set_fat_pointer(int id, WORD* pointer){
	memcpy(&eFile_directory[id * BYTE_PER_FAT_ENTRY], pointer, BYTE_PER_FAT_ENTRY);
}

int cmp_fat_pointer(int id, WORD* pointer){
	return memcmp(&eFile_directory[id * BYTE_PER_FAT_ENTRY], pointer, BYTE_PER_FAT_ENTRY);
}

//---------- alloc_fat_space-----------------
// Allocate free space in fat
// Input: size of free space
// Output: end_flag if no enough space, otherwise the start index
WORD alloc_fat_space(int size){
	WORD free_space_id;
	get_free_pointer(&free_space_id);
	WORD next = free_space_id;
	WORD last;
	int count = 0;
	
	// end flag
	while(!cmp_fat_pointer(next, &FAT_END_FLAG)){
		last = next;
		get_fat_pointer(next, &next);
		count++;
		if(count==size){
			set_fat_pointer(last, &FAT_END_FLAG);
			set_free_pointer(&next);
			break;
		}
	}
	
	// no enough space
	if(count<size){
		return FAT_END_FLAG;
	}
	
	return free_space_id;
}

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
	memset(emptyFileName, 0, sizeof(emptyFileName));
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
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}

	memset(eFile_directory, 0, sizeof(eFile_directory));
	memset(eFile_fat, 0, sizeof(eFile_fat));
	
	// set the first directory entry
//	WORD first_dir_entry = 0;
//	memcpy(&eFile_directory[6], &first_dir_entry, 2);
	
	// format the fat
	WORD index = 0;
	for(WORD i = 0; i<(SIZE_FAT_ENTRIES-1); ){
		index = i;
		i++;
		set_fat_pointer(index, &i);
	}
	
	// set the last point to END FLAG
	set_fat_pointer(SIZE_FAT_ENTRIES-1, &FAT_END_FLAG);
	
	status = eFile_Close();
	if(status){
		return 1;
	}
	
	status = eFile_DClose();
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
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
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
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	
	// fill one entry, select the first
	WORD index;
	for(index = 0; index<SIZE_DIR_ENTRIES-1; index++){
		if(!cmp_dir_entry_filename(index, emptyFileName)){
			break;
		}
	}
	// full directory
	if(index == SIZE_DIR_ENTRIES-1){
		return 1;
	}
	
	// get one block free space
	WORD start = alloc_fat_space(1);
	if(start==(SIZE_FAT_ENTRIES)){
		return 1;
	}
	set_dir_entry(index, (BYTE*)name, &start);
	
  return 0;   // replace
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
int eFile_Delete(const char name[]){  // remove this file 
	int i;
	for(i=0; i<SIZE_DIR_ENTRIES-1; i++){
		if(!cmp_dir_entry_filename(i, (BYTE*)name)){
			break;
		}
	}
	if(i==(SIZE_DIR_ENTRIES-1)){
		// no such file
		return 1;
	}
	
	// clear file block
	WORD start;
	memcpy(&start, &eFile_directory[i * BYTE_PER_DIR_ENTRY + BYTE_PER_DIR_ENTRY_NAME], 2);
	WORD next = start;
	BYTE empty_buffer[512];
	memset(empty_buffer, 0, sizeof(empty_buffer));
	while(!cmp_fat_pointer(next, &FAT_END_FLAG)){
		eDisk_WriteBlock(empty_buffer, next+START_BLOCK_OF_FILE);
		get_fat_pointer(next, &next);
	}
	eDisk_WriteBlock(empty_buffer, next);
	
	// free FAT space
	WORD free_space_id;
	get_free_pointer(&free_space_id);
	set_fat_pointer(next, &free_space_id);
	set_free_pointer(&start);
	
	// clear directory
	memset(&eFile_directory[i*BYTE_PER_DIR_ENTRY], 0, BYTE_PER_DIR_ENTRY);
  return 0;   // replace
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen(void){ // open directory
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
  status = eDisk_ReadBlock(eFile_directory, 0);
	if(status){
		return 1;
	}
  return 0;   // replace
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
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
  status = eDisk_WriteBlock(eFile_directory, 0);
	if(status){
		return 1;
	}
  return 0;   // replace
}


//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
  status = eDisk_Write(DRIVE_NUM, eFile_fat, ((SIZE_DIR_ENTRIES*BYTE_PER_DIR_ENTRY)/512)-1,
			(SIZE_FAT_ENTRIES * BYTE_PER_FAT_ENTRY)/512);
	if(status){
		return 1;
	}
  return 0;   // replace
}
