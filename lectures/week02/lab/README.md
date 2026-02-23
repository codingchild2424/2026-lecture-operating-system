# Week 2 실습: Process 1 - fork, exec, wait, pipe, I/O

> 교재: xv6 Ch 1 - Operating System Interfaces
> 소요 시간: 약 50분
> 환경: Linux 또는 macOS (gcc 필요)

---

## 학습 목표

이번 실습을 통해 다음을 이해합니다:

- `fork()`로 프로세스를 복제하고 `wait()`으로 자식을 기다리는 방법
- `exec()` 패밀리로 현재 프로세스를 새 프로그램으로 교체하는 방법
- `pipe()`로 프로세스 간 데이터를 주고받는 방법
- `dup2()`로 표준 입출력을 파일이나 파이프로 리다이렉트하는 방법

이 네 가지 시스템 콜의 조합이 Unix 셸의 핵심 원리입니다.

---

## 사전 준비

```bash
# gcc가 설치되어 있는지 확인
gcc --version

# 실습 디렉토리로 이동
cd week2/lab/examples/
```

---

## 실습 1: fork()와 wait() 기초 (~12분)

### 배경 지식

`fork()`는 현재 프로세스를 그대로 복제하여 자식 프로세스를 생성합니다.

```
fork() 호출 전:     fork() 호출 후:

  [부모 PID=100]      [부모 PID=100]  fork() 반환값 = 자식PID (200)
                       │
                       └─ [자식 PID=200]  fork() 반환값 = 0
```

- 부모 프로세스: `fork()`가 자식의 PID를 반환
- 자식 프로세스: `fork()`가 0을 반환
- 실패 시: -1 반환

`wait()`는 자식 프로세스가 종료될 때까지 부모를 블록(대기)시킵니다.

### 실습 단계

**단계 1**: 예제 코드를 컴파일하고 실행합니다.

```bash
gcc -Wall -o fork_basic fork_basic.c
./fork_basic
```

**단계 2**: 출력을 관찰하고 다음 질문에 답해보세요.

- Q1: 부모와 자식 중 누가 먼저 출력하나요? 실행할 때마다 같은가요?
- Q2: `wait()`을 제거하면 어떻게 되나요? (주석 처리 후 재컴파일)
- Q3: 자식이 `exit(42)`로 종료했을 때, 부모에서 42를 어떻게 확인하나요?

**단계 3**: 직접 수정해보기

`fork_basic.c`를 수정하여 자식 프로세스가 5개 생성되도록 바꿔보세요. 각 자식이 서로 다른 exit code(0~4)로 종료하게 하고, 부모에서 모든 종료 코드를 출력하세요.

### 핵심 정리

```c
pid_t pid = fork();
if (pid == 0) {
    // 자식 코드
    exit(0);
} else {
    // 부모 코드
    int status;
    wait(&status);            // 자식이 종료될 때까지 대기
    WEXITSTATUS(status);      // 자식의 exit code 추출
}
```

---

## 실습 2: exec() 패밀리 (~12분)

### 배경 지식

`exec()`은 현재 프로세스의 코드, 데이터, 스택을 새 프로그램으로 **완전히 교체**합니다. 성공하면 돌아오지 않습니다.

```
exec() 전:                exec() 후:
[프로세스 PID=200]        [프로세스 PID=200]
 코드: fork_basic          코드: /bin/ls        ← 완전히 교체됨
 데이터: ...               데이터: ...
 PID는 그대로!             PID는 그대로!
```

exec 패밀리 함수 이름 규칙:

| 접미사 | 의미 | 예시 |
|--------|------|------|
| `l` | 인자를 나열 (list) | `execl("/bin/ls", "ls", "-l", NULL)` |
| `v` | 인자를 배열로 (vector) | `execv("/bin/ls", argv)` |
| `p` | PATH 환경변수 검색 | `execlp("ls", "ls", "-l", NULL)` |
| `e` | 환경변수 직접 지정 | `execle(path, arg, ..., envp)` |

