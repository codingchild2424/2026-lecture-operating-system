# Week 13 Lab: 저장 장치 관리 / 파일 시스템

> **교재**: xv6 Chapter 8 - File System
> **소요 시간**: 약 50분
> **유형**: 실습 (Lab only)

---

## 학습 목표

1. xv6 파일 시스템의 **계층 구조**(layered design)를 이해한다.
2. **Buffer cache**가 디스크 블록을 어떻게 캐싱하고 동기화하는지 분석한다.
3. **Logging 시스템**이 crash recovery를 어떻게 보장하는지 이해한다.
4. **Inode**를 통한 파일 생성/읽기/쓰기 과정을 코드 수준에서 추적한다.
5. xv6의 파일 크기 제한과 확장 가능성을 계산한다.

---

## 배경 지식

### 파일 시스템 계층 구조

xv6 파일 시스템은 아래와 같이 **6개 계층**으로 구성된다.
각 계층은 바로 아래 계층의 서비스만 사용하며, 관심사를 분리한다.

```
  +---------------------------------------------------------+
  |                  사용자 프로그램                           |
  |              (open, read, write, close)                  |
  +---------------------------------------------------------+
                           |
                    시스템 콜 경계
                           |
  +---------------------------------------------------------+
  |  [6] File descriptor 계층   (kernel/file.c, sysfile.c)  |
  |      - 파일 디스크립터 테이블 관리                         |
  |      - read/write 오프셋 추적                             |
  |      - 파이프, 디바이스, inode 파일 통합 인터페이스          |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [5] Pathname 계층         (kernel/fs.c: namei, namex)  |
  |      - 경로명 "/" 로 분리하여 순회                         |
  |      - 각 요소를 디렉토리에서 탐색                         |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [4] Directory 계층        (kernel/fs.c: dirlookup,     |
  |      - 디렉토리 = 특별한 inode (dirent 배열)    dirlink) |
  |      - 이름 -> inode 번호 매핑                            |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [3] Inode 계층            (kernel/fs.c: ialloc, readi, |
  |      - 파일 메타데이터 (type, size, nlink)       writei) |
  |      - 데이터 블록 매핑 (direct + indirect)               |
  |      - 블록 할당/해제 (balloc, bfree)                     |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [2] Logging 계층          (kernel/log.c)               |
  |      - 트랜잭션: begin_op() / end_op()                   |
  |      - Write-ahead logging으로 crash recovery 보장       |
  |      - log_write()가 bwrite()를 대체                     |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [1] Buffer cache 계층     (kernel/bio.c)               |
  |      - 디스크 블록의 메모리 캐시                           |
  |      - bread() / bwrite() / brelse()                    |
  |      - LRU 교체 정책, sleep-lock 동기화                   |
  +---------------------------------------------------------+
                           |
  +---------------------------------------------------------+
  |  [0] Disk 드라이버          (kernel/virtio_disk.c)      |
  |      - virtio 프로토콜로 실제 디스크 I/O 수행              |
  +---------------------------------------------------------+
```

### xv6 디스크 레이아웃

xv6의 디스크는 고정 크기 블록(1024 bytes)의 배열로 구성된다.
`mkfs` 도구가 아래와 같은 레이아웃으로 디스크 이미지를 생성한다.

```
블록 번호:  0      1      2                  33     45    46       2000
         +------+------+-------- ... -------+------+-----+-------- ... --+
         | boot | super|      log           |inode | bit |   data       |
         |block |block |  (31 blocks)       |blocks| map |  blocks      |
         |      |      |  header + 30 data  |(12)  | (1) |  (1954)      |
         +------+------+-------- ... -------+------+-----+-------- ... --+
         |             |                    |      |     |              |
         0             2                    33     45    46            1999
```

**주요 상수** (`kernel/param.h`, `kernel/fs.h`, `mkfs/mkfs.c` 참조):

