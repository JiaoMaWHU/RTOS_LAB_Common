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
BYTE eFile_writer_buffer[512];
BYTE eFile_reader_buffer[512];
CHAR dir_next_cursor = -1;
WORD FAT_END_FLAG = SIZE_FAT_ENTRIES;
Sema4Type writerSema; // for mutex with reader
Sema4Type readerSema; // for mutex with other readers
int readerCounter = 0; // right now we only have one reader, 
											// this counter seems useless, however we save it for later usage
int writer_buf_end_id = -1;
int reader_buf_read_id = -1;
int file_written_fat_id; // fat id;
int file_read_fat_id; // fat id

// util of dirtory array
void get_dir_entry(int id, WORD* filename, WORD* pointer){
	if(filename!=NULL){
		memcpy(filename, &eFile_directory[id * BYTE_PER_DIR_ENTRY], BYTE_PER_DIR_ENTRY_NAME);
	}
	if(pointer!=NULL){
		memcpy(pointer, &eFile_directory[id * BYTE_PER_DIR_ENTRY + BYTE_PER_DIR_ENTRY_NAME], 2);
	}
}

void set_dir_entry(int id, const BYTE* filename, WORD* pointer){
	if(filename!=NULL){
		memcpy(&eFile_directory[id * BYTE_PER_DIR_ENTRY], filename, BYTE_PER_DIR_ENTRY_NAME);
	}
	if(pointer!=NULL){
		memcpy(&eFile_directory[id * BYTE_PER_DIR_ENTRY + BYTE_PER_DIR_ENTRY_NAME], pointer, 2);
	}
}

void get_free_pointer(WORD* pointer){
	memcpy(&pointer, &eFile_directory[SIZE_DIR_ENTRIES*BYTE_PER_DIR_ENTRY-2], 2);
}

void set_free_pointer(WORD* pointer){
	memcpy(&eFile_directory[SIZE_DIR_ENTRIES*BYTE_PER_DIR_ENTRY-2], &pointer, 2);
}

int cmp_dir_entry_filename(int id, const BYTE* cmp_filename){
	// return 0 if equal, otherwise logical 1
	return memcmp(&eFile_directory[id*(BYTE_PER_DIR_ENTRY)], cmp_filename, BYTE_PER_DIR_ENTRY_NAME);
}

// util of fat array

void get_fat_pointer(int id, WORD* pointer){
	memcpy(pointer, &eFile_directory[id * BYTE_PER_FAT_ENTRY], BYTE_PER_FAT_ENTRY);
}

void set_fat_pointer(int id, WORD* pointer){
	memcpy(&eFile_directory[id * BYTE_PER_FAT_ENTRY], pointer, BYTE_PER_FAT_ENTRY);
}

