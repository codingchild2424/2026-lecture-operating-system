/*
 * fs_trace.c - xv6 파일 시스템 연산 추적 프로그램
 *
 * 파일 생성, 쓰기, 닫기, 다시 열기, 읽기, 검증까지
 * 각 단계를 명확하게 출력하여 파일 시스템 동작을 이해합니다.
 *
 * 이 프로그램은 xv6 사용자 프로그램입니다.
 *
 * 빌드 방법:
 *   1. 이 파일을 xv6-riscv/user/fs_trace.c 에 복사
 *   2. Makefile의 UPROGS에 $U/_fs_trace 추가
 *   3. make qemu 로 빌드 및 실행
 *
 * xv6 쉘에서 실행:
 *   $ fs_trace
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* 구분선 출력 */
void
print_separator(void)
{
  printf("--------------------------------------------------\n");
}

/* 파일 상태 정보 출력 */
void
print_stat(char *path)
{
  struct stat st;
  int fd;

  fd = open(path, O_RDONLY);
  if(fd < 0){
    printf("  [ERROR] open(\"%s\") 실패\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf("  [ERROR] fstat 실패\n");
    close(fd);
    return;
  }

  printf("  파일 정보:\n");
  printf("    - type  : %d ", st.type);
  if(st.type == 1) printf("(T_DIR)\n");
  else if(st.type == 2) printf("(T_FILE)\n");
  else if(st.type == 3) printf("(T_DEVICE)\n");
  else printf("(unknown)\n");
  printf("    - ino   : %d\n", st.ino);
  printf("    - nlink : %d\n", st.nlink);
  printf("    - size  : %d bytes\n", st.size);

  close(fd);
}

int
main(int argc, char *argv[])
{
  int fd;
  int n;
  char buf[128];
  char *testfile = "testfile.txt";
  char *testdir  = "testdir";
  char *msg1 = "Hello, xv6 file system!\n";
  char *msg2 = "Second write to file.\n";

  (void)argc;
  (void)argv;

  printf("==================================================\n");
  printf("  xv6 파일 시스템 연산 추적 프로그램\n");
  printf("==================================================\n\n");

  /* --------------------------------------------------------
   * 단계 1: 파일 생성
   * sys_open() -> create() -> ialloc() + dirlink()
   * -------------------------------------------------------- */
  printf("[단계 1] 파일 생성: open(\"%s\", O_CREATE|O_RDWR)\n", testfile);
  printf("  호출 경로:\n");
  printf("    sys_open() -> create() -> nameiparent()\n");
  printf("                            -> ialloc()  : 빈 inode 할당\n");
  printf("                            -> dirlink() : 디렉토리에 엔트리 추가\n");
  print_separator();

  fd = open(testfile, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("  [ERROR] 파일 생성 실패!\n");
    exit(1);
  }

  printf("  성공! fd = %d\n", fd);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 2: 파일에 데이터 쓰기 (첫 번째)
   * sys_write() -> filewrite() -> writei() -> bmap() -> log_write()
   * -------------------------------------------------------- */
  printf("[단계 2] 첫 번째 쓰기: write(fd, \"%s\", %d)\n",
         "Hello, xv6 file system!\\n", strlen(msg1));
  printf("  호출 경로:\n");
  printf("    sys_write() -> filewrite()\n");
  printf("      -> begin_op()          : 트랜잭션 시작\n");
  printf("      -> ilock()             : inode 잠금\n");
  printf("      -> writei()\n");
  printf("         -> bmap()           : 논리 블록 -> 물리 블록 매핑\n");
  printf("            -> balloc()      : 새 데이터 블록 할당 (처음 쓰기)\n");
  printf("         -> bread()          : 블록 읽기\n");
  printf("         -> either_copyin()  : 사용자 데이터 -> 버퍼 복사\n");
  printf("         -> log_write()      : 로그에 기록 예약\n");
  printf("         -> iupdate()        : inode 크기 갱신\n");
  printf("      -> iunlock()           : inode 잠금 해제\n");
  printf("      -> end_op()            : 트랜잭션 종료 -> commit\n");
  print_separator();

  n = write(fd, msg1, strlen(msg1));
  if(n != strlen(msg1)){
    printf("  [ERROR] 쓰기 실패! (wrote %d bytes)\n", n);
    close(fd);
    exit(1);
  }

  printf("  성공! %d bytes 기록\n", n);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 3: 파일에 데이터 쓰기 (두 번째)
   * 같은 블록 내 오프셋 이동, bmap()이 기존 블록 반환
   * -------------------------------------------------------- */
  printf("[단계 3] 두 번째 쓰기: write(fd, \"%s\", %d)\n",
         "Second write to file.\\n", strlen(msg2));
  printf("  (이번에는 bmap()이 이미 할당된 블록을 반환합니다)\n");
  print_separator();

  n = write(fd, msg2, strlen(msg2));
  if(n != strlen(msg2)){
    printf("  [ERROR] 쓰기 실패! (wrote %d bytes)\n", n);
    close(fd);
    exit(1);
  }

  printf("  성공! %d bytes 기록\n", n);
  print_stat(testfile);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 4: 파일 닫기
   * sys_close() -> fileclose() -> begin_op() -> iput() -> end_op()
   * -------------------------------------------------------- */
  printf("[단계 4] 파일 닫기: close(fd=%d)\n", fd);
  printf("  호출 경로:\n");
  printf("    sys_close() -> fileclose()\n");
  printf("      -> f->ref 감소 (0이 되면):\n");
  printf("         -> begin_op()\n");
  printf("         -> iput(ip)    : inode 참조 감소\n");
  printf("         -> end_op()\n");
  print_separator();

  close(fd);
  printf("  성공! fd %d 닫힘\n\n", fd);

  /* --------------------------------------------------------
   * 단계 5: 파일 다시 열기 (O_CREATE 없이)
   * sys_open() -> namei() -> ilock() (create 아닌 경로)
   * -------------------------------------------------------- */
  printf("[단계 5] 파일 다시 열기: open(\"%s\", O_RDONLY)\n", testfile);
  printf("  호출 경로 (O_CREATE 없음):\n");
  printf("    sys_open() -> namei(\"%s\")\n", testfile);
  printf("      -> namex() -> dirlookup() : 디렉토리에서 이름 검색\n");
  printf("    -> ilock()                   : inode 잠금 + 디스크에서 읽기\n");
  printf("    -> filealloc() + fdalloc()   : 파일 테이블/fd 할당\n");
  print_separator();

  fd = open(testfile, O_RDONLY);
  if(fd < 0){
    printf("  [ERROR] 파일 열기 실패!\n");
    exit(1);
  }

  printf("  성공! fd = %d\n\n", fd);

  /* --------------------------------------------------------
   * 단계 6: 파일 읽기
   * sys_read() -> fileread() -> readi() -> bread()
   * -------------------------------------------------------- */
  printf("[단계 6] 파일 읽기: read(fd, buf, sizeof(buf))\n");
  printf("  호출 경로:\n");
  printf("    sys_read() -> fileread()\n");
  printf("      -> ilock()          : inode 잠금\n");
  printf("      -> readi()\n");
  printf("         -> bmap()        : 논리 블록 -> 물리 블록\n");
  printf("         -> bread()       : 블록 읽기 (buffer cache 확인)\n");
  printf("         -> either_copyout() : 버퍼 -> 사용자 공간 복사\n");
  printf("      -> f->off 갱신\n");
  printf("      -> iunlock()\n");
  printf("  (주의: read는 트랜잭션 불필요 - 디스크 수정 없음)\n");
  print_separator();

  memset(buf, 0, sizeof(buf));
  n = read(fd, buf, sizeof(buf) - 1);
  if(n < 0){
    printf("  [ERROR] 읽기 실패!\n");
    close(fd);
    exit(1);
  }

  buf[n] = '\0';
  printf("  성공! %d bytes 읽음\n", n);
  printf("  내용: \"%s\"\n\n", buf);

  /* --------------------------------------------------------
   * 단계 7: 데이터 검증
   * -------------------------------------------------------- */
  printf("[단계 7] 데이터 검증\n");
  print_separator();

  /* 전체 예상 데이터 구성 */
  char expected[128];
  char *p = expected;
  /* msg1 복사 */
  char *s = msg1;
  while(*s)
    *p++ = *s++;
  /* msg2 복사 */
  s = msg2;
  while(*s)
    *p++ = *s++;
  *p = '\0';

  int expected_len = strlen(expected);

  if(n == expected_len){
    /* 바이트 단위 비교 */
    int match = 1;
    int i;
    for(i = 0; i < n; i++){
      if(buf[i] != expected[i]){
        match = 0;
        break;
      }
    }
    if(match){
      printf("  검증 성공! 읽은 데이터가 쓴 데이터와 일치합니다.\n");
      printf("  (write -> log -> disk -> buffer cache -> read 경로 확인)\n");
    } else {
      printf("  검증 실패! 데이터 불일치 (offset %d)\n", i);
    }
  } else {
    printf("  검증 실패! 길이 불일치 (expected %d, got %d)\n",
           expected_len, n);
  }

  close(fd);
  printf("\n");

  /* --------------------------------------------------------
   * 단계 8: 디렉토리 생성
   * sys_mkdir() -> create(path, T_DIR, 0, 0)
   *   -> ialloc + dirlink(".", ip) + dirlink("..", dp)
   * -------------------------------------------------------- */
  printf("[단계 8] 디렉토리 생성: mkdir(\"%s\")\n", testdir);
  printf("  호출 경로:\n");
  printf("    sys_mkdir() -> create(\"%s\", T_DIR)\n", testdir);
  printf("      -> ialloc()         : 새 inode 할당 (type=T_DIR)\n");
  printf("      -> dirlink(\".\")    : 자기 자신 엔트리\n");
  printf("      -> dirlink(\"..\")   : 부모 디렉토리 엔트리\n");
  printf("      -> dirlink(dp, \"%s\") : 부모에 새 디렉토리 등록\n", testdir);
  printf("      -> dp->nlink++      : 부모의 링크 수 증가 (\"..\" 때문)\n");
  print_separator();

  if(mkdir(testdir) < 0){
    printf("  [ERROR] 디렉토리 생성 실패!\n");
  } else {
    printf("  성공!\n");
    print_stat(testdir);
  }
  printf("\n");

  /* --------------------------------------------------------
   * 단계 9: 디렉토리 안에 파일 생성
   * namei가 경로를 "/" 기준으로 분리하여 순회
   * -------------------------------------------------------- */
  char *nested = "testdir/inner.txt";
  printf("[단계 9] 중첩 파일 생성: open(\"%s\", O_CREATE|O_RDWR)\n", nested);
  printf("  경로 해석 과정 (namex):\n");
  printf("    \"%s\" -> skipelem -> \"testdir\" -> dirlookup\n", nested);
  printf("                     -> skipelem -> \"inner.txt\" -> create\n");
  print_separator();

  fd = open(nested, O_CREATE | O_RDWR);
  if(fd < 0){
    printf("  [ERROR] 중첩 파일 생성 실패!\n");
  } else {
    char *inner_msg = "nested file content\n";
    write(fd, inner_msg, strlen(inner_msg));
    printf("  성공! fd = %d\n", fd);
    print_stat(nested);
    close(fd);
  }
  printf("\n");

  /* --------------------------------------------------------
   * 단계 10: 정리 (unlink)
   * sys_unlink() -> nameiparent() -> dirlookup() -> ip->nlink--
   * -------------------------------------------------------- */
  printf("[단계 10] 파일 삭제: unlink(\"%s\")\n", nested);
  printf("  호출 경로:\n");
  printf("    sys_unlink()\n");
  printf("      -> nameiparent()  : 부모 디렉토리 찾기\n");
  printf("      -> dirlookup()    : 대상 파일 찾기\n");
  printf("      -> writei()       : 디렉토리 엔트리 0으로 초기화\n");
  printf("      -> ip->nlink--    : 링크 수 감소\n");
  printf("      -> iupdate()      : 디스크에 반영\n");
  printf("  (nlink == 0 && ref == 0 이면 iput()에서 itrunc로 블록 해제)\n");
  print_separator();

  if(unlink(nested) < 0){
    printf("  [ERROR] 삭제 실패!\n");
  } else {
    printf("  성공! \"%s\" 삭제됨\n", nested);
  }
  printf("\n");

  /* 남은 테스트 파일/디렉토리 정리 */
  unlink(testdir);
  unlink(testfile);

  /* --------------------------------------------------------
   * 완료
   * -------------------------------------------------------- */
  printf("==================================================\n");
  printf("  모든 파일 시스템 연산 추적 완료!\n");
  printf("==================================================\n");
  printf("\n");
  printf("정리:\n");
  printf("  - 파일 생성: sys_open -> create -> ialloc + dirlink\n");
  printf("  - 파일 쓰기: sys_write -> filewrite -> writei -> log_write\n");
  printf("  - 파일 읽기: sys_read -> fileread -> readi -> bread\n");
  printf("  - 디렉토리 : sys_mkdir -> create(T_DIR) + \".\" + \"..\"\n");
  printf("  - 파일 삭제: sys_unlink -> nlink-- -> (itrunc if nlink==0)\n");
  printf("  - 모든 쓰기는 begin_op/end_op 트랜잭션 안에서 수행됨\n");

  exit(0);
}