| 상수 | 값 | 의미 |
|------|-----|------|
| `FSSIZE` | 2000 | 전체 파일 시스템 크기 (블록 수) |
| `BSIZE` | 1024 | 블록 크기 (bytes) |
| `LOGBLOCKS` | 30 | 로그 데이터 블록 수 |
| `nlog` | 31 | 로그 영역 = 헤더 1 + 데이터 30 |
| `NINODES` | 200 | 최대 inode 수 |
| `ninodeblocks` | 13 | inode 블록 수 (200 / IPB + 1) |
| `nbitmap` | 1 | 비트맵 블록 수 (2000 / 8192 + 1) |
| `NBUF` | 30 | Buffer cache 크기 |
| `NDIRECT` | 12 | 직접 블록 포인터 수 |
| `NINDIRECT` | 256 | 간접 블록 포인터 수 (1024/4) |
| `MAXFILE` | 268 | 최대 파일 크기 (블록 수: 12+256) |

각 영역의 역할:

| 영역 | 블록 범위 | 역할 |
|------|----------|------|
| Boot block | 0 | 부트 로더 (xv6에서는 사용하지 않음) |
| Super block | 1 | 파일 시스템 메타데이터 (크기, 각 영역 시작 위치) |
| Log | 2 ~ 32 | Write-ahead log (crash recovery용) |
| Inode blocks | 33 ~ 44 | 디스크 inode 구조체 배열 |
| Bitmap | 45 | 데이터 블록 사용 여부 비트맵 |
| Data blocks | 46 ~ 1999 | 실제 파일 데이터 |

> **참고**: 정확한 블록 범위는 `mkfs` 실행 시 출력되는 값을 확인하자.
> `examples/disk_layout.py`를 실행하면 시각적으로 확인할 수 있다.

### 핵심 자료 구조

**superblock** (`kernel/fs.h`):
```c
struct superblock {
  uint magic;        // Must be FSMAGIC (0x10203040)
  uint size;         // 전체 블록 수
  uint nblocks;      // 데이터 블록 수
  uint ninodes;      // inode 수
  uint nlog;         // 로그 블록 수
  uint logstart;     // 로그 영역 시작 블록
  uint inodestart;   // inode 영역 시작 블록
  uint bmapstart;    // 비트맵 영역 시작 블록
};
```

**디스크 inode** (`kernel/fs.h`):
```c
struct dinode {
  short type;              // 파일 유형 (0=free, T_DIR, T_FILE, T_DEVICE)
  short major;             // 장치 번호 (T_DEVICE만 해당)
  short minor;
  short nlink;             // 하드 링크 수
  uint size;               // 파일 크기 (bytes)
  uint addrs[NDIRECT+1];   // 데이터 블록 주소 (12 direct + 1 indirect)
};
```

**버퍼** (`kernel/buf.h`):
```c
struct buf {
  int valid;           // 디스크에서 읽어온 데이터가 있는가?
  int disk;            // 디스크 드라이버가 소유 중인가?
  uint dev;            // 장치 번호
  uint blockno;        // 블록 번호
  struct sleeplock lock;
  uint refcnt;         // 참조 카운트
  struct buf *prev;    // LRU 리스트 (이전)
  struct buf *next;    // LRU 리스트 (다음)
  uchar data[BSIZE];   // 블록 데이터 (1024 bytes)
};
```

---

## Exercise 1: Buffer Cache 분석 (약 12분)

> **파일**: `kernel/bio.c`, `kernel/buf.h`

Buffer cache는 파일 시스템의 **가장 하위 소프트웨어 계층**이다.
두 가지 핵심 역할을 한다:

1. **동기화**: 같은 블록에 대한 동시 접근을 직렬화
2. **캐싱**: 자주 사용되는 블록을 메모리에 유지하여 디스크 I/O 감소

### 1-1. `bget()` 함수 분석

`bget()`은 buffer cache의 핵심 함수로, 요청된 블록을 캐시에서 찾거나 새로 할당한다.

```c
// kernel/bio.c
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // [단계 1] 이미 캐시에 있는지 확인
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);   // sleep-lock 획득
      return b;
    }
  }

  // [단계 2] 캐시에 없으면 LRU 버퍼를 재활용
  // 리스트의 끝(head.prev)부터 탐색 = 가장 오래된 것부터
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;        // 아직 디스크에서 읽지 않음
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}
```

**질문 1-1**: `bget()`에서 캐시 히트(단계 1)와 캐시 미스(단계 2)의 차이점은 무엇인가?

> **힌트**: `valid` 필드의 값에 주목하라. 캐시 히트 시에는 `valid`를 건드리지 않고,
> 캐시 미스 시에는 `valid = 0`으로 설정한다.

### 1-2. `bread()`와 `bwrite()` 함수

```c
// kernel/bio.c - 블록 읽기
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;
  b = bget(dev, blockno);
  if(!b->valid) {              // 캐시 미스였으면
    virtio_disk_rw(b, 0);     // 디스크에서 읽기 (0 = read)
    b->valid = 1;
  }
  return b;                    // locked 상태로 반환
}

// kernel/bio.c - 블록 쓰기
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);       // 디스크에 쓰기 (1 = write)
}
```

### 1-3. `brelse()` - 버퍼 해제와 LRU 갱신

```c
// kernel/bio.c
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);       // sleep-lock 해제

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 아무도 사용하지 않으면 -> 리스트의 앞(MRU)으로 이동
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  release(&bcache.lock);
}
```

**LRU 리스트 구조**:

```
  가장 최근 사용                              가장 오래전 사용
       (MRU)                                      (LRU)
         |                                          |
         v                                          v
  head <-> buf_A <-> buf_B <-> buf_C <-> ... <-> buf_Z <-> head
       next->                                        <-prev
```

- `brelse()` 시 `refcnt`가 0이 되면 → 리스트의 **앞쪽**(head.next)으로 이동 (MRU)
- `bget()`에서 빈 버퍼가 필요하면 → 리스트의 **뒤쪽**(head.prev)에서 탐색 (LRU)

### 1-4. 관찰 질문

**질문 1-2**: Buffer cache에서 왜 **sleep-lock**을 사용하는가? 일반 spin-lock 대신?

> **답**: 디스크 I/O는 수 밀리초가 걸리는 느린 연산이다.
> spin-lock을 사용하면 I/O 대기 중 CPU를 낭비하게 된다.
> Sleep-lock은 대기 중인 프로세스를 sleep 상태로 전환하여 CPU를 다른 프로세스에
> 양보할 수 있게 한다. 또한, sleep-lock은 프로세스가 lock을 잡은 채로
> sleep할 수 있게 허용한다 (spin-lock은 이것이 금지됨).

**질문 1-3**: `bcache.lock` (spin-lock)과 `b->lock` (sleep-lock)은 각각 무엇을 보호하는가?

> **답**:
> - `bcache.lock`: 버퍼 캐시의 **연결 리스트 구조**와 `refcnt`를 보호 (짧은 임계 영역)
> - `b->lock`: 개별 버퍼의 **데이터(`data[]`)**를 보호 (디스크 I/O 동안 유지)

**질문 1-4**: `NBUF`는 30이다. 만약 30개의 버퍼가 모두 사용 중이면 어떻게 되는가?

> **답**: `bget()`에서 `panic("bget: no buffers")`가 호출되어 커널이 중단된다.
> 실제 운영체제에서는 사용 중인 버퍼가 해제될 때까지 대기하도록 구현한다.

---

## Exercise 2: Logging 시스템 분석 (약 12분)

> **파일**: `kernel/log.c`

xv6의 로깅 시스템은 파일 시스템 연산의 **원자성(atomicity)**을 보장한다.
예를 들어, 파일 생성은 inode 할당, 디렉토리 엔트리 추가 등 여러 블록 쓰기를
필요로 하는데, 중간에 전원이 꺼져도 파일 시스템이 **일관된 상태**를 유지해야 한다.

### 2-1. Write-Ahead Logging 개요

