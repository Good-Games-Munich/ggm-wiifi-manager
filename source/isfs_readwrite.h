/*
Code used with Permission from https://github.com/Bazmoc/Wii-Network-Config-Editor/
These functions are used for reading and writing to the NAND(isfs)
*/

#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <stdlib.h>
#include <ogc/isfs.h>


u8 *ISFS_GetFile(u8 *path, u32 *size, s32 length);

u8 *ISFS_WRITE_CONFIGDAT(u8 *buf2);