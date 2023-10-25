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

#define NBUCKETS 13
struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf hashbucket[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  // Initialize Bcache locks
  char *lock_names[] = {"bcache0", "bcache1", "bcache2", "bcache3", "bcache4", "bcache5", 
      "bcache6", "bcache7", "bcache8", "bcache9", "bcache10", "bcache11", "bcache12" };
  for (int i = 0; i < NBUCKETS; i++) 
    initlock(&bcache.lock[i], lock_names[i]);

  // Create linked list of buffers
  for (int i = 0; i < NBUCKETS; i++)
  {
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++) {
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *lru;
  int id = blockno % NBUCKETS;

  acquire(&bcache.lock[id]);

  // Is the block already cached?
  lru = 0;
  for (b = bcache.hashbucket[id].prev; b != &bcache.hashbucket[id]; b = b->prev){
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
    if (!lru && b->refcnt == 0)
      lru = b;
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // If able to do in the same bucket.
  if (lru)
  {
    lru->dev = dev;
    lru->blockno = blockno;
    lru->valid = 0;
    lru->refcnt = 1;
    release(&bcache.lock[id]);
    acquiresleep(&lru->lock);
    return lru;
  }

  // temporarily release lock
  release(&bcache.lock[id]);

  // Find buffer in other buckets.
  for (int i = 0; i < NBUCKETS; i++)
  {
    if (i == id)
      continue;

    acquire(&bcache.lock[i]);

    for (b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        // move b out of bucket i
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[i]);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // put b into bucket id
        acquire(&bcache.lock[id]);
        b->next = bcache.hashbucket[id].next;
        b->prev = &bcache.hashbucket[id];
        bcache.hashbucket[id].next->prev = b;
        bcache.hashbucket[id].next = b;
        release(&bcache.lock[id]);
        acquiresleep(&b->lock);
        return b;
      }
    }

    release(&bcache.lock[i]);
  }
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
  int id = b->blockno % NBUCKETS;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[id].next;
    b->prev = &bcache.hashbucket[id];
    bcache.hashbucket[id].next->prev = b;
    bcache.hashbucket[id].next = b;
  }
  
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  int id = b->blockno % NBUCKETS;

  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  int id = b->blockno % NBUCKETS;

  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}


