---
theme: default
title: "Week 13 — Lab: File System Internals"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 13 — File System Internals

---

layout: section

---

# Lab Overview

---

# Lab Overview

- **Goal**: Analyze the xv6 file system layer by layer, trace file operations end-to-end
- **Duration**: ~50 minutes
- **4 Exercises**:
  1. Buffer cache — LRU replacement, sleep-locks, cache hits/misses
  2. Logging system — write-ahead logging, 4-stage commit, crash recovery
  3. File creation tracing — complete call chain from `sys_open` to `dirlink`
  4. Large file support — direct, single/double/triple indirect blocks and size limits
- **Reference**: `kernel/bio.c`, `kernel/log.c`, `kernel/fs.c`, `kernel/sysfile.c`

---

# xv6 File System Layers

```
┌─────────────────────────────┐
│  6. File Descriptors         │  open(), read(), write(), close()
├─────────────────────────────┤
│  5. Pathnames                │  namei(), nameiparent()
├─────────────────────────────┤
│  4. Directories              │  dirlookup(), dirlink()
├─────────────────────────────┤
│  3. Inodes                   │  ialloc(), iget(), iput(), readi(), writei()
├─────────────────────────────┤
│  2. Logging                  │  begin_op(), log_write(), end_op()
├─────────────────────────────┤
│  1. Buffer Cache             │  bread(), bwrite(), brelse()
└─────────────────────────────┘
          │
     Block Device (virtio disk)
```

- Each layer relies only on the layer below it
- Crash safety is the responsibility of the **logging** layer
- The **buffer cache** ensures only one in-memory copy of each disk block exists at a time

---

layout: section

---

# Exercise 1

---

# Exercise 1: Buffer Cache

**Purpose**: Cache disk blocks in memory; serialize access so only one copy exists per block.

**Key functions in `kernel/bio.c`**:

| Function | Role |
|---|---|
| `binit()` | Initialize the cache; link all `buf` structs into a doubly-linked LRU list |
| `bget()` | Return cached block or evict LRU unused entry |
| `bread()` | Call `bget()`; read from disk if not valid |
| `bwrite()` | Write buffer to disk |
| `brelse()` | Release buffer; move to MRU end of LRU list |

**LRU replacement**:
- `buf.refcnt == 0` means the buffer is evictable
- `bget()` scans from MRU → LRU to find the least recently used evictable block
- `brelse()` moves the buffer to the head (MRU end) after use

**Sleep-locks**: each buffer holds a `sleeplock`; only one process may hold a buffer at a time — others sleep until it is released.

---

layout: section

---

# Exercise 2

---

# Exercise 2: Logging System

**Purpose**: Write-ahead logging (WAL) guarantees that disk writes are atomic across crashes.

**4-stage commit cycle**:

```
begin_op()        ← increment outstanding op count; wait if log is full
  │
log_write(b)      ← record block number; pin buffer in cache
  │
end_op()          ← decrement count; if last op, call commit()
  │
  └─ commit()
       ├─ write_log()       write modified blocks to log region on disk
       ├─ write_head()      write log header to disk (makes commit durable)
       ├─ install_trans()   copy log blocks to their real disk locations
       └─ write_head(0)     clear log header (marks log as empty)
```

**Crash recovery** (`recover_from_log()` at boot):
- If log header shows a committed transaction, re-run `install_trans()`
- If no committed header, discard log — disk is already consistent

**Key invariant**: never write a file-system block to its real location until the entire transaction is committed and the log header is on disk.

---

layout: section

---

# Exercise 3

---

# Exercise 3: File Creation Tracing

**Full call chain when `open("newfile", O_CREATE)` is called:**

```
sys_open()                   ← kernel/sysfile.c
  └─ create()                ← kernel/sysfile.c
       ├─ nameiparent()      resolve parent directory inode
       ├─ dirlookup()        check that name does not already exist
       ├─ ialloc()           allocate a new inode on disk (type = T_FILE)
       │    └─ log_write()   log the new inode block
       ├─ ilock() / iunlock()
       ├─ dirlink()          add (name, inum) entry to parent directory
       │    └─ writei()      write directory entry
       │         └─ log_write()
       └─ filealloc()        allocate a file descriptor struct
            └─ fdalloc()     assign fd number in proc->ofile[]
```

**Task**: Set a breakpoint at `ialloc()` in GDB and verify the inode type and device number are set correctly before `log_write()` is called.

---

layout: section

---

# Exercise 4

---

# Exercise 4: Large File Support

**xv6 inode block layout** (`struct dinode` in `kernel/fs.h`):

```
addrs[0..11]   → 12 direct blocks          (12 × 1 KB  =    12 KB)
addrs[12]      → single indirect block     (256 × 1 KB =   256 KB)
addrs[13]      → double indirect block     (256² × 1 KB = 64 MB)   ← add this
addrs[14]      → triple indirect block     (256³ × 1 KB = 16 GB)   ← optional
```

**Block pointer counts** (1 KB block, 4-byte block numbers → 256 pointers/block):

| Level | Formula | Size |
|---|---|---|
| Direct | 12 blocks | 12 KB |
| Single indirect | 256 blocks | 256 KB |
| Double indirect | 256 × 256 | 64 MB |
| Triple indirect | 256 × 256 × 256 | 16 GB |

**Task**: Modify `bmap()` in `kernel/fs.c` to support double-indirect blocks. Update `MAXFILE` and `NDIRECT` constants accordingly. Verify with a test that writes more than 268 blocks to a single file.

---

# Key Takeaways

**Buffer cache**
- Single in-memory copy per disk block; LRU eviction; sleep-locks for mutual exclusion

**Logging**
- Write-ahead log makes multi-block updates crash-safe
- `commit()` is the point of no return — after `write_head()`, recovery will always re-apply the transaction

**File creation**
- Touches at least two disk structures: the new inode block and the parent directory block
- Both writes are wrapped in `begin_op()` / `end_op()` so they appear atomic

**Large files**
- Indirect block levels multiply capacity quadratically/cubically
- Changing `NDIRECT` affects both `struct dinode` and `struct inode` — update both

> The file system is the most complex subsystem in xv6. Reading it layer by layer, as these exercises do, is the most effective way to build a complete mental model.
