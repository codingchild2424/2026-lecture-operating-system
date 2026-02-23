/*
 * exec_example.c - exec() 패밀리 함수들의 동작을 보여주는 예제
 *
 * Compile: gcc -Wall -o exec_example exec_example.c
 * Run:     ./exec_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * exec() 함수는 현재 프로세스의 이미지를 새 프로그램으로 교체한다.
 * exec() 호출 이후의 코드는 실행되지 않는다 (성공 시).
 *
 * exec 패밀리 이름 규칙:
 *   l = list (인자를 나열)     vs  v = vector (인자를 배열로)
 *   p = PATH 검색 사용         vs  없음 = 전체 경로 필요
 *   e = 환경변수 직접 지정     vs  없음 = 현재 환경 상속
 *
 *   execl, execlp, execle, execv, execvp, execvpe
 */

/* helper: fork 후 자식에서 exec 실행, 부모는 wait */
static void run_demo(const char *title, void (*demo)(void))
{
    printf("\n--- %s ---\n", title);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        demo();
        /* exec이 성공하면 여기에 도달하지 않는다 */
        perror("exec failed");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("(자식 종료, exit code=%d)\n", WEXITSTATUS(status));
        }
    }
}

/* Demo 1: execl - 전체 경로 + 인자 나열 */
static void demo_execl(void)
{
    printf("[자식 PID=%d] execl로 /bin/echo 실행\n", getpid());
    /*
     * execl(path, arg0, arg1, ..., NULL)
     * arg0은 관례상 프로그램 이름
     */
    execl("/bin/echo", "echo", "Hello", "from", "execl!", (char *)NULL);
    /* execl이 성공하면 이 줄은 실행되지 않음 */
}

/* Demo 2: execlp - PATH 검색 + 인자 나열 */
static void demo_execlp(void)
{
    printf("[자식 PID=%d] execlp로 ls 실행 (PATH 검색)\n", getpid());
    /*
     * execlp(file, arg0, arg1, ..., NULL)
     * PATH 환경변수에서 file을 검색
     */
    execlp("ls", "ls", "-l", "-h", "/tmp", (char *)NULL);
}

/* Demo 3: execv - 전체 경로 + 인자 배열 */
static void demo_execv(void)
{
    printf("[자식 PID=%d] execv로 /bin/echo 실행 (배열 사용)\n", getpid());
    /*
     * execv(path, argv[])
     * argv는 NULL로 끝나는 문자열 배열
     */
    char *args[] = {"echo", "Hello", "from", "execv!", NULL};
    execv("/bin/echo", args);
}

/* Demo 4: execvp - PATH 검색 + 인자 배열 */
static void demo_execvp(void)
{
    printf("[자식 PID=%d] execvp로 wc 실행\n", getpid());
    /*
     * execvp(file, argv[])
     * PATH에서 검색 + 배열 인자
     * 이 조합이 실무에서 가장 많이 사용됨
     */
    char *args[] = {"echo", "execvp works!", NULL};
    execvp("echo", args);
}

/* Demo 5: fork + exec + wait = 새 프로그램 실행의 기본 패턴 */
static void demo_pattern(void)
{
    printf("[자식 PID=%d] fork+exec 패턴: date 명령어 실행\n", getpid());
    execlp("date", "date", (char *)NULL);
}

/* Demo 6: exec 실패 처리 */
static void demo_exec_fail(void)
{
    printf("[자식 PID=%d] 존재하지 않는 프로그램 실행 시도\n", getpid());
    execlp("this_program_does_not_exist",
           "this_program_does_not_exist", (char *)NULL);
    /* exec 실패 시에만 여기에 도달 */
    perror("exec failed (예상된 에러)");
    exit(127);
}

int main(void)
{
    printf("=== exec() 패밀리 데모 ===\n");
    printf("현재 프로세스 PID=%d\n", getpid());

    printf("\n핵심 개념:\n");
    printf("  - exec()는 현재 프로세스의 코드/데이터를 새 프로그램으로 교체\n");
    printf("  - exec() 성공 시 호출 이후 코드는 절대 실행되지 않음\n");
    printf("  - 따라서 보통 fork() 후 자식에서 exec()를 호출\n");

    run_demo("1. execl: 전체 경로 + 인자 나열", demo_execl);
    run_demo("2. execlp: PATH 검색 + 인자 나열", demo_execlp);
    run_demo("3. execv: 전체 경로 + 인자 배열", demo_execv);
    run_demo("4. execvp: PATH 검색 + 인자 배열 (가장 많이 사용)", demo_execvp);
    run_demo("5. fork+exec+wait 패턴: date 실행", demo_pattern);
    run_demo("6. exec 실패 처리", demo_exec_fail);

    printf("\n=== 정리 ===\n");
    printf("  execl(path, arg0, ..., NULL)  - 경로 직접 지정, 인자 나열\n");
    printf("  execlp(file, arg0, ..., NULL) - PATH 검색, 인자 나열\n");
    printf("  execv(path, argv[])           - 경로 직접 지정, 인자 배열\n");
    printf("  execvp(file, argv[])          - PATH 검색, 인자 배열\n");
    printf("\n셸에서 가장 많이 쓰는 패턴: fork() → 자식에서 execvp() → 부모에서 wait()\n");

    return 0;
}
