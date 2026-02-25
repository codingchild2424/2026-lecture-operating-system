// wakeup_demo.c - xv6 user program
// pipe의 blocking 동작을 통해 sleep/wakeup 메커니즘을 관찰하는 프로그램
//
// 이 프로그램은 다음을 보여준다:
//   1. reader가 빈 pipe에서 block되는 모습 (sleep)
//   2. writer가 데이터를 쓰면 reader가 깨어나는 모습 (wakeup)
//   3. writer가 가득 찬 pipe에서 block되는 모습 (sleep)
//   4. pipe close 시 EOF 처리
//
// 빌드 방법:
//   1. 이 파일을 user/ 디렉토리에 복사
//   2. Makefile의 UPROGS에 $U/_wakeup_demo\ 추가
//   3. make clean && make qemu

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// ============================================================
// Demo 1: Reader가 Writer를 기다리는 모습
// ============================================================
// Reader(자식)가 먼저 read()를 호출하면 pipe가 비어있으므로
// 커널의 piperead()에서 sleep(&pi->nread, &pi->lock)이 호출된다.
// Writer(부모)가 pause() 후 데이터를 쓰면
// pipewrite() 안에서 wakeup(&pi->nread)이 호출되어
// Reader가 깨어난다.
// ============================================================
static void
demo_reader_waits(void)
{
  int fds[2];
  int pid;

  printf("=== Demo 1: Reader blocks until Writer sends data ===\n");

  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    // 자식 (Reader)
    close(fds[1]);

    printf("[Reader] Calling read()... (pipe is empty, will block)\n");

    // 이 시점에서 pipe 버퍼는 비어있다.
    // 커널 내부 동작:
    //   piperead() 진입
    //   -> acquire(&pi->lock)
    //   -> while(pi->nread == pi->nwrite && pi->writeopen)
    //        -> 조건 true (버퍼 비어있음, writer 열려있음)
    //        -> sleep(&pi->nread, &pi->lock)
    //             -> acquire(&p->lock)
    //             -> release(&pi->lock)  // writer가 진행할 수 있게 됨
    //             -> p->state = SLEEPING
    //             -> sched()  // CPU 양보, 다른 프로세스 실행
    //
    // Reader는 여기서 멈추고 Writer가 깨워줄 때까지 대기한다.

    char buf[32];
    int n = read(fds[0], buf, sizeof(buf) - 1);

    // Writer가 wakeup(&pi->nread)를 호출하면:
    //   wakeup()이 프로세스 테이블을 순회
    //   -> Reader의 p->chan == &pi->nread 확인
    //   -> p->state = RUNNABLE로 변경
    //   스케줄러가 Reader를 다시 실행
    //   -> sleep()에서 sched() 다음으로 복귀
    //   -> piperead()의 while 루프 재확인
    //   -> 데이터가 있으므로 루프 탈출
    //   -> for 루프에서 데이터 복사

    if(n > 0){
      buf[n] = '\0';
      printf("[Reader] Woke up! Received %d bytes: \"%s\"\n", n, buf);
    }

    close(fds[0]);
    exit(0);
  } else {
    // 부모 (Writer)
    close(fds[0]);

    // 일부러 지연을 주어 Reader가 먼저 block되게 한다.
    // pause(n)은 n tick 동안 프로세스를 일시 정지시킨다.
    printf("[Writer] Waiting 10 ticks before sending data...\n");
    pause(10);

    char *msg = "wakeup!";
    printf("[Writer] Writing \"%s\" to pipe...\n", msg);

    // write() 호출 시 커널 내부 동작:
    //   pipewrite() 진입
    //   -> 데이터를 pi->data 버퍼에 복사
    //   -> wakeup(&pi->nread)  // Reader를 깨움!
    //   -> release(&pi->lock)
    write(fds[1], msg, strlen(msg));

    printf("[Writer] Data sent. Reader should wake up now.\n");

    close(fds[1]);
    wait(0);
    printf("=== Demo 1 complete ===\n\n");
  }
}