```
  +-----------+     +-----------+     +-----------+     +-----------+
  | begin_op  | --> | log_write | --> |  end_op   | --> |  commit   |
  | (트랜잭션 |     | (로그에   |     | (마지막   |     | (실제     |
  |  시작)    |     |  기록 예약)|    |  연산이면 |     |  커밋)    |
  +-----------+     +-----------+     |  커밋)    |     +-----------+
                                      +-----------+
```

**commit()의 4단계**:

```c
// kernel/log.c
static void commit()
{
  if (log.lh.n > 0) {
    write_log();      // [1] 수정된 블록을 캐시 -> 로그 영역에 기록
    write_head();     // [2] 로그 헤더를 디스크에 기록 ← 진짜 커밋 시점!
    install_trans(0); // [3] 로그 영역 -> 원래 위치로 복사 (홈 위치)
    log.lh.n = 0;
    write_head();     // [4] 로그 헤더의 n=0으로 만들어 로그 삭제
  }
}
```

### 2-2. `begin_op()`과 `end_op()` 분석

```c
// kernel/log.c
void begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);           // 커밋 중이면 대기
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGBLOCKS){
      sleep(&log, &log.lock);           // 로그 공간 부족하면 대기
    } else {
      log.outstanding += 1;             // 진행 중인 연산 수 증가
      release(&log.lock);
      break;
    }
  }
}

void end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;                 // 진행 중인 연산 수 감소
  if(log.outstanding == 0){
    do_commit = 1;                      // 마지막 연산이면 커밋
    log.committing = 1;
  } else {
    wakeup(&log);                       // 대기 중인 begin_op 깨우기
  }
  release(&log.lock);

  if(do_commit){
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);                       // 커밋 완료 알림
    release(&log.lock);
  }
}
```

### 2-3. `log_write()` - 실제 쓰기 대신 로그에 기록 예약

```c
// kernel/log.c
void log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGBLOCKS)
    panic("too big a transaction");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)  // log absorption!
      break;                             // 같은 블록이면 중복 추가 안 함
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {    // 새 블록이면
    bpin(b);               // 캐시에서 쫓겨나지 않도록 고정
    log.lh.n++;
  }
  release(&log.lock);
}
```

**질문 2-1**: "log absorption"이란 무엇이며, 왜 중요한가?

> **답**: 같은 블록을 여러 번 수정해도 로그에는 한 번만 기록하는 최적화이다.
> 예를 들어, 같은 비트맵 블록을 여러 번 수정해도 로그 공간을 하나만 차지한다.
> 이것은 제한된 로그 크기(`LOGBLOCKS = 30`) 안에서 더 많은 연산을 처리할 수 있게 한다.

### 2-4. Crash Recovery 시나리오 분석

`commit()`의 4단계 중 어디에서 전원이 꺼지는지에 따라 결과가 달라진다:

```
  commit() {
    write_log();       // [1] 로그에 데이터 기록
    ────────── 전원 꺼짐 시점 A ──────────
    write_head();      // [2] 헤더에 n 기록 ← 커밋 포인트!
    ────────── 전원 꺼짐 시점 B ──────────
    install_trans(0);  // [3] 데이터를 홈 위치로 복사
    ────────── 전원 꺼짐 시점 C ──────────
    log.lh.n = 0;
    write_head();      // [4] 로그 삭제
  }
```

| 전원 꺼짐 시점 | 로그 헤더 상태 | 재부팅 시 동작 | 결과 |
|:---:|:---:|:---|:---|
| A | `n = 0` | 복구할 것 없음 | 트랜잭션 **무시** (데이터 손실) |
| B | `n > 0` | `install_trans(1)` 실행 | 트랜잭션 **재실행** (완전 복구) |
| C | `n > 0` | `install_trans(1)` 실행 | 트랜잭션 **재실행** (멱등, 안전) |

**`recover_from_log()` 코드**:

```c
// kernel/log.c
static void recover_from_log(void)
{
  read_head();           // 디스크에서 로그 헤더 읽기
  install_trans(1);      // n > 0이면 로그 -> 홈 위치로 복사 (재실행)
  log.lh.n = 0;
  write_head();          // 로그 삭제
}
```

