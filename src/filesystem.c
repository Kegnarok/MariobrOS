#include "filesystem.h"

/* lba n means sector n, with sector_size = 0x200 = 512 bytes. The volume starts
 * at 1M = 0x10000 in memory. Thus, lba n is the address 512 * n from the 
 * beginning  of the volume, that is at the physical address 0x10000 + n * 0x200.
 * The superpblock is at lba 2, so 0x10400 is the position of block 0.
 * Each block is 2048 = 256 * 8 bytes wide.
 */


superblock_t *spb = 0; // Superblock address
bgp_t *bgpt = 0; // Block group descriptor table
u_int32 block_factor = 0; // Only used in the definition of the BLOCK macro
u_int32 block_size = 0; // block size in words (NOT in bytes)
u_int16 *std_buf = 0;
/* The above standard buffer is set up with the size of a block using mem_alloc.
 * Its purpose is to give access to memory within a function, without a call
 * to mem_alloc each time such a buffer is needed.
 * Warning: its content may be modified by any of the following elementary
 * functions, so do not use it while calling such functions. */
inode_t *std_inode = 0;
/* Similar to std_buf, but for inodes */

/**
 *  @name BLOCK - Used in readPIO, readLBA and writeLBA to access a block
 *  @param n    - The block number to access
 */
#define BLOCK(n) (block_factor*(n))

void find_inode(u_int32 inode, inode_t *buffer)
{
  u_int32 block_group = (inode-1) / spb->inode_per_group;
  u_int32 index = (inode-1) % spb->inode_per_group;
  readPIO(BLOCK(bgpt[block_group].inode_table_address), \
          index*sizeof(inode_t)/2, sizeof(inode_t)/2, (u_int16*) buffer);
}


void update_block(u_int16* buffer, u_int32 ofs, u_int32 length, u_int32 address)
{
  readLBA(address, block_factor, std_buf);
  for(u_int32 pos = ofs; pos < ofs + length; pos++) {
    std_buf[pos] = buffer[pos-ofs];
  }
  writeLBA(address, block_factor, std_buf);
}

void update_or_bitmap(u_int32 address, u_int8 word, u_int16 mask)
{
  readLBA(address, 1, std_buf);
  std_buf[word] |= mask;
  writeLBA(address, 1, std_buf);
}


u_int32 allocate_inode()
{
  if(!(spb->unalloc_inode_num && spb->first_free_inode)) {
    return 0;
  }
  u_int32 ret = spb->first_free_inode;
  spb->unalloc_inode_num--;

  u_int32 k = spb->first_free_inode>>11; // Current checked sector
  u_int8 i = (u_int8) (spb->first_free_inode % 2048)>>4; // Current checked word
  u_int8 j = (u_int8) spb->first_free_inode % 16; // Current checked bit

  u_int32 address=bgpt[(k<<11)/spb->inode_per_group].inode_address_bitmap;
  update_or_bitmap(BLOCK(address) + k/2, i + ((k%2)<<8), (1<<j));
  j++;
  bgpt[(k<<11)/spb->inode_per_group].unalloc_inode--;

  if(! bgpt[(k<<11)/spb->inode_per_group].unalloc_inode) {
    for(; i < 128; i++) {
      // The first loop is inlined so as not to re-read the bitmap
      if(std_buf[i]!=0xff) { // std_buf has been set up in update_or_bitmap
        for(; j < 16; j++) {
          if(!(std_buf[i] & (1<<j))) {
            spb->first_free_inode = (k<<11) + (i<<4) + j;
            return ret;
          }
        }
      }
    }
  }

  for(k++; k < (spb->inode_num)>>11; k++) {
    if(! bgpt[(k<<11)/spb->inode_per_group].unalloc_inode) {
      continue;
    }
    address = bgpt[(k<<11) / spb->inode_per_group].inode_address_bitmap;
    readPIO(BLOCK(address), k<<8, 256, std_buf);
    for(i=0; i < 128; i++) {
      if(std_buf[i]!=0xff) {
        for(j = 0; j < 16; j++) {
          if(!(std_buf[i] & (1<<j))) {
            spb->first_free_inode = (k<<11) + (i<<4) + j;
            return ret;
          }
        }
      }
    }
  }

  spb->first_free_inode = 0; // No next free inode
  return ret;
}

u_int8 unallocate_inode(u_int32 inode)
{
  if(inode < spb->first_free_inode) {
    spb->first_free_inode = inode;
  }
  u_int32 address = bgpt[(inode-1) / spb->inode_per_group].inode_address_bitmap;
  readLBA(BLOCK(address), 1, std_buf);
  if(! (std_buf[((inode-1)%2048) / 16] & (1<<((inode-1)%16))) ) {
    return 1; // Not allocated inode
  }
  spb->unalloc_inode_num++;
  bgpt[(inode-1) / spb->inode_per_group].unalloc_inode++;
  std_buf[((inode-1)%2048) / 16] &= ~(1<<((inode-1)%16));
  writeLBA(BLOCK(address), 1, std_buf);
  return 0;
}

u_int8 unallocate_block(u_int32 block)
{
  u_int32 address = bgpt[block / spb->block_per_group].block_address_bitmap;
  readLBA(BLOCK(address), 1, std_buf);
  if(! (std_buf[(block%2048) / 16] & (1<<(block%16)) )) {
      return 1; // Not allocated block
  }
  bgpt[block / spb->block_per_group].unalloc_block++;
  std_buf[(block%2048) / 16] &= ~(1<<(block%16));
  writeLBA(BLOCK(address), 1, std_buf);
  return 0;
}

u_int32 allocate_block(u_int32 prec)
{
  if(!spb->unalloc_block_num) {
    return 0;
  }
  spb->unalloc_block_num--;

  u_int32 k = prec>>11; // Current checked sector
  u_int8 i = (u_int8) (prec % 2048)>>4; // Current checked word
  u_int8 j = (u_int8) prec % 16; // Current checked bit

  u_int32 address = bgpt[(k<<11)/spb->block_per_group].block_address_bitmap;

  readLBA(BLOCK(address) + k/2, 1, std_buf);
  if(bgpt[(k<<11)/spb->block_per_group].unalloc_block) {
    for(; i < 128; i++) {
      // The first loop is inlined so as not to re-read the bitmap
      if(std_buf[i]!=0xff) {
        for(; j < 16; j++) {
          if(!(std_buf[i] & (1<<j))) {
            bgpt[(k<<11)/spb->block_per_group].unalloc_block--;
            update_or_bitmap(BLOCK(address) + k/2, i + ((k%2)<<8), (1<<j));
            return (k<<11) + (i<<4) + j;
          }
        }
      }
    }
  }

  for(k++; k < (spb->block_num)>>11; k++) {
    if(! bgpt[(k<<11)/spb->block_per_group].unalloc_block) {
      continue;
    }
    address = bgpt[(k<<11) / spb->block_per_group].block_address_bitmap;
    readLBA(BLOCK(address) + k/2, 1, std_buf);
    for(i=0; i < 128; i++) {
      if(std_buf[i]!=0xff) {
        for(j = 0; j < 16; j++) {
          if(!(std_buf[i] & (1<<j))) {
            std_buf[i + ((k%2)<<8)] |= (1<<j);
            bgpt[(k<<11)/spb->block_per_group].unalloc_block--;
            return (k<<11) + (i<<4) + j;
          }
        }
      }
    }
  }

  if(!prec) {
    throw("Superblock corrupted, no free block found");
  } else {
    return allocate_block(0); // Restart from the beginning
  }
}



/* The following function does not use std_buf if offset+length<12*block_size */
u_int32 read_inode_data(u_int32 inode, u_int16* buffer, u_int32 offset, \
                        u_int32 length)
{
  u_int32 width; // Length read, in words (NOT in bytes)
  u_int32 to_read = offset / block_size;
  find_inode(inode, std_inode);
  u_int32 ofs = offset % block_size;

  if(length > block_size - ofs) {
    width = block_size - ofs;
  } else {
    width = length;
  }

  if(to_read < 12) {
    readPIO(BLOCK(std_inode->dbp[to_read]), ofs, width, buffer);
  } else {
    to_read -= 12;
    if(to_read < block_size / 2) {
      readPIO(BLOCK(std_inode->sibp), 0, block_size, std_buf);
      readPIO(BLOCK(((u_int32*)std_buf)[to_read]), ofs, width, buffer);
    } else {
      to_read -= block_size / 2;
      u_int32 dbsize = block_size*block_size;
      if(to_read < dbsize / 4) {
        readPIO(BLOCK(std_inode->dibp), 0, block_size, std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[to_read/block_size]), 0,  \
                block_size, std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[to_read % block_size]), \
                ofs, width, buffer);
      } else {
        to_read -= dbsize / 4;
        if(to_read >= dbsize*block_size / 8) {
          throw("Invalid offset");
        }
        readPIO(BLOCK(std_inode->tibp), 0, block_size, std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[to_read/dbsize]), 0, block_size, \
                std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[(to_read%dbsize)/block_size]), 0, \
                block_size, std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[to_read % block_size]), \
                ofs, width, buffer);
      }
    }
  }
  return width;
}