### 실습 단계

**단계 1**: 예제 코드를 컴파일하고 실행합니다.

```bash
gcc -Wall -o exec_example exec_example.c
./exec_example
```

**단계 2**: 출력을 관찰하고 다음 질문에 답해보세요.

- Q1: `exec()` 호출 뒤에 있는 `printf()`가 실행되나요? 왜 그런가요?
- Q2: Demo 6에서 exec 실패 시 `perror`가 출력되는 이유는 무엇인가요?
- Q3: `fork()` 없이 `exec()`만 호출하면 어떻게 되나요?

**단계 3**: 직접 수정해보기

`exec_example.c`에 새로운 demo를 추가해보세요: `execvp`를 사용하여 `grep`을 실행하고, 특정 파일에서 패턴을 검색하도록 만드세요.

```c
// 힌트: "grep" "pattern" "filename" 을 execvp로 실행
char *args[] = {"grep", "hello", "/etc/hosts", NULL};
execvp("grep", args);
```

### 핵심 정리

```
fork() + exec() + wait() = 새 프로그램 실행의 기본 패턴

부모:  fork() ──→ wait() ──→ 계속 실행
         │
자식:  exec("ls") ──→ (ls가 실행되고 종료)
```

왜 fork와 exec이 분리되어 있을까요?
- fork와 exec 사이에 자식 프로세스의 환경을 조작할 수 있습니다 (fd 리다이렉션, 환경변수 설정 등).
- 이것이 Unix 철학의 핵심 설계 중 하나입니다.

---

## 실습 3: pipe()로 프로세스 간 통신 (~13분)

### 배경 지식

`pipe()`는 커널 내부에 버퍼를 만들고, 읽기/쓰기용 파일 디스크립터 2개를 반환합니다.

```
pipe(fd) 호출 결과:

  쓰는 프로세스 ──write()──→ fd[1] ═══[커널 버퍼]═══ fd[0] ──read()──→ 읽는 프로세스
                            (쓰기 끝)                (읽기 끝)
```

중요한 규칙:
- 사용하지 않는 fd는 **반드시 close()** 해야 합니다.
- 쓰기 끝(`fd[1]`)이 모두 닫혀야 읽는 쪽의 `read()`가 EOF(0)를 반환합니다.

### 실습 단계

**단계 1**: 예제 코드를 컴파일하고 실행합니다.

```bash
gcc -Wall -o pipe_example pipe_example.c
./pipe_example
```

**단계 2**: 출력을 관찰하고 다음 질문에 답해보세요.

- Q1: Demo 1에서 부모가 `close(fd[0])`을, 자식이 `close(fd[1])`을 하는 이유는?
- Q2: Demo 2에서 양방향 통신을 위해 pipe를 몇 개 사용하나요? 왜 1개로는 안 되나요?
- Q3: Demo 3에서 `dup2(fd[1], STDOUT_FILENO)`는 무슨 역할을 하나요?

**단계 3**: 직접 수정해보기

Demo 1을 수정하여 부모가 여러 줄의 메시지를 보내고, 자식이 줄 단위로 읽어서 출력하도록 바꿔보세요.

```c
// 힌트: 자식에서 fdopen()으로 FILE*을 만들어 fgets()를 사용
FILE *fp = fdopen(fd[0], "r");
char line[256];
while (fgets(line, sizeof(line), fp) != NULL) {
    printf("[자식] %s", line);
}
fclose(fp);
```

### 핵심 정리

```c
int fd[2];
pipe(fd);           // fd[0]=읽기, fd[1]=쓰기
pid_t pid = fork();
if (pid == 0) {
    close(fd[1]);   // 자식: 쓰기 끝 닫기
    read(fd[0], buf, sizeof(buf));
    close(fd[0]);
} else {
    close(fd[0]);   // 부모: 읽기 끝 닫기
    write(fd[1], msg, len);
    close(fd[1]);   // 쓰기 끝 닫아야 자식이 EOF 받음
    wait(NULL);
}
```

