/*
 * pipe_example.c - pipe()를 사용한 프로세스 간 통신 예제
 *
 * Compile: gcc -Wall -o pipe_example pipe_example.c
 * Run:     ./pipe_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * pipe()는 단방향 통신 채널을 생성한다.
 *
 *   int fd[2];
 *   pipe(fd);
 *
 *   fd[0] = 읽기 끝 (read end)
 *   fd[1] = 쓰기 끝 (write end)
 *
 *   쓰기: write(fd[1], buf, len)
 *   읽기: read(fd[0], buf, len)
 *
 * 데이터 흐름:  쓰는 쪽 ---> fd[1] ====pipe==== fd[0] ---> 읽는 쪽
 */

/* Demo 1: 부모 → 자식 단방향 통신 */
static void demo_parent_to_child(void)
{
    printf("\n=== Demo 1: 부모 → 자식 단방향 통신 ===\n\n");

    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* 자식: 읽기만 하므로 쓰기 끝을 닫는다 */
        close(fd[1]);

        char buf[256];
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[자식] 부모로부터 받은 메시지: \"%s\"\n", buf);
        }

        close(fd[0]);
        exit(0);
    } else {
        /* 부모: 쓰기만 하므로 읽기 끝을 닫는다 */
        close(fd[0]);

        const char *msg = "안녕, 자식 프로세스!";
        printf("[부모] 자식에게 메시지 전송: \"%s\"\n", msg);
        write(fd[1], msg, strlen(msg));

        close(fd[1]);  /* 쓰기 끝을 닫아야 자식의 read()가 EOF를 받음 */
        wait(NULL);
    }
}

/* Demo 2: 양방향 통신 (pipe 2개 사용) */
static void demo_bidirectional(void)
{
    printf("\n=== Demo 2: 양방향 통신 (pipe 2개) ===\n\n");

    int parent_to_child[2];  /* 부모 → 자식 */
    int child_to_parent[2];  /* 자식 → 부모 */

    if (pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* 자식: parent_to_child에서 읽고, child_to_parent에 쓴다 */
        close(parent_to_child[1]);  /* 쓰기 끝 닫기 */
        close(child_to_parent[0]);  /* 읽기 끝 닫기 */

        char buf[256];
        ssize_t n = read(parent_to_child[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[자식] 받은 메시지: \"%s\"\n", buf);
        }

        const char *reply = "잘 받았어요, 부모님!";
        printf("[자식] 답장 전송: \"%s\"\n", reply);
        write(child_to_parent[1], reply, strlen(reply));

        close(parent_to_child[0]);
        close(child_to_parent[1]);
        exit(0);
    } else {
        /* 부모 */
        close(parent_to_child[0]);  /* 읽기 끝 닫기 */
        close(child_to_parent[1]);  /* 쓰기 끝 닫기 */

        const char *msg = "안녕, 잘 지내니?";
        printf("[부모] 메시지 전송: \"%s\"\n", msg);
        write(parent_to_child[1], msg, strlen(msg));
        close(parent_to_child[1]);  /* 전송 완료 */

        char buf[256];
        ssize_t n = read(child_to_parent[0], buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[부모] 자식의 답장: \"%s\"\n", buf);
        }

        close(child_to_parent[0]);
        wait(NULL);
    }
}

/* Demo 3: pipe로 두 명령어 연결 (셸의 cmd1 | cmd2 원리) */
static void demo_pipe_commands(void)
{
    printf("\n=== Demo 3: pipe로 명령어 연결 (echo | wc) ===\n\n");

    /*
     * 셸에서 "echo hello world | wc -w" 를 실행하는 것과 같은 원리:
     *
     *   echo의 stdout → pipe → wc의 stdin
     */

    int fd[2];
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(1);
    }

    /* 첫 번째 자식: echo (stdout을 pipe의 쓰기 끝으로) */
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid1 == 0) {
        /* echo: stdout을 pipe의 쓰기 끝으로 연결 */
        close(fd[0]);         /* 읽기 끝 닫기 */
        dup2(fd[1], STDOUT_FILENO);  /* stdout → pipe write end */
        close(fd[1]);         /* 원본 fd 닫기 */

        execlp("echo", "echo", "hello", "world", "from", "pipe", (char *)NULL);
        perror("exec echo");
        exit(1);
    }

    /* 두 번째 자식: wc (stdin을 pipe의 읽기 끝으로) */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }

    if (pid2 == 0) {
        /* wc: stdin을 pipe의 읽기 끝으로 연결 */
        close(fd[1]);         /* 쓰기 끝 닫기 */
        dup2(fd[0], STDIN_FILENO);   /* stdin → pipe read end */
        close(fd[0]);         /* 원본 fd 닫기 */

        execlp("wc", "wc", "-w", (char *)NULL);
        perror("exec wc");
        exit(1);
    }

    /* 부모: pipe의 양쪽 끝을 모두 닫고 자식들을 기다림 */
    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    printf("(위 숫자는 echo가 출력한 단어 수입니다)\n");
}

int main(void)
{
    printf("=== pipe() 프로세스 간 통신 데모 ===\n");
    printf("핵심: pipe(fd) → fd[0]=읽기, fd[1]=쓰기\n");

    demo_parent_to_child();
    demo_bidirectional();
    demo_pipe_commands();

    printf("\n=== 주의사항 정리 ===\n");
    printf("1. 사용하지 않는 pipe fd는 반드시 close()할 것\n");
    printf("   - 쓰기 끝이 열려있으면 읽는 쪽의 read()가 EOF를 받지 못함\n");
    printf("2. pipe는 단방향 → 양방향 통신에는 pipe 2개 필요\n");
    printf("3. pipe 버퍼가 가득 차면 write()가 블록됨\n");

    return 0;
}