u_int32 write_inode_data(u_int32 inode, u_int16* buffer, u_int32 offset, \
                         u_int32 length)
{
  u_int32 width; // Length written, in words (NOT in bytes)
  u_int32 to_write = offset / block_size;
  find_inode(inode, std_inode);
  u_int32 ofs = offset % block_size;

  if(length > block_size - ofs) {
    width = block_size - ofs;
  } else {
    width = length;
  }

  if(to_write < 12) {
    update_block(buffer, ofs, length, BLOCK(std_inode->dbp[to_write]));
  } else {
    u_int32 address;
    to_write -= 12;
    if(to_write < block_size / 2) {
      readPIO(BLOCK(std_inode->sibp), 0, block_size, std_buf);
      address = BLOCK(((u_int32*)std_buf)[to_write]);
      update_block(buffer, ofs, length, address);
    } else {
      to_write -= block_size / 2;
      u_int32 dbsize = block_size*block_size;
      if(to_write < dbsize / 4) {
        readPIO(BLOCK(std_inode->dibp), 0, block_size, std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[to_write/block_size]), 0,  \
                block_size, std_buf);
        address = BLOCK(((u_int32*)std_buf)[to_write % block_size]);
        update_block(buffer, ofs, length, address);
      } else {
        to_write -= dbsize / 4;
        if(to_write >= dbsize*block_size / 8) {
          throw("Invalid offset");
        }
        readPIO(BLOCK(std_inode->tibp), 0, block_size, std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[to_write/dbsize]), 0, block_size, \
                std_buf);
        readPIO(BLOCK(((u_int32*)std_buf)[(to_write%dbsize)/block_size]), 0, \
                block_size, std_buf);
        address = BLOCK(((u_int32*)std_buf)[to_write % block_size]);
        update_block(buffer, ofs, length, address);
      }
    }
  }
  return width;
}

u_int32 open_file(string str_path)
{
  list_t path = (str_split(str_path, '/', TRUE)->tail);
  
  u_int32 inode = 2; // root directory
  dir_entry *entry = 0;
  int i; char* a; char* b;
  u_int32 endpos = (u_int32)std_buf + (block_size<<1);
  while(path->tail || ! (*((char*) path->head) == '\0') ) {
    read_inode_data(inode, std_buf, 0, block_size);
    entry = (void*) std_buf;
    while(entry->inode && (u_int32)entry < endpos) {
      a = (char*) path->head;
      b = (char*) &(entry->name);

      for(i=0; a[i] != '\0' && b[i] != '\0' && a[i]==b[i]; i++);
      // The difference with str_cmp is that b may not end with '\0'
      if(a[i] == b[i] || (a[i] == '\0' && i == entry->name_length)) {
        inode = entry->inode;
        break;
      }
      entry = (dir_entry*) (((u_int32)entry) + entry->size);
    }
    if((u_int32)entry == endpos) {
      while(path) {
        pop(&path);
      }
      return 0; // Not found
    }
    if(! path->tail) {
      pop(&path);
      return inode;
    }
    pop(&path);
  }
  pop(&path);
  return inode;
}

void ls_dir(u_int32 inode)
{
  read_inode_data(inode, std_buf, 0, block_size);
  dir_entry *entry = (void*) std_buf;
  u_int32 endpos = (u_int32)std_buf + (block_size<<1);
  
  while(entry->size && (u_int32)entry < endpos) {
    writef("\n-->  ");
    for(u_int8 i = 0 ; i < entry->name_length ; i++) {
      writef("%c", ((u_int8*) &(entry->name))[i]);
    }
    writef(" \t <-- inode = %u", entry->inode);
    entry = (dir_entry*) (((u_int32)entry) + entry->size);
  }
  writef("\n  _____\n");
}



void filesystem_install()
{
  set_disk(FALSE);
  
  spb = (void*) mem_alloc(sizeof(superblock_t));
  readPIO(2, 0, sizeof(superblock_t)/2, (u_int16*) spb);
  if(spb->signature!=0xef53) {
    throw("Wrong superblock signature : is this ext2 ?");
  }

  block_size = 512<<(spb->block_size);
  /* The next definition means: block_factor = block_size / 0x200*/
  block_factor = 2<<(spb->block_size);

  std_buf = mem_alloc(block_size<<1);

  u_int32 block_group_num = (spb->block_num + spb->block_per_group - 1) / \
    spb->block_per_group;
  if(block_group_num != (spb->inode_num + spb->inode_per_group - 1) /   \
     spb->inode_per_group) {
    throw("Incoherent number of block groups");
  }

  u_int32 bgpt_size = block_group_num * sizeof(bgp_t);
  bgpt = (void*) mem_alloc(bgpt_size);
  readPIO(BLOCK(1), 0, bgpt_size/2, (u_int16*) bgpt);

  std_inode = mem_alloc(sizeof(inode_t));
}