---

## 실습 4: dup()과 I/O 리다이렉션 (~13분)

### 배경 지식

모든 프로세스는 파일 디스크립터 테이블을 갖고 있습니다:

```
fd 번호 → 파일 (커널 내부 구조체)
  0     → stdin  (키보드)
  1     → stdout (화면)
  2     → stderr (화면)
  3     → (open으로 열면 여기부터 할당)
```

`dup2(oldfd, newfd)` = newfd를 닫고, newfd가 oldfd와 같은 파일을 가리키게 합니다.

```
셸의 "echo hello > output.txt" 구현:

1. fd = open("output.txt", O_WRONLY|O_CREAT|O_TRUNC)   → fd=3
2. dup2(3, 1)     → stdout(1)이 output.txt를 가리킴
3. close(3)       → 원본 fd 정리
4. exec("echo")   → echo의 stdout 출력이 파일로 간다!
```

### 실습 단계

**단계 1**: 예제 코드를 컴파일하고 실행합니다.

```bash
gcc -Wall -o redirect redirect.c
./redirect
```

**단계 2**: 출력을 관찰하고 다음 질문에 답해보세요.

- Q1: Demo 1에서 `dup2(fd, STDOUT_FILENO)` 후 `close(fd)`를 하는 이유는?
- Q2: Demo 3에서 `dup(fd)`와 `dup2(oldfd, newfd)`의 차이점은?
- Q3: Demo 4의 패턴이 셸에서 `sort < input > output`을 실행하는 것과 어떻게 대응되나요?

**단계 3**: 직접 수정해보기

Demo 4를 수정하여 stderr를 별도의 에러 파일로 리다이렉트하는 기능을 추가해보세요 (`2> error.txt`에 해당).

```c
// 힌트: stderr = fd 2
int err_fd = open("error.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(err_fd, STDERR_FILENO);  // stderr → error.txt
close(err_fd);
```

### 핵심 정리

```
셸의 리다이렉션 구현 패턴:

  fork()
    │
    ├─ 자식:
    │    1. open() 으로 파일 열기
    │    2. dup2() 로 stdin/stdout 교체
    │    3. close() 로 원본 fd 정리
    │    4. exec() 로 명령어 실행
    │
    └─ 부모:
         wait() 로 자식 종료 대기
```

---

## 전체 요약

이번 실습에서 배운 4가지 시스템 콜의 관계:

```
┌─────────────────────────────────────────────────────┐
│                    셸의 동작 원리                       │
│                                                       │
│   $ sort < input.txt | grep apple > output.txt       │
│                                                       │
│   1. pipe() → 파이프 생성                              │
│   2. fork() → 자식1 (sort)                            │
│      - dup2(input.txt → stdin)    [리다이렉션]         │
│      - dup2(pipe_write → stdout)  [파이프]             │
│      - exec("sort")                                   │
│   3. fork() → 자식2 (grep)                            │
│      - dup2(pipe_read → stdin)    [파이프]             │
│      - dup2(output.txt → stdout)  [리다이렉션]         │
│      - exec("grep", "apple")                          │
│   4. wait() × 2                                       │
└─────────────────────────────────────────────────────┘
```

| 시스템 콜 | 역할 |
|-----------|------|
| `fork()` | 프로세스 복제 (새 자식 생성) |
| `exec()` | 프로세스 이미지 교체 (새 프로그램 실행) |
| `wait()` | 자식 종료 대기 |
| `pipe()` | 프로세스 간 통신 채널 생성 |
| `dup2()` | 파일 디스크립터 교체 (I/O 리다이렉션) |

---

## 다음 단계

이번 실습에서 배운 시스템 콜들을 조합하여 이번 주 과제에서 다음을 구현합니다:

1. **pingpong**: pipe를 사용한 양방향 통신 프로그램
2. **minishell**: fork + exec + wait + pipe + dup2를 조합한 간단한 셸

과제 명세는 `week2/homework/README.md`를 참고하세요.