**질문 2-2**: `write_head()`(단계 2)가 왜 "진짜 커밋 시점"인가?

> **답**: `write_head()`는 로그 헤더의 `n` 값을 디스크에 기록한다.
> 이 쓰기가 완료된 후에야 "커밋된 트랜잭션이 존재한다"는 사실이
> 디스크에 영구적으로 기록된다. 재부팅 시 `read_head()`로 이 값을 읽어
> `n > 0`이면 복구를 수행한다. 단일 블록 쓰기(`write_head`)의 원자성은
> 디스크 하드웨어가 보장한다.

**질문 2-3**: `install_trans()`는 왜 멱등(idempotent)한가?

> **답**: `install_trans()`는 로그 블록의 내용을 홈 위치에 **덮어쓰기**만 한다.
> 이미 올바른 데이터가 있더라도 같은 데이터를 다시 쓰는 것이므로 결과는 동일하다.
> 따라서 여러 번 실행해도 안전하다.

---

## Exercise 3: 파일 생성 과정 추적 (약 15분)

> **파일**: `kernel/sysfile.c`, `kernel/fs.c`, `kernel/file.c`, `kernel/log.c`, `kernel/bio.c`

사용자 프로그램에서 `open("newfile", O_CREATE|O_RDWR)`을 호출할 때
실행되는 코드 경로를 단계별로 추적해보자.

### 3-1. 전체 호출 체인

```
  사용자: open("newfile", O_CREATE|O_RDWR)
    |
    v
  [sysfile.c] sys_open()
    |-- begin_op()                    // 트랜잭션 시작
    |-- create("newfile", T_FILE, 0, 0)
    |     |-- nameiparent("newfile")  // 부모 디렉토리(/) inode 찾기
    |     |-- ilock(dp)              // 부모 디렉토리 잠금
    |     |-- dirlookup(dp, "newfile") // 이미 존재하는지 확인
    |     |-- ialloc(dev, T_FILE)    // 새 inode 할당 ★
    |     |     |-- bread(dev, IBLOCK(inum))  // inode 블록 읽기
    |     |     |-- log_write(bp)             // 수정된 블록 로깅
    |     |     `-- brelse(bp)
    |     |-- ilock(ip)              // 새 inode 잠금
    |     |-- iupdate(ip)           // nlink=1 디스크에 기록
    |     |     |-- bread(...)
    |     |     |-- log_write(...)
    |     |     `-- brelse(...)
    |     |-- dirlink(dp, "newfile", ip->inum)  // 디렉토리에 엔트리 추가 ★
    |     |     `-- writei(dp, ...)  // 디렉토리 inode에 기록
    |     |           |-- bmap(dp, ...) // 블록 매핑 (필요 시 balloc)
    |     |           |-- bread(...)
    |     |           |-- log_write(...)
    |     |           `-- brelse(...)
    |     `-- iunlockput(dp)         // 부모 디렉토리 해제
    |
    |-- filealloc()                  // 시스템 전역 파일 테이블에서 슬롯 할당
    |-- fdalloc(f)                   // 프로세스 파일 디스크립터 테이블에서 슬롯 할당
    |-- f->type = FD_INODE
    |-- f->ip = ip
    |-- f->readable = 1, f->writable = 1
    |-- iunlock(ip)                  // inode 잠금 해제 (참조는 유지)
    |-- end_op()                     // 트랜잭션 종료 -> commit
    `-- return fd                    // 파일 디스크립터 반환
```

### 3-2. 핵심 함수 코드

**`sys_open()`** 중 `O_CREATE` 경로 (`kernel/sysfile.c`):

```c
uint64 sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;

  argint(1, &omode);
  argstr(0, path, MAXPATH);

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);  // 파일 생성 (locked 상태로 반환)
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    // ... 기존 파일 열기 ...
  }

  f = filealloc();           // struct file 할당
  fd = fdalloc(f);           // fd 번호 할당

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  iunlock(ip);
  end_op();

  return fd;
}
```

