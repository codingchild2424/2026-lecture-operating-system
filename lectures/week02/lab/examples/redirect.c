/*
 * redirect.c - dup2()를 사용한 I/O 리다이렉션 예제
 *
 * Compile: gcc -Wall -o redirect redirect.c
 * Run:     ./redirect
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

/*
 * 파일 디스크립터(fd) 기본 개념:
 *   0 = stdin  (표준 입력)
 *   1 = stdout (표준 출력)
 *   2 = stderr (표준 에러)
 *
 * dup2(oldfd, newfd):
 *   newfd가 oldfd와 같은 파일을 가리키게 한다.
 *   기존 newfd는 닫힌다.
 *
 * 리다이렉션 원리:
 *   "cmd > file" = stdout(1)을 file로 연결
 *   "cmd < file" = stdin(0)을 file로 연결
 */

/* Demo 1: 출력 리다이렉션 (stdout → 파일) */
static void demo_output_redirect(void)
{
    printf("\n=== Demo 1: 출력 리다이렉션 (echo > output.txt) ===\n\n");

    const char *filename = "/tmp/redirect_demo_output.txt";

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* 자식: stdout을 파일로 리다이렉트한 후 echo 실행 */

        /* 파일 열기 (쓰기용, 없으면 생성, 있으면 덮어씀) */
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }

        /* stdout(1)을 fd가 가리키는 파일로 교체 */
        dup2(fd, STDOUT_FILENO);
        close(fd);  /* 원본 fd는 더 이상 필요 없음 */

        /* 이제 echo의 출력이 파일로 간다 */
        execlp("echo", "echo", "이 내용은 파일에 저장됩니다!", (char *)NULL);
        perror("exec");
        exit(1);
    }

    wait(NULL);

    /* 파일 내용 확인 */
    printf("[부모] %s 파일 내용:\n", filename);
    pid_t cat_pid = fork();
    if (cat_pid == 0) {
        execlp("cat", "cat", filename, (char *)NULL);
        exit(1);
    }
    wait(NULL);
    printf("\n");

    /* 임시 파일 정리 */
    unlink(filename);
}

/* Demo 2: 입력 리다이렉션 (파일 → stdin) */
static void demo_input_redirect(void)
{
    printf("\n=== Demo 2: 입력 리다이렉션 (wc < input.txt) ===\n\n");

    const char *filename = "/tmp/redirect_demo_input.txt";

    /* 먼저 입력 파일을 생성 */
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }
    const char *content = "apple\nbanana\ncherry\ndate\nelderberry\n";
    write(fd, content, strlen(content));
    close(fd);

    printf("[부모] 입력 파일 내용:\n%s\n", content);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* 자식: stdin을 파일로 리다이렉트한 후 wc 실행 */
        int infd = open(filename, O_RDONLY);
        if (infd < 0) {
            perror("open");
            exit(1);
        }

        /* stdin(0)을 파일로 교체 */
        dup2(infd, STDIN_FILENO);
        close(infd);

        /* wc가 stdin에서 읽으면 파일 내용을 읽게 된다 */
        printf("[자식] wc -l 실행 결과 (줄 수): ");
        fflush(stdout);
        execlp("wc", "wc", "-l", (char *)NULL);
        perror("exec");
        exit(1);
    }

    wait(NULL);

    /* 임시 파일 정리 */
    unlink(filename);
}

/* Demo 3: dup2 없이 dup()으로 fd 복제 이해하기 */
static void demo_dup_basics(void)
{
    printf("\n=== Demo 3: dup() 기본 동작 이해 ===\n\n");

    const char *filename = "/tmp/redirect_demo_dup.txt";
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }

    printf("원본 fd = %d\n", fd);

    /*
     * dup(fd)는 fd와 같은 파일을 가리키는 새 fd를 반환한다.
     * 가장 작은 사용 가능한 fd 번호를 사용.
     */
    int new_fd = dup(fd);
    printf("dup(%d) = %d (같은 파일을 가리킴)\n", fd, new_fd);

    /* 두 fd 모두 같은 파일에 쓸 수 있다 */
    const char *msg1 = "Written via original fd\n";
    const char *msg2 = "Written via duplicated fd\n";
    write(fd, msg1, strlen(msg1));
    write(new_fd, msg2, strlen(msg2));

    close(fd);
    close(new_fd);

    /* 파일 내용 확인 */
    printf("\n파일 내용:\n");
    pid_t pid = fork();
    if (pid == 0) {
        execlp("cat", "cat", filename, (char *)NULL);
        exit(1);
    }
    wait(NULL);

    unlink(filename);
}

/* Demo 4: 셸의 리다이렉션 구현 패턴 */
static void demo_shell_redirect_pattern(void)
{
    printf("\n=== Demo 4: 셸이 리다이렉션을 구현하는 패턴 ===\n\n");

    /*
     * 셸에서 "sort < input > output" 을 실행하는 과정:
     *
     * 1. fork()로 자식 생성
     * 2. 자식에서:
     *    a. input 파일을 열어 stdin으로 dup2
     *    b. output 파일을 열어 stdout으로 dup2
     *    c. exec("sort")
     * 3. 부모에서 wait()
     */

    const char *infile = "/tmp/redirect_demo_sort_in.txt";
    const char *outfile = "/tmp/redirect_demo_sort_out.txt";

    /* 입력 파일 생성 */
    int fd = open(infile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    const char *data = "cherry\napple\nbanana\ndate\n";
    write(fd, data, strlen(data));
    close(fd);

    printf("입력 파일 (%s):\n%s\n", infile, data);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* 입력 리다이렉션: stdin ← infile */
        int in_fd = open(infile, O_RDONLY);
        if (in_fd < 0) { perror("open input"); exit(1); }
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);

        /* 출력 리다이렉션: stdout → outfile */
        int out_fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) { perror("open output"); exit(1); }
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);

        /* sort 실행 (stdin에서 읽고 stdout으로 출력) */
        execlp("sort", "sort", (char *)NULL);
        perror("exec");
        exit(1);
    }

    wait(NULL);

    /* 결과 확인 */
    printf("출력 파일 (%s):\n", outfile);
    pid_t cat_pid = fork();
    if (cat_pid == 0) {
        execlp("cat", "cat", outfile, (char *)NULL);
        exit(1);
    }
    wait(NULL);

    /* 임시 파일 정리 */
    unlink(infile);
    unlink(outfile);
}

int main(void)
{
    printf("=== dup2() I/O 리다이렉션 데모 ===\n");
    printf("핵심: dup2(oldfd, newfd) → newfd가 oldfd와 같은 파일을 가리킴\n");

    demo_output_redirect();
    demo_input_redirect();
    demo_dup_basics();
    demo_shell_redirect_pattern();

    printf("\n=== 정리 ===\n");
    printf("  > file  : fd = open(file, O_WRONLY|O_CREAT|O_TRUNC); dup2(fd, 1);\n");
    printf("  < file  : fd = open(file, O_RDONLY);                  dup2(fd, 0);\n");
    printf("  >> file : fd = open(file, O_WRONLY|O_CREAT|O_APPEND); dup2(fd, 1);\n");
    printf("\n  셸 패턴: fork() → [자식: dup2로 리다이렉트 → exec()] → [부모: wait()]\n");

    return 0;
}