int cmp_fat_pointer(int id, WORD* pointer){
	// return 0 if equal, otherwise logical 1
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
	// find free blocks
	while(TRUE){
		count++;
		last = next;
		if(count==size){ // enough space, 
			set_fat_pointer(last, &FAT_END_FLAG); // last entry 
			set_free_pointer(&next);
			break;
		}
		if(!cmp_fat_pointer(next, &FAT_END_FLAG)){
			break;
		}
		get_fat_pointer(next, &next);
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
	
	// init semaphore
	OS_InitSemaphore(&writerSema, 1);
	OS_InitSemaphore(&readerSema, 1);
	readerCounter = 0;
	
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
	for(WORD i = 0; i<(SIZE_FAT_ENTRIES-1); i++){
		index = i+1;
		set_fat_pointer(i, &index);
	}
	
	// set the last point to END FLAG
	set_fat_pointer(SIZE_FAT_ENTRIES-1, &FAT_END_FLAG);
	
	// format real file in the disk
	BYTE empty_buffer[512];
	memset(empty_buffer, 0, sizeof(empty_buffer));
	for(int i=0; i<(SIZE_FAT_ENTRIES); i++){
		status = eDisk_WriteBlock(empty_buffer, i+START_BLOCK_OF_FILE);
		if(status){
			return 1;
		}
	}
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
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	
	// find the file entry in directory
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
	
	// wait writer semaphore
	OS_Wait(&writerSema);
	
	// find the index of the last block
	// at the end of loop, next will be the index of last block
	WORD next;
	get_dir_entry(i, NULL, &next);
	while(cmp_fat_pointer(next, &FAT_END_FLAG)){
		get_fat_pointer(next, &next);
	}
	
	// save file fat id
	file_written_fat_id = next;
	
	// read the last block into buffer
	status = eDisk_ReadBlock(eFile_writer_buffer, next+START_BLOCK_OF_FILE);
	if(status){
		OS_Signal(&writerSema);
		return 1;
	}
	
  return 0;   // replace  
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	
	// init writer_buf_end_id
	if(writer_buf_end_id==-1){
		int i = 0;
		while(i<512){
			if(eFile_writer_buffer[i]==END_OF_TEXT){
				break;
			}
		}
		writer_buf_end_id = i;
	}
	
  eFile_writer_buffer[writer_buf_end_id++] = data;
	
	// if the flag out of range
	// save this block, allocate a new block
	if(writer_buf_end_id==512){
		// write buffer back
		status = eDisk_WriteBlock(eFile_writer_buffer,file_written_fat_id+START_BLOCK_OF_FILE);
		memset(eFile_writer_buffer, 0, sizeof(eFile_writer_buffer));
		
		// allocate a new block
		WORD next = alloc_fat_space(1);
		// set the pointer of the end fat entry to a new entry
		set_fat_pointer(file_written_fat_id, &next);
		
		// update
		file_written_fat_id = next;
		writer_buf_end_id = 0;
	}
	if(status){
		return 1;
	}
	return 0;   // replace
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	DSTATUS status = eFile_Init();
	if(status){
		OS_Signal(&writerSema);
		return 1;
	}
	// mark the end file flag
  eFile_writer_buffer[writer_buf_end_id] = END_OF_TEXT;
	writer_buf_end_id = -1;
	
	// write buffer back
	status = eDisk_WriteBlock(eFile_writer_buffer, file_written_fat_id+START_BLOCK_OF_FILE);
	if(status){
		OS_Signal(&writerSema);
		return 1;
	}
	
	// write the fat back
	status = eFile_Close();
	if(status){
		OS_Signal(&writerSema);
		return 1;
	}
	
	OS_Signal(&writerSema);
	
  return 0;   // replace
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	
	// find the file entry in directory
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
	
	// wait writer semaphore
	OS_Wait(&readerSema);
	
	readerCounter++;
	if(readerCounter==1){
		OS_Wait(&writerSema);
	}

	// get the first block and set indices
	WORD next;
	get_dir_entry(i, NULL, &next);
	file_read_fat_id = next;
	reader_buf_read_id = 0;
	
	// load buffer
	status = eDisk_ReadBlock(eFile_reader_buffer, next+START_BLOCK_OF_FILE);
	if(status){
		readerCounter--;
		OS_Signal(&readerSema);
		// leave the releasing of writerSema to others
		return 1;
	}
	
  return 0;   // replace   
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful, and 2 on end of file, 1 for failure
int eFile_ReadNext( char *pt){       // get next byte 
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	
  if(eFile_reader_buffer[reader_buf_read_id]==END_OF_TEXT){
		return 2;
	}
	*pt = eFile_reader_buffer[reader_buf_read_id++];
	
	// load a new block and update indices
	if(reader_buf_read_id==512){
		WORD next;
		get_fat_pointer(file_read_fat_id, &next);
		status = eDisk_ReadBlock(eFile_reader_buffer, next+START_BLOCK_OF_FILE);
		if(status){
			// loading failed
			return 1;
		}
		file_read_fat_id = next;
		reader_buf_read_id = 0;
	}
  return 0;   // replace
}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	// we won't validate whether the file is opened or not
	// make sure users follow this rule
  reader_buf_read_id = -1;
	readerCounter--;
	if(readerCounter==0){
		OS_Signal(&writerSema);
	}
	OS_Signal(&readerSema);
  return 0;   // replace
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete(const char name[]){  // remove this file 
	DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	
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
	while(TRUE){
		status = eDisk_WriteBlock(empty_buffer, next+START_BLOCK_OF_FILE);
		if(status){
			return 1;
		}
		if(!cmp_fat_pointer(next, &FAT_END_FLAG)){
			break;
		}
		get_fat_pointer(next, &next);
	}
	
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
  DSTATUS status = eFile_Init();
	if(status){
		return 1;
	}
	
	// find the file entry in directory
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
	
	int size_ = 1;
	WORD next;
	get_dir_entry(i, NULL, &next);
	while(TRUE){
		size_++;
		if(!cmp_fat_pointer(next, &FAT_END_FLAG)){
			break;
		}
		get_fat_pointer(next, &next);
	}
	
	*size = size_;
  return 0;   // replace
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
  status = eDisk_Write(DRIVE_NUM, eFile_fat, 1, (SIZE_FAT_ENTRIES * BYTE_PER_FAT_ENTRY)/512);
	if(status){
		return 1;
	}
  return 0;   // replace
}
