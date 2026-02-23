#!/usr/bin/env python3
"""
disk_layout.py - xv6 디스크 레이아웃 시각화 스크립트

xv6 파일 시스템의 디스크 블록 배치를 시각적으로 보여줍니다.
kernel/param.h, kernel/fs.h, mkfs/mkfs.c의 상수를 기반으로 계산합니다.

실행:
    python3 disk_layout.py

교육 목적: xv6 파일 시스템 수업에서 디스크 구조를 이해하기 위한 도구
"""

# ============================================================
# xv6 파일 시스템 상수 (kernel/param.h, kernel/fs.h 기준)
# ============================================================

FSSIZE = 2000           # 전체 파일 시스템 크기 (블록 수)
BSIZE = 1024            # 블록 크기 (bytes)
MAXOPBLOCKS = 10        # 하나의 FS 연산이 쓸 수 있는 최대 블록 수
LOGBLOCKS = MAXOPBLOCKS * 3   # 로그 데이터 블록 수 (30)
NBUF = MAXOPBLOCKS * 3  # Buffer cache 크기 (30)

NINODES = 200           # 최대 inode 수 (mkfs/mkfs.c에서 정의)
NDIRECT = 12            # 직접 블록 포인터 수
BSIZE_UINT = BSIZE // 4 # 한 블록에 들어가는 uint 수 (= NINDIRECT = 256)
NINDIRECT = BSIZE_UINT  # 간접 블록 포인터 수

DINODE_SIZE = 64        # sizeof(struct dinode) = 2+2+2+2+4+52 = 64 bytes
IPB = BSIZE // DINODE_SIZE   # 블록당 inode 수 (1024/64 = 16)
BPB = BSIZE * 8         # 블록당 비트맵 비트 수 (8192)

DIRENT_SIZE = 16        # sizeof(struct dirent) = 2 + 14 = 16 bytes


def calculate_layout():
    """xv6 디스크 레이아웃 계산 (mkfs/mkfs.c의 로직과 동일)"""

    nlog = LOGBLOCKS + 1          # 로그 헤더(1) + 데이터(30) = 31
    ninodeblocks = NINODES // IPB + 1   # 200/16 + 1 = 13
    nbitmap = FSSIZE // BPB + 1         # 2000/8192 + 1 = 1

    # 메타 블록: boot(1) + super(1) + log + inode + bitmap
    nmeta = 1 + 1 + nlog + ninodeblocks + nbitmap
    nblocks = FSSIZE - nmeta            # 데이터 블록 수

    layout = {
        "boot":     {"start": 0,            "count": 1},
        "super":    {"start": 1,            "count": 1},
        "log":      {"start": 2,            "count": nlog},
        "inode":    {"start": 2 + nlog,     "count": ninodeblocks},
        "bitmap":   {"start": 2 + nlog + ninodeblocks, "count": nbitmap},
        "data":     {"start": nmeta,        "count": nblocks},
    }

    return layout, nmeta, nblocks


def print_header():
    """제목 출력"""
    print("=" * 70)
    print("  xv6 파일 시스템 디스크 레이아웃 시각화")
    print("=" * 70)
    print()


def print_constants():
    """주요 상수 출력"""
    print("[주요 상수]")
    print(f"  FSSIZE       = {FSSIZE:>6} 블록    (전체 파일 시스템 크기)")
    print(f"  BSIZE        = {BSIZE:>6} bytes   (블록 크기)")
    print(f"  LOGBLOCKS    = {LOGBLOCKS:>6}         (로그 데이터 블록 수)")
    print(f"  NBUF         = {NBUF:>6}         (Buffer cache 크기)")
    print(f"  NINODES      = {NINODES:>6}         (최대 inode 수)")
    print(f"  NDIRECT      = {NDIRECT:>6}         (직접 블록 포인터 수)")
    print(f"  NINDIRECT    = {NINDIRECT:>6}         (간접 블록 포인터 수)")
    print(f"  IPB          = {IPB:>6}         (블록당 inode 수)")
    print(f"  BPB          = {BPB:>6}         (블록당 비트맵 비트 수)")
    print(f"  MAXOPBLOCKS  = {MAXOPBLOCKS:>6}         (FS 연산당 최대 쓰기 블록)")
    print()


