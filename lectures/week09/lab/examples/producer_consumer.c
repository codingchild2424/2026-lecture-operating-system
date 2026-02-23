// producer_consumer.c - xv6 user program
// pipe를 이용한 producer-consumer 패턴 데모
//
// Producer (부모 프로세스): 여러 개의 메시지를 pipe에 쓴다
// Consumer (자식 프로세스): pipe에서 메시지를 읽어 출력한다
//
// 빌드 방법:
//   1. 이 파일을 user/ 디렉토리에 복사
//   2. Makefile의 UPROGS에 $U/_producer_consumer\ 추가
//   3. make clean && make qemu

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_MESSAGES 5
#define MSG_SIZE 64

// 간단한 정수를 문자열로 변환 (xv6에는 sprintf가 없음)
static void
int_to_str(int n, char *buf)
{
  if(n == 0){
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }

  char tmp[12];
  int i = 0;
  int neg = 0;

  if(n < 0){
    neg = 1;
    n = -n;
  }

  while(n > 0){
    tmp[i++] = '0' + (n % 10);
    n /= 10;
  }

  int j = 0;
  if(neg)
    buf[j++] = '-';

  while(i > 0)
    buf[j++] = tmp[--i];

  buf[j] = '\0';
}

// 메시지 생성: "msg N: hello from producer"
static int
make_message(int seq, char *buf, int bufsize)
{
  char num[12];
  int_to_str(seq, num);

  // 수동 문자열 조합
  char *parts[] = {"msg ", num, ": hello from producer"};
  int nparts = 3;
  int pos = 0;

  for(int p = 0; p < nparts; p++){
    for(int k = 0; parts[p][k] != '\0' && pos < bufsize - 1; k++){
      buf[pos++] = parts[p][k];
    }
  }
  buf[pos] = '\0';
  return pos;
}

// Consumer: pipe에서 메시지를 읽어 출력
static void
consumer(int readfd)
{
  char buf[MSG_SIZE];
  int n;

  printf("[Consumer] Reading from pipe...\n");

  // pipe에서 데이터를 읽는다.
  // 데이터가 없으면 piperead() 내부에서 sleep한다.
  // writer가 데이터를 쓰면 wakeup으로 깨어난다.
  while(1){
    // 먼저 메시지 길이(1 byte)를 읽는다
    char lenbuf;
    n = read(readfd, &lenbuf, 1);
    if(n <= 0)
      break;

    int msglen = (int)lenbuf;
    if(msglen <= 0 || msglen >= MSG_SIZE)
      break;

    // 메시지 본문을 읽는다
    int total = 0;
    while(total < msglen){
      n = read(readfd, buf + total, msglen - total);
      if(n <= 0)
        break;
      total += n;
    }
    if(total < msglen)
      break;

    buf[total] = '\0';
    printf("[Consumer] Received: %s\n", buf);
  }

  printf("[Consumer] Pipe closed by producer (read returned 0). Exiting.\n");
}

// Producer: pipe에 메시지를 쓴다
static void
producer(int writefd)
{
  char buf[MSG_SIZE];

  printf("[Producer] Sending %d messages through pipe...\n", NUM_MESSAGES);

  for(int i = 0; i < NUM_MESSAGES; i++){
    int msglen = make_message(i, buf, MSG_SIZE);

    // 먼저 메시지 길이(1 byte)를 쓴다
    char lenbuf = (char)msglen;
    write(writefd, &lenbuf, 1);

    // 메시지 본문을 쓴다
    // 버퍼가 가득 차면 pipewrite() 내부에서 sleep한다.
    // reader가 데이터를 읽으면 wakeup으로 깨어난다.
    write(writefd, buf, msglen);

    printf("[Producer] Sent: %s\n", buf);
  }

  printf("[Producer] Done. Closing write end.\n");
}

int
main(int argc, char *argv[])
{
  int fds[2];  // fds[0] = read end, fds[1] = write end
  int pid;

  // pipe 시스템 콜로 pipe를 생성한다.
  // 커널 내부에서 pipealloc()이 호출되어
  // struct pipe가 할당되고, 두 개의 file descriptor가 반환된다.
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  // fork()로 자식 프로세스를 생성한다.
  // 자식은 부모의 file descriptor 테이블을 복사받으므로
  // 같은 pipe의 양쪽 끝에 접근할 수 있다.
  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    // ---- 자식 프로세스 (Consumer) ----
    // write end는 사용하지 않으므로 닫는다.
    // 이것이 중요한 이유: write end가 열려 있으면
    // piperead()에서 pi->writeopen이 1이므로
    // 데이터가 없어도 EOF 대신 sleep한다.
    close(fds[1]);

    consumer(fds[0]);

    close(fds[0]);
    exit(0);
  } else {
    // ---- 부모 프로세스 (Producer) ----
    // read end는 사용하지 않으므로 닫는다.
    close(fds[0]);

    producer(fds[1]);

    // write end를 닫는다.
    // 커널 내부에서 pipeclose()가 호출되어:
    //   pi->writeopen = 0
    //   wakeup(&pi->nread) -> reader가 깨어남
    // reader는 piperead()의 while 조건에서
    // pi->writeopen == 0이므로 루프를 빠져나오고
    // read()가 0을 반환한다 (EOF).
    close(fds[1]);

    // 자식 프로세스가 끝나길 기다린다.
    // 커널 내부에서 kwait()가 호출되고,
    // 자식이 아직 살아있으면 sleep(p, &wait_lock)으로 대기한다.
    wait(0);
    exit(0);
  }
}
