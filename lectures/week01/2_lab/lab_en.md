---
theme: default
title: "Week 01 — Lab: Coding Agents"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 1 — Coding Agents

---

layout: section

---

# Lab Overview

---

# Lab Overview

- **Goal**: Get comfortable using AI-powered coding agents for real development tasks
- **Duration**: ~50 minutes
- **Submission**: None — this is an exploration lab
- **5 Tasks**:
  1. Install a coding agent
  2. File organization with an agent
  3. GitHub repo documentation
  4. RALPH technique (iterative refinement)
  5. Build a mini OS with an agent

---

# What Are Coding Agents?

- AI-powered CLI tools that understand and generate code in context
- Can read your file system, run commands, and make edits autonomously
- Operate in a **read → plan → act → verify** loop
- Useful for: scaffolding, refactoring, documentation, debugging

```
You: "Create a Makefile for a C project with debug and release targets"
Agent: reads directory → writes Makefile → confirms build succeeds
```

---

# Available Agents

| Agent | Cost | Notes |
|---|---|---|
| **Gemini CLI** | Free | Google's agent, good default choice |
| **Claude Code** | Paid | Strong at multi-file reasoning |
| **GPT Codex** | Paid | OpenAI's coding agent |
| **OpenCode** | Free / Open-source | Self-hostable alternative |

- Install at least one before lab starts
- Gemini CLI recommended if you have no preference

---

layout: section

---

# Tasks 1 & 2

---

# Task 1 & 2: Install & File Organization

**Task 1 — Install a Coding Agent**
- Follow the official install guide for your chosen agent
- Verify it runs: `gemini --version` (or equivalent)
- Authenticate with your API key or account

**Task 2 — File Organization**
- Point the agent at a messy directory of files
- Ask it to propose and apply a logical folder structure
- Example prompt:
  ```
  "Organize the files in ./downloads into subfolders by type and date"
  ```
- Review the changes — did the agent make sensible decisions?

---

layout: section

---

# Tasks 3 & 4

---

# Task 3 & 4: Repo Docs & RALPH Technique

**Task 3 — GitHub Repo Documentation**
- Open an existing GitHub repository (yours or a public one)
- Ask the agent to generate a `README.md` from the source code
- Check: does the README accurately describe the project?

**Task 4 — RALPH Technique (Iterative Refinement)**

RALPH = **R**equest → **A**nalyze → **L**ist issues → **P**rompt again → **H**armonize

```
1. Make an initial request
2. Critically review the agent's output
3. List specific problems or gaps
4. Re-prompt with targeted corrections
5. Repeat until the result is satisfactory
```

- Practice this loop on the README from Task 3

---

layout: section

---

# Task 5

---

# Task 5: Build a Mini OS

- Ask your coding agent to scaffold a minimal OS kernel in C
- Suggested prompt:
  ```
  "Create a minimal x86 OS kernel in C that boots with GRUB,
  prints 'Hello, OS!' to the screen, and halts. Include a
  Makefile and linker script."
  ```
- Observe how the agent:
  - Breaks the problem into files (`boot.asm`, `kernel.c`, `linker.ld`)
  - Explains each piece
  - Handles errors if you give feedback
- You do **not** need to successfully boot it — the process matters

---

# Summary & Next Steps

**What we practiced today**
- Installing and authenticating a coding agent
- Delegating file and documentation tasks to an AI tool
- Applying the RALPH iterative refinement loop
- Using an agent to tackle a non-trivial systems project

**Coming up — Week 2 Lab**
- Process system calls: `fork()`, `exec()`, `wait()`, `pipe()`
- You will write and run C programs directly on Linux

> Coding agents are tools — understanding what they produce is still your responsibility.
