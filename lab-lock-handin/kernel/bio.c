// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define MAX_HASH_BUCKETS 13
#define NULL 0
extern uint ticks;

struct {
  struct buf buf[NBUF]; 
  struct spinlock lock;
  // hash table 
  struct spinlock hashlock[MAX_HASH_BUCKETS];
  struct buf hashtable[MAX_HASH_BUCKETS];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

static int bhash(int blockno);
static int btime();

void
binit(void)
{
	struct buf *b;
	// init buffer lock and init free list buffer
	// initial the hash table and hash lock
	for (int i = 0; i < MAX_HASH_BUCKETS; ++i) {
		initlock(&bcache.hashlock[i], "hashlock");
		// Create linked list of buffers
		bcache.hashtable[i].prev = &bcache.hashtable[i];
		bcache.hashtable[i].next = &bcache.hashtable[i];
	}
	initlock(&bcache.lock, "bcache");
	for(b = bcache.buf; b < bcache.buf + NBUF; b++){
		initsleeplock(&b->lock, "bufferlock");
		b->timestamp = 0;
		b->dev = -1;
		b->blockno = -1;
		b->refcnt = 0;
		b->next = bcache.hashtable[0].next;
		b->prev = &bcache.hashtable[0];
		bcache.hashtable[0].next->prev = b;
		bcache.hashtable[0].next = b;
	}
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf * lrub = 0;
  int minticks = -1;
  int no = bhash(blockno);
  
  acquire(&bcache.hashlock[no]);
  // Is the block already cached?
  for(b = bcache.hashtable[no].next; b != &bcache.hashtable[no]; b = b->next){
	if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hashlock[no]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.hashlock[no]);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.lock);
  acquire(&bcache.hashlock[no]);
  for(b = bcache.hashtable[no].next; b != &bcache.hashtable[no]; b = b->next){
	if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hashlock[no]);
	  release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
	if(b->refcnt == 0 && minticks >= b->timestamp) {
		lrub = b;
		minticks = b->timestamp;
	}
  }
  if (lrub) {
  	lrub->dev = dev;
	lrub->blockno = blockno;
    lrub->valid = 0;
	lrub->refcnt = 1;
	release(&bcache.hashlock[no]);
	release(&bcache.lock);
    acquiresleep(&lrub->lock);
	return lrub;
  }

  // we steal a buffer block from other buckets
  for (int i = 1; i < MAX_HASH_BUCKETS; ++i) {
	int newno = bhash(no + i);
	acquire(&bcache.hashlock[newno]);
  	for (b = bcache.hashtable[newno].prev; b != &bcache.hashtable[newno]; b = b->prev){
		if(b->refcnt == 0 && minticks >= b->timestamp) {
			lrub = b;
			minticks = b->timestamp;
		}
	}
	if (lrub) {
	  	lrub->dev = dev;
		lrub->blockno = blockno;
	    lrub->valid = 0;
		lrub->refcnt = 1;
		lrub->next->prev = lrub->prev;
		lrub->prev->next = lrub->next;
		lrub->next = bcache.hashtable[no].next;
		lrub->prev = &bcache.hashtable[no];
		bcache.hashtable[no].next->prev = lrub;
    	bcache.hashtable[no].next = lrub;
		release(&bcache.hashlock[newno]);
		release(&bcache.hashlock[no]);
		release(&bcache.lock);
	    acquiresleep(&lrub->lock);
		return lrub;
	}
  	release(&bcache.hashlock[newno]);
  }
  release(&bcache.hashlock[no]);
  release(&bcache.lock);
  
  // we check the the cache again and the one block must atomic  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  
  releasesleep(&b->lock);
  int no = bhash(b->blockno);
  acquire(&bcache.hashlock[no]);
  b->refcnt--;
  if (b->refcnt == 0) {
 	b->timestamp = btime();
  } 	
  release(&bcache.hashlock[no]);
  // bdegub();
}

void
bpin(struct buf *b) {
  int no = bhash(b->blockno);
  acquire(&bcache.hashlock[no]);
  b->refcnt++;
  release(&bcache.hashlock[no]);
}

void
bunpin(struct buf *b) {
  int no = bhash(b->blockno);
  acquire(&bcache.hashlock[no]);
  b->refcnt--;
  release(&bcache.hashlock[no]);
}

static int bhash(int blockno) {
  return blockno%MAX_HASH_BUCKETS;
}

static int btime(){
  return ticks;
}

#if 0
static void bdegub(){
  for (int i = 0; i < MAX_HASH_BUCKETS; ++i) {
  	struct buf *b;
	acquire(&bcache.hashlock[i]);
    printf("block : %d================\n",i);
	for (b = bcache.hashtable[i].next; b != &bcache.hashtable[i]; b = b->next) {
		printf("%p %p %p\n",b->prev,b,b->next);
		printf("dev:%d block:%d refcnt:%d timestamp:%d\n",b->dev,b->blockno,b->refcnt,b->timestamp);
	}
	release(&bcache.hashlock[i]);
  }
}
#endif

