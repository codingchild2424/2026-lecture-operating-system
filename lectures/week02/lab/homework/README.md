# Week 2 과제: Process 1 - fork, exec, wait, pipe, I/O

> 교재: xv6 Ch 1 - Operating System Interfaces
> 마감: 다음 수업 시작 전
> 제출: 소스 코드 파일 (`pingpong.c`, `minishell.c`)

---

## 개요

이번 주 과제는 2개입니다. 모두 **필수**입니다.

| 과제 | 난이도 | 핵심 개념 | 파일 |
|------|--------|-----------|------|
| 1. pingpong | 기초 | pipe, fork, read/write | `pingpong.c` |
| 2. minishell | 중급 | fork, exec, wait, pipe, dup2 | `minishell.c` |

---

## 환경

```bash
# 필요한 도구
gcc --version   # gcc 필요

# 컴파일 방법 (모든 과제 공통)
gcc -Wall -o output source.c

# 테스트 실행
cd tests/
chmod +x *.sh
./test_pingpong.sh ../skeleton/pingpong.c
./test_minishell.sh ../skeleton/minishell.c
```

---

## 과제 1: pingpong (필수)

### 설명

xv6 교재 Ch 1 Exercise 1에 기반한 과제입니다.

두 프로세스(부모와 자식)가 파이프를 통해 1바이트를 주고받는 프로그램을 작성하세요.

```
부모 ──[ping: 1바이트]──→ 자식
부모 ←──[pong: 1바이트]── 자식
```

### 동작

1. 부모가 자식에게 1바이트(`'p'`)를 보냅니다 (ping).
2. 자식이 바이트를 받고 `<pid>: received ping`을 출력합니다.
3. 자식이 부모에게 1바이트를 다시 보냅니다 (pong).
4. 부모가 바이트를 받고 `<pid>: received pong`을 출력합니다.
5. 왕복 시간을 마이크로초 단위로 측정하여 출력합니다.

### 출력 형식 (정확히 이 형식을 따라야 합니다)

```
<자식PID>: received ping
<부모PID>: received pong
Round-trip time: X.XXX us
```

예시:

```
12345: received ping
12344: received pong
Round-trip time: 156.000 us
```

### 구현 가이드

스켈레톤 파일 `skeleton/pingpong.c`에 TODO 주석이 있습니다. 각 TODO를 순서대로 구현하세요.

**필요한 시스템 콜**: `pipe()`, `fork()`, `read()`, `write()`, `close()`, `wait()`, `getpid()`

**핵심 포인트**:

1. 양방향 통신을 위해 **pipe 2개**가 필요합니다.
2. 각 프로세스에서 사용하지 않는 파이프 fd는 반드시 `close()`해야 합니다.
3. ping을 받는 쪽은 자식, pong을 받는 쪽은 부모입니다.

**힌트** (막히면 참고하세요):

<details>
<summary>힌트 1: pipe 구성</summary>

```
parent_to_child[2]:  부모가 [1]에 쓰고, 자식이 [0]에서 읽음
child_to_parent[2]:  자식이 [1]에 쓰고, 부모가 [0]에서 읽음
```

</details>

<details>
<summary>힌트 2: 자식 프로세스에서 닫아야 할 fd</summary>

```c
// 자식은 parent_to_child에서 읽기만, child_to_parent에 쓰기만
close(parent_to_child[1]);  // 쓰기 끝 닫기
close(child_to_parent[0]);  // 읽기 끝 닫기
```

</details>

<details>
<summary>힌트 3: read/write 사용법</summary>

```c
char buf = 'p';
write(parent_to_child[1], &buf, 1);  // 1바이트 쓰기
read(parent_to_child[0], &buf, 1);   // 1바이트 읽기
```

</details>

### 테스트

```bash
cd tests/
./test_pingpong.sh ../skeleton/pingpong.c
```

테스트 항목:
- 컴파일 성공
- 정상 종료
- "received ping" / "received pong" 출력 확인
- PID가 서로 다른지 확인
- Round-trip time 출력 확인
- 반복 실행 안정성

---

## 과제 2: minishell (필수)

### 설명

간단한 셸(shell)을 구현합니다. 이 과제를 통해 실제 셸이 어떻게 명령어를 실행하는지 이해할 수 있습니다.

### 지원 기능

| 기능 | 예시 | 설명 |
|------|------|------|
| 단일 명령어 | `ls -l` | fork + execvp + wait |
| 출력 리다이렉션 | `echo hello > file.txt` | stdout을 파일로 |
| 입력 리다이렉션 | `sort < data.txt` | stdin을 파일로부터 |
| 파이프 | `ls \| wc -l` | cmd1의 stdout을 cmd2의 stdin으로 |
| 종료 | `exit` | 셸 종료 |

### 구현 가이드

스켈레톤 파일 `skeleton/minishell.c`에는 다음이 이미 구현되어 있습니다:
- `main()` 함수 (입력 읽기, 파이프 분리, exit 처리)
- `parse_command()` 함수 (명령어 파싱, 리다이렉션 파싱)
- `struct command` 구조체

**학생이 구현해야 하는 함수**:

#### 1. `execute_command(struct command *cmd)`

단일 명령어를 실행합니다.