def print_layout_table(layout, nmeta, nblocks):
    """레이아웃 테이블 출력"""
    total_size_kb = FSSIZE * BSIZE / 1024

    print("[디스크 레이아웃 상세]")
    print(f"  전체 크기: {FSSIZE} 블록 = {FSSIZE * BSIZE:,} bytes"
          f" = {total_size_kb:.0f} KB")
    print(f"  메타 블록: {nmeta} 블록")
    print(f"  데이터 블록: {nblocks} 블록")
    print()

    print(f"  {'영역':<12} {'시작 블록':>10} {'블록 수':>10} "
          f"{'끝 블록':>10} {'크기 (bytes)':>14} {'설명'}")
    print("  " + "-" * 78)

    descriptions = {
        "boot":   "부트 로더 (xv6에서 미사용)",
        "super":  "superblock (FS 메타데이터)",
        "log":    f"WAL 로그 (헤더 1 + 데이터 {LOGBLOCKS})",
        "inode":  f"디스크 inode ({NINODES}개, {IPB}개/블록)",
        "bitmap": f"블록 사용 비트맵 ({BPB}비트/블록)",
        "data":   "실제 파일 데이터",
    }

    for name in ["boot", "super", "log", "inode", "bitmap", "data"]:
        info = layout[name]
        start = info["start"]
        count = info["count"]
        end = start + count - 1
        size = count * BSIZE
        desc = descriptions[name]
        print(f"  {name:<12} {start:>10} {count:>10} "
              f"{end:>10} {size:>14,} {desc}")

    print()


def print_visual_layout(layout):
    """시각적 디스크 맵 출력"""
    print("[디스크 블록 맵]")
    print()

    # 컴팩트 뷰
    regions = [
        ("boot",   layout["boot"],   "B"),
        ("super",  layout["super"],  "S"),
        ("log",    layout["log"],    "L"),
        ("inode",  layout["inode"],  "I"),
        ("bitmap", layout["bitmap"], "M"),
        ("data",   layout["data"],   "D"),
    ]

    # 비례 시각화 (폭 60자)
    bar_width = 60
    total = FSSIZE

    print("  블록 0" + " " * (bar_width - 12) + f"블록 {FSSIZE - 1}")
    print("  |" + " " * (bar_width - 2) + "|")
    bar = ""
    labels = []
    for name, info, char in regions:
        count = info["count"]
        # 최소 1자 보장
        width = max(1, round(count / total * bar_width))
        bar += char * width
        labels.append((name, info["start"], char))

    # 정확히 bar_width에 맞추기
    if len(bar) > bar_width:
        bar = bar[:bar_width]
    elif len(bar) < bar_width:
        bar = bar + "D" * (bar_width - len(bar))

    print(f"  [{bar}]")
    print()

    # 범례
    print("  범례:")
    for name, info, char in regions:
        start = info["start"]
        count = info["count"]
        end = start + count - 1
        pct = count / total * 100
        print(f"    {char} = {name:<8} (블록 {start:>4} ~ {end:>4},"
              f" {count:>4}블록, {pct:5.1f}%)")

    print()

    # 상세 ASCII art
    print("  상세 다이어그램:")
    print()
    print("  +------+------+---------------------------+")
    log_info = layout["log"]
    print(f"  | boot | super|         log               |")
    print(f"  |  [0] |  [1] |  [{log_info['start']}]"
          f" ~ [{log_info['start'] + log_info['count'] - 1}]"
          f"              |")
    print("  +------+------+---------------------------+")

    inode_info = layout["inode"]
    bitmap_info = layout["bitmap"]
    print(f"  |       inode blocks         | bitmap     |")
    print(f"  | [{inode_info['start']}]"
          f" ~ [{inode_info['start'] + inode_info['count'] - 1}]"
          f"               |   [{bitmap_info['start']}]      |")
    print("  +----------------------------+-----------+")

    data_info = layout["data"]
    print(f"  |                data blocks                |")
    print(f"  |  [{data_info['start']}]"
          f" ~ [{data_info['start'] + data_info['count'] - 1}]"
          f"                           |")
    print("  +-------------------------------------------+")
    print()