**`create()`** (`kernel/sysfile.c`):

```c
static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  dp = nameiparent(path, name);  // 부모 디렉토리 inode
  ilock(dp);

  // 이미 존재하는지 확인
  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;     // 기존 파일 반환
    iunlockput(ip);
    return 0;
  }

  ip = ialloc(dp->dev, type);   // 새 inode 할당
  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);                   // 디스크에 기록

  dirlink(dp, name, ip->inum);  // 디렉토리에 엔트리 추가
  iunlockput(dp);

  return ip;   // locked 상태로 반환
}
```

### 3-3. write() 시스템 콜 추적

파일에 데이터를 쓸 때의 흐름:

```
  사용자: write(fd, "hello", 5)
    |
    v
  [sysfile.c] sys_write()
    |-- argfd(0, 0, &f)              // fd -> struct file* 변환
    `-- filewrite(f, addr, 5)
          |
          v
        [file.c] filewrite()
          |-- begin_op()              // 트랜잭션 시작
          |-- ilock(f->ip)            // inode 잠금
          |-- writei(f->ip, 1, addr, f->off, n)
          |     |
          |     v
          |   [fs.c] writei()
          |     |-- bmap(ip, off/BSIZE)  // 논리 블록 -> 물리 블록
          |     |     `-- balloc()       // 필요하면 새 블록 할당
          |     |-- bread(ip->dev, addr) // 블록 읽기
          |     |-- either_copyin(...)   // 사용자 데이터 -> 버퍼 복사
          |     |-- log_write(bp)        // 로그에 기록 예약
          |     |-- brelse(bp)           // 버퍼 해제
          |     `-- iupdate(ip)          // inode의 size 갱신
          |
          |-- f->off += r               // 파일 오프셋 갱신
          |-- iunlock(f->ip)             // inode 잠금 해제
          `-- end_op()                   // 트랜잭션 종료 -> commit
```

### 3-4. read() 시스템 콜 추적

```
  사용자: read(fd, buf, 5)
    |
    v
  [sysfile.c] sys_read()
    `-- fileread(f, addr, 5)
          |
          v
        [file.c] fileread()
          |-- ilock(f->ip)
          |-- readi(f->ip, 1, addr, f->off, n)
          |     |
          |     v
          |   [fs.c] readi()
          |     |-- bmap(ip, off/BSIZE)     // 논리 블록 -> 물리 블록
          |     |-- bread(ip->dev, addr)    // 블록 읽기 (캐시 확인)
          |     |-- either_copyout(...)     // 버퍼 -> 사용자 공간 복사
          |     `-- brelse(bp)
          |
          |-- f->off += r
          `-- iunlock(f->ip)