// ============================================================
// Demo 2: 큰 데이터 전송 시 Writer blocking
// ============================================================
// pipe 버퍼 크기는 512 bytes (PIPESIZE)이다.
// Writer가 512 bytes 이상을 쓰려 하면 버퍼가 가득 차서
// pipewrite()에서 sleep(&pi->nwrite, &pi->lock)이 호출된다.
// Reader가 데이터를 읽으면 wakeup(&pi->nwrite)으로
// Writer가 깨어나서 나머지를 쓴다.
// ============================================================
static void
demo_writer_blocks(void)
{
  int fds[2];
  int pid;

  printf("=== Demo 2: Writer blocks when pipe buffer is full ===\n");

  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    // 자식 (Reader) - 일부러 천천히 읽는다
    close(fds[1]);

    // Reader가 먼저 10 tick 대기하여
    // Writer가 버퍼를 가득 채우고 block되게 한다.
    printf("[Reader] Waiting 10 ticks before reading...\n");
    pause(10);

    char buf[128];
    int total = 0;
    int n;

    printf("[Reader] Starting to read (Writer may be blocked)...\n");

    // 데이터를 모두 읽는다
    while((n = read(fds[0], buf, sizeof(buf))) > 0){
      total += n;
    }

    printf("[Reader] Total bytes read: %d\n", total);

    close(fds[0]);
    exit(0);
  } else {
    // 부모 (Writer) - 큰 데이터를 한번에 쓴다
    close(fds[0]);

    // 600 bytes를 쓴다 (PIPESIZE=512보다 큼)
    // 'A' 문자로 채운 버퍼를 사용
    char bigbuf[600];
    for(int i = 0; i < 600; i++){
      bigbuf[i] = 'A' + (i % 26);
    }

    printf("[Writer] Trying to write 600 bytes (PIPESIZE=512)...\n");

    // write() 호출 시 커널 내부 동작:
    //   pipewrite() 진입, acquire(&pi->lock)
    //   -> 1 byte씩 while 루프에서 버퍼에 복사
    //   -> 512 bytes 쓴 후: nwrite == nread + PIPESIZE (버퍼 가득 참)
    //   -> wakeup(&pi->nread)  // Reader를 깨움
    //   -> sleep(&pi->nwrite, &pi->lock)  // Writer가 잠듦
    //
    //   Reader가 데이터를 읽으면:
    //   -> piperead() 안에서 nread 증가
    //   -> wakeup(&pi->nwrite)  // Writer를 깨움!
    //   -> Writer가 깨어나 나머지 88 bytes를 쓴다

    int written = write(fds[1], bigbuf, 600);
    printf("[Writer] Successfully wrote %d bytes\n", written);

    close(fds[1]);
    wait(0);
    printf("=== Demo 2 complete ===\n\n");
  }
}

// ============================================================
// Demo 3: Pipe close와 EOF
// ============================================================
// Writer가 write end를 닫으면 pipeclose()가 호출되어
// pi->writeopen = 0이 되고 wakeup(&pi->nread)이 호출된다.
// Reader는 piperead()의 while 조건에서 pi->writeopen == 0이므로
// 루프를 빠져나오고, 버퍼가 비어있으면 0을 반환한다 (EOF).
// ============================================================
static void
demo_pipe_close(void)
{
  int fds[2];
  int pid;

  printf("=== Demo 3: Pipe close triggers EOF for reader ===\n");

  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    // 자식 (Reader)
    close(fds[1]);

    char buf[32];
    int n;

    printf("[Reader] Waiting for data...\n");

    // 첫 번째 read: 데이터가 올 때까지 block
    n = read(fds[0], buf, sizeof(buf) - 1);
    if(n > 0){
      buf[n] = '\0';
      printf("[Reader] Got: \"%s\"\n", buf);
    }

    printf("[Reader] Calling read() again... waiting for more data or EOF\n");

    // 두 번째 read: Writer가 close하면 EOF (0 반환)
    // 커널 내부 동작:
    //   piperead() -> while(pi->nread == pi->nwrite && pi->writeopen)
    //   -> pi->writeopen == 0이므로 while 조건 false
    //   -> 루프 빠져나옴
    //   -> for 루프에서 pi->nread == pi->nwrite이므로 즉시 break
    //   -> return 0 (EOF)
    n = read(fds[0], buf, sizeof(buf) - 1);
    printf("[Reader] read() returned %d (0 = EOF)\n", n);

    close(fds[0]);
    exit(0);
  } else {
    // 부모 (Writer)
    close(fds[0]);

    // 데이터 하나 보내고
    char *msg = "last message";
    write(fds[1], msg, strlen(msg));
    printf("[Writer] Sent \"%s\"\n", msg);

    // 잠시 후 close
    pause(10);
    printf("[Writer] Closing write end of pipe...\n");

    // close() -> pipeclose() 호출
    //   -> pi->writeopen = 0
    //   -> wakeup(&pi->nread)  // Reader를 깨움
    //   Reader는 while 조건을 재확인하고
    //   pi->writeopen == 0이므로 루프 탈출
    close(fds[1]);

    wait(0);
    printf("=== Demo 3 complete ===\n\n");
  }
}

int
main(int argc, char *argv[])
{
  printf("\n");
  printf("========================================\n");
  printf("  Sleep/Wakeup Demo via Pipe Blocking\n");
  printf("========================================\n\n");

  demo_reader_waits();
  demo_writer_blocks();
  demo_pipe_close();

  printf("All demos completed.\n");
  exit(0);
}