def print_inode_structure():
    """inode 구조 시각화"""
    print("[inode 블록 주소 구조]")
    print()
    print("  struct dinode {")
    print("    short type;          // 파일 유형 (2 bytes)")
    print("    short major;         // 장치 번호  (2 bytes)")
    print("    short minor;         //            (2 bytes)")
    print("    short nlink;         // 링크 수    (2 bytes)")
    print("    uint  size;          // 파일 크기  (4 bytes)")
    print(f"    uint  addrs[{NDIRECT}+1];  // 블록 주소  ({(NDIRECT+1)*4} bytes)")
    print(f"  }};  // 총 {DINODE_SIZE} bytes")
    print()
    print(f"  블록당 inode 수 = BSIZE / sizeof(dinode)"
          f" = {BSIZE} / {DINODE_SIZE} = {IPB}")
    print()

    print("  addrs[] 구조:")
    print()
    print("  addrs[0]  -----> [데이터 블록]")
    print("  addrs[1]  -----> [데이터 블록]")
    print("    ...              ...")
    print(f"  addrs[{NDIRECT - 1}] -----> [데이터 블록]")
    print(f"                      총 {NDIRECT}개 직접 블록"
          f" = {NDIRECT * BSIZE:,} bytes")
    print()
    print(f"  addrs[{NDIRECT}] -----> [간접 블록 (1024 bytes)]")
    print("                     |")
    print("                     +---> addr[0]   -> [데이터 블록]")
    print("                     +---> addr[1]   -> [데이터 블록]")
    print("                     +---> ...          ...")
    print(f"                     +---> addr[{NINDIRECT - 1}] -> [데이터 블록]")
    print(f"                      총 {NINDIRECT}개 간접 블록"
          f" = {NINDIRECT * BSIZE:,} bytes")
    print()


def print_file_size_limits():
    """파일 크기 제한 계산"""
    print("[파일 크기 제한 분석]")
    print()

    # 현재 제한
    maxfile = NDIRECT + NINDIRECT
    max_bytes = maxfile * BSIZE
    print("  현재 xv6 (direct + single indirect):")
    print(f"    직접 블록:   {NDIRECT:>10} 블록")
    print(f"    간접 블록:   {NINDIRECT:>10} 블록")
    print(f"    합계 (MAXFILE): {maxfile:>7} 블록")
    print(f"    최대 파일 크기: {max_bytes:>7,} bytes"
          f" = {max_bytes / 1024:.0f} KB")
    print()

    # Double indirect 추가 시
    double_indirect = NINDIRECT * NINDIRECT
    maxfile_di = NDIRECT + NINDIRECT + double_indirect
    max_bytes_di = maxfile_di * BSIZE
    print("  Double indirect 추가 시:")
    print(f"    직접 블록:            {NDIRECT:>10} 블록")
    print(f"    단일 간접 블록:       {NINDIRECT:>10} 블록")
    print(f"    이중 간접 블록: {NINDIRECT}x{NINDIRECT} = "
          f"{double_indirect:>6} 블록")
    print(f"    합계:              {maxfile_di:>10} 블록")
    print(f"    최대 파일 크기:    {max_bytes_di:>10,} bytes"
          f" = {max_bytes_di / 1024 / 1024:.2f} MB")
    print()

    # Triple indirect 추가 시
    triple_indirect = NINDIRECT * NINDIRECT * NINDIRECT
    maxfile_ti = NDIRECT + NINDIRECT + double_indirect + triple_indirect
    max_bytes_ti = maxfile_ti * BSIZE
    print("  Triple indirect 추가 시:")
    print(f"    직접 블록:               {NDIRECT:>12} 블록")
    print(f"    단일 간접 블록:          {NINDIRECT:>12} 블록")
    print(f"    이중 간접 블록:          {double_indirect:>12} 블록")
    print(f"    삼중 간접 블록: {NINDIRECT}^3 = {triple_indirect:>8} 블록")
    print(f"    합계:                 {maxfile_ti:>12} 블록")
    print(f"    최대 파일 크기:       {max_bytes_ti:>12,} bytes"
          f" = {max_bytes_ti / 1024 / 1024 / 1024:.2f} GB")
    print()