```

**질문 3-1**: `read()`는 `begin_op()/end_op()`을 호출하지 않는다. 왜?

> **답**: `read()`는 디스크를 **수정하지 않는다**. 따라서 crash recovery가 필요 없고,
> 로그 트랜잭션이 불필요하다. 반면 `write()`는 데이터 블록, inode, 비트맵 등
> 여러 블록을 수정하므로 트랜잭션이 필요하다.

**질문 3-2**: `filewrite()`에서 왜 한 번에 `((MAXOPBLOCKS-1-1-2)/2)*BSIZE` 바이트만 쓰는가?

> **답**: 하나의 트랜잭션에서 쓸 수 있는 블록 수가 `MAXOPBLOCKS(=10)`으로 제한된다.
> 쓰기 연산에는 데이터 블록 외에도 inode 블록(1), indirect 블록(1),
> 비트맵 블록(2, 정렬 안 될 수 있으므로) 등이 필요하다.
> 따라서 순수 데이터에 사용할 수 있는 블록 수는 `(10-1-1-2)/2 = 3`개이고,
> 한 번에 `3 * 1024 = 3072 bytes`만 쓸 수 있다.
> 큰 파일은 여러 트랜잭션으로 나누어 쓴다.

### 3-5. 실습: fs_trace 프로그램

`examples/fs_trace.c`는 xv6 사용자 프로그램으로, 파일 생성/쓰기/읽기 과정을
단계별로 출력한다. xv6에서 실행하여 각 단계를 확인해보자.

```
$ fs_trace
```

이 프로그램의 소스 코드를 읽고, 각 시스템 콜이 위에서 분석한
어떤 코드 경로를 따라 실행되는지 매핑해보자.

---

## Exercise 4: 큰 파일 지원 분석 (약 11분)

> **파일**: `kernel/fs.h`, `kernel/fs.c`의 `bmap()` 함수

### 4-1. 현재 xv6의 파일 크기 제한

xv6의 inode는 다음과 같은 블록 주소 구조를 가진다:

```
  struct dinode {
    ...
    uint addrs[NDIRECT+1];   // addrs[0..11]: 직접 블록
  };                          // addrs[12]:    간접 블록 포인터

  addrs[0]  -> 데이터 블록 0     ─┐
  addrs[1]  -> 데이터 블록 1      │
  addrs[2]  -> 데이터 블록 2      │ 12개 직접(direct) 블록
  ...                             │ = 12 * 1024 = 12,288 bytes
  addrs[11] -> 데이터 블록 11    ─┘

  addrs[12] -> [간접 블록] ─┐
                  |          │
                  v          │ 256개 간접(indirect) 블록
              +--------+    │ = 256 * 1024 = 262,144 bytes
              | addr 0 | -> 데이터 블록 12
              | addr 1 | -> 데이터 블록 13
              | ...    |
              |addr 255| -> 데이터 블록 267
              +--------+    │
                            ─┘
```

**최대 파일 크기 계산**:

```
  NDIRECT = 12
  NINDIRECT = BSIZE / sizeof(uint) = 1024 / 4 = 256
  MAXFILE = NDIRECT + NINDIRECT = 12 + 256 = 268 블록
  최대 파일 크기 = 268 * 1024 bytes = 274,432 bytes ≈ 268 KB