```
[구현 순서]
1. fork()로 자식 생성
2. 자식에서:
   - cmd->infile이 있으면: open() → dup2(fd, STDIN_FILENO) → close(fd)
   - cmd->outfile이 있으면: open() → dup2(fd, STDOUT_FILENO) → close(fd)
   - execvp(cmd->argv[0], cmd->argv) 실행
   - exec 실패 시 에러 출력 후 exit(127)
3. 부모에서: waitpid()로 자식 종료 대기
```

#### 2. `execute_pipe(struct command *cmd1, struct command *cmd2)`

파이프로 연결된 두 명령어를 실행합니다.

```
[구현 순서]
1. pipe(fd)로 파이프 생성
2. fork()로 자식1 생성 (cmd1 실행):
   - fd[0] 닫기
   - dup2(fd[1], STDOUT_FILENO)로 stdout을 pipe 쓰기 끝으로
   - close(fd[1])
   - cmd1->infile이 있으면 입력 리다이렉션
   - execvp(cmd1->argv[0], cmd1->argv)
3. fork()로 자식2 생성 (cmd2 실행):
   - fd[1] 닫기
   - dup2(fd[0], STDIN_FILENO)로 stdin을 pipe 읽기 끝으로
   - close(fd[0])
   - cmd2->outfile이 있으면 출력 리다이렉션
   - execvp(cmd2->argv[0], cmd2->argv)
4. 부모에서: fd[0], fd[1] 닫기, 자식 2개 모두 waitpid
```

### execute_command부터 구현하세요

`execute_pipe`는 `execute_command`의 확장입니다. 먼저 `execute_command`를 완성하고 테스트한 후, `execute_pipe`를 구현하세요.

**힌트** (막히면 참고하세요):

<details>
<summary>힌트 1: execute_command의 기본 구조</summary>

```c
pid_t pid = fork();
if (pid == 0) {
    // 자식: 리다이렉션 설정 후 exec
    execvp(cmd->argv[0], cmd->argv);
    perror("minishell");
    exit(127);
}
// 부모: wait
int status;
waitpid(pid, &status, 0);
```

</details>

<details>
<summary>힌트 2: 입력 리다이렉션</summary>

```c
if (cmd->infile != NULL) {
    int fd = open(cmd->infile, O_RDONLY);
    if (fd < 0) {
        perror(cmd->infile);
        exit(1);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
}
```

</details>

<details>
<summary>힌트 3: execute_pipe에서 부모가 반드시 해야 할 일</summary>

부모 프로세스에서 pipe의 양쪽 끝을 **반드시** 닫아야 합니다. 그렇지 않으면 자식2의 `read()`가 EOF를 받지 못해 영원히 블록됩니다.

```c
close(fd[0]);
close(fd[1]);
waitpid(pid1, NULL, 0);
waitpid(pid2, &status, 0);
```

</details>

### 테스트

```bash
cd tests/
./test_minishell.sh ../skeleton/minishell.c
```

테스트 항목:
- 컴파일 성공
- `echo` 명령어 실행
- 명령어 인자 처리
- 출력 리다이렉션 (`>`)
- 입력 리다이렉션 (`<`)
- 파이프 (`|`)
- 파이프 + 리다이렉션 조합
- `exit` 명령어
- EOF 처리
- 존재하지 않는 명령어 에러 처리
- 빈 입력 처리
- 연속 명령어 실행

### 수동 테스트

자동 테스트 외에도 직접 셸을 실행하여 다양한 명령어를 시도해보세요:

```bash
$ ./minishell
minishell> ls
minishell> echo hello world
hello world
minishell> echo test > /tmp/test.txt
minishell> cat /tmp/test.txt
test
minishell> cat < /tmp/test.txt
test
minishell> ls | wc -l
minishell> echo hello | cat > /tmp/pipe_test.txt
minishell> cat /tmp/pipe_test.txt
hello
minishell> exit
```

---

## 제출 방법

다음 파일을 제출하세요:
1. `pingpong.c` - 완성된 pingpong 프로그램
2. `minishell.c` - 완성된 minishell 프로그램

### 채점 기준

| 항목 | 배점 |
|------|------|
| pingpong - 컴파일 및 정상 실행 | 10% |
| pingpong - 올바른 출력 형식 | 15% |
| pingpong - Round-trip time 측정 | 5% |
| minishell - 단일 명령어 실행 | 20% |
| minishell - 출력 리다이렉션 | 15% |
| minishell - 입력 리다이렉션 | 15% |
| minishell - 파이프 | 20% |

### 주의사항

- **모든 코드는 `gcc -Wall`로 경고 없이 컴파일되어야 합니다.**
- macOS와 Linux 모두에서 동작해야 합니다.
- `fork()` 실패, `exec()` 실패, `open()` 실패 등의 에러 처리를 포함하세요.
- 파이프의 사용하지 않는 fd는 반드시 닫아야 합니다 (그렇지 않으면 프로그램이 멈출 수 있습니다).
- 메모리 누수는 이번 과제에서 감점하지 않습니다.

---

## 참고 자료

- xv6 교재 Ch 1: Operating System Interfaces (특히 1.1, 1.2, 1.3절)
- `man fork`, `man exec`, `man wait`, `man pipe`, `man dup2`
- 실습 예제 코드: `../lab/examples/`