def print_log_structure():
    """로그 구조 시각화"""
    print("[로그 영역 구조]")
    print()
    print(f"  MAXOPBLOCKS = {MAXOPBLOCKS}")
    print(f"  LOGBLOCKS   = MAXOPBLOCKS * 3 = {LOGBLOCKS}")
    print(f"  로그 영역   = 헤더(1) + 데이터({LOGBLOCKS}) = {LOGBLOCKS + 1} 블록")
    print()
    print("  +----------+----------+----------+-----+----------+")
    print("  | log      | log      | log      | ... | log      |")
    print("  | header   | block 0  | block 1  |     | block 29 |")
    print("  | (블록 2) | (블록 3) | (블록 4) |     | (블록 32)|")
    print("  +----------+----------+----------+-----+----------+")
    print()
    print("  log header 구조:")
    print("  +------+----------+----------+-----+----------+")
    print("  |  n   | block[0] | block[1] | ... |block[29] |")
    print("  | (커밋 | (원래    | (원래    |     | (원래    |")
    print("  | 블록수)| 위치 0) | 위치 1)  |     | 위치 29) |")
    print("  +------+----------+----------+-----+----------+")
    print()
    print("  commit() 과정:")
    print("    1. write_log()  : cache -> log 영역으로 복사")
    print("    2. write_head() : header.n 기록 (커밋 포인트!)")
    print("    3. install_trans(): log -> 원래 위치로 복사")
    print("    4. write_head() : header.n = 0 (로그 삭제)")
    print()

    max_write = ((MAXOPBLOCKS - 1 - 1 - 2) // 2) * BSIZE
    print(f"  filewrite() 한 번에 쓸 수 있는 최대 크기:")
    print(f"    (MAXOPBLOCKS-1-1-2)/2 * BSIZE")
    print(f"    = ({MAXOPBLOCKS}-1-1-2)/2 * {BSIZE}")
    print(f"    = {(MAXOPBLOCKS - 1 - 1 - 2) // 2} * {BSIZE}")
    print(f"    = {max_write} bytes")
    print()


def print_directory_structure():
    """디렉토리 구조 시각화"""
    print("[디렉토리 엔트리 구조]")
    print()
    print(f"  struct dirent {{")
    print(f"    ushort inum;       // inode 번호 (2 bytes)")
    print(f"    char   name[14];   // 파일 이름  (14 bytes)")
    print(f"  }};  // 총 {DIRENT_SIZE} bytes")
    print()
    print(f"  블록당 디렉토리 엔트리 수 = {BSIZE} / {DIRENT_SIZE}"
          f" = {BSIZE // DIRENT_SIZE}")
    print()

    print("  루트 디렉토리 (\"/\") 예시:")
    print("  +-------+----------------+")
    print("  | inum  | name           |")
    print("  +-------+----------------+")
    print("  |   1   | .              |")
    print("  |   1   | ..             |")
    print("  |   2   | console        |")
    print("  |   3   | init           |")
    print("  |   4   | sh             |")
    print("  |  ...  | ...            |")
    print("  +-------+----------------+")
    print()


def print_buffer_cache():
    """Buffer cache 구조 시각화"""
    print("[Buffer Cache 구조]")
    print()
    print(f"  NBUF = {NBUF} (캐시할 수 있는 최대 블록 수)")
    print()
    print("  이중 연결 리스트 (LRU 순서):")
    print()
    print("   MRU (최근 사용)                      LRU (오래된)")
    print("    |                                      |")
    print("    v                                      v")
    print("  head <-> buf[A] <-> buf[B] <-> ... <-> buf[Z] <-> head")
    print("       next->                               <-prev")
    print()
    print("  brelse() : refcnt==0이면 head.next로 이동 (MRU)")
    print("  bget()   : 캐시 미스 시 head.prev부터 빈 버퍼 탐색 (LRU)")
    print()
    print("  잠금 체계:")
    print("    bcache.lock (spin-lock)  : 리스트 구조, refcnt 보호")
    print("    buf.lock    (sleep-lock) : 버퍼 데이터 보호 (I/O 중 sleep 가능)")
    print()


def main():
    """메인 함수"""
    layout, nmeta, nblocks = calculate_layout()

    print_header()
    print_constants()

    print("-" * 70)
    print()
    print_layout_table(layout, nmeta, nblocks)

    print("-" * 70)
    print()
    print_visual_layout(layout)

    print("-" * 70)
    print()
    print_inode_structure()

    print("-" * 70)
    print()
    print_file_size_limits()

    print("-" * 70)
    print()
    print_log_structure()

    print("-" * 70)
    print()
    print_directory_structure()

    print("-" * 70)
    print()
    print_buffer_cache()

    print("=" * 70)
    print("  시각화 완료")
    print("=" * 70)


if __name__ == "__main__":
    main()