```

### 4-2. `bmap()` 함수 분석

`bmap()`은 파일 내 논리 블록 번호를 디스크의 물리 블록 번호로 변환한다.

```c
// kernel/fs.c
static uint bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  // 직접 블록 (bn = 0..11)
  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0){
      addr = balloc(ip->dev);    // 할당 안 됐으면 새로 할당
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  // 간접 블록 (bn = 0..255, 원래 12..267)
  if(bn < NINDIRECT){
    if((addr = ip->addrs[NDIRECT]) == 0){
      addr = balloc(ip->dev);    // 간접 블록 자체를 할당
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);   // 간접 블록 읽기
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      addr = balloc(ip->dev);    // 데이터 블록 할당
      a[bn] = addr;
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");   // 268 블록 초과 시 에러
}
```

### 4-3. Double Indirect Block 추가 시 확장 계산

만약 `addrs[]` 배열에 **double indirect** 포인터를 하나 추가한다면:

```
  addrs[0..11]  : 12개 직접 블록
  addrs[12]     : 1개 단일 간접 블록 -> 256개 데이터 블록
  addrs[13]     : 1개 이중 간접 블록 (새로 추가!)
                     |
                     v
                 +--------+
                 | ptr 0  | -> [간접 블록 0] -> 256개 데이터 블록
                 | ptr 1  | -> [간접 블록 1] -> 256개 데이터 블록
                 | ...    |
                 |ptr 255 | -> [간접 블록 255] -> 256개 데이터 블록
                 +--------+
                  256 포인터 * 256 데이터 블록 = 65,536 블록
```

**확장된 최대 파일 크기 계산**:

```
  직접 블록:        12 블록
  단일 간접:       256 블록
  이중 간접: 256 * 256 = 65,536 블록
  ──────────────────────────
  합계:          65,804 블록

  최대 파일 크기 = 65,804 * 1024 bytes
               = 67,383,296 bytes
               ≈ 64.25 MB
```

**질문 4-1**: 현재 `dinode` 구조체에서 `addrs[]`의 크기는 `NDIRECT+1 = 13`이다.
Double indirect를 추가하려면 `addrs[]`를 어떻게 변경해야 하는가?

> **답**: `addrs[NDIRECT+2]`로 변경 (크기 14). `addrs[12]`는 single indirect,
> `addrs[13]`은 double indirect로 사용한다. 또한 `MAXFILE` 매크로도
> `NDIRECT + NINDIRECT + NINDIRECT*NINDIRECT`로 변경해야 한다.

**질문 4-2**: Triple indirect block을 추가하면 최대 파일 크기는 얼마인가?

> **답**:
> ```
>   직접:                12 블록
>   단일 간접:          256 블록
>   이중 간접:   256 * 256 = 65,536 블록
>   삼중 간접: 256^3      = 16,777,216 블록
>   합계:                  16,843,020 블록
>   = 16,843,020 * 1024 bytes ≈ 16 GB
> ```

### 4-4. `bmap()` 확장 의사 코드

Double indirect를 지원하도록 `bmap()`을 확장한 의사 코드:

```c
static uint bmap(struct inode *ip, uint bn)
{
  // ... 기존 direct, single indirect 코드 ...

  bn -= NINDIRECT;

  // Double indirect (bn = 0 .. NINDIRECT*NINDIRECT - 1)
  if(bn < NINDIRECT * NINDIRECT){
    // 1단계: double indirect 블록 자체 로드
    if((addr = ip->addrs[NDIRECT+1]) == 0){
      addr = balloc(ip->dev);
      ip->addrs[NDIRECT+1] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    // 2단계: 중간 간접 블록 로드 (bn / NINDIRECT 번째)
    uint idx1 = bn / NINDIRECT;
    if((addr = a[idx1]) == 0){
      addr = balloc(ip->dev);
      a[idx1] = addr;
      log_write(bp);
    }
    brelse(bp);

    // 3단계: 최종 데이터 블록
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    uint idx2 = bn % NINDIRECT;
    if((addr = a[idx2]) == 0){
      addr = balloc(ip->dev);
      a[idx2] = addr;
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
```

---

## 요약 및 핵심 정리

### 파일 시스템 계층 요약

| 계층 | 소스 파일 | 핵심 함수 | 역할 |
|------|----------|----------|------|
| File descriptor | `file.c`, `sysfile.c` | `fileread`, `filewrite`, `sys_open` | 사용자 인터페이스 |
| Pathname | `fs.c` | `namei`, `namex` | 경로명 해석 |
| Directory | `fs.c` | `dirlookup`, `dirlink` | 이름-inode 매핑 |
| Inode | `fs.c` | `ialloc`, `readi`, `writei`, `bmap` | 파일 메타데이터와 데이터 블록 |
| Log | `log.c` | `begin_op`, `end_op`, `log_write` | Crash recovery |
| Buffer cache | `bio.c` | `bread`, `bwrite`, `brelse` | 블록 캐싱, 동기화 |

### 핵심 개념 정리

1. **Buffer cache**: LRU 교체 정책, sleep-lock으로 동시성 제어
   - `bcache.lock` (spin-lock): 리스트 구조 보호
   - `b->lock` (sleep-lock): 버퍼 데이터 보호 (I/O 중 sleep 가능)

2. **Logging**: Write-ahead log로 원자성 보장
   - `write_head()`가 진짜 커밋 시점
   - 재부팅 시 `recover_from_log()`로 자동 복구
   - Log absorption으로 공간 절약

3. **Inode**: 12 direct + 1 indirect = 최대 268 블록 (268 KB)
   - `bmap()`: 논리 블록 -> 물리 블록 변환 (필요 시 할당)
   - `ialloc()`: 빈 inode 탐색 및 할당

4. **파일 생성 흐름**: `sys_open` -> `create` -> `ialloc` + `dirlink`
   - 모든 수정은 `begin_op()`/`end_op()` 트랜잭션 안에서 수행
   - `log_write()`가 `bwrite()`를 대체하여 로깅

5. **파일 크기 확장**: double indirect 추가 시 약 64 MB까지 지원 가능
