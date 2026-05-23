# Project Mirage 🌌

**Project Mirage** is a Ring-0 LLM honeypot and behavioral analysis sandboxing framework. It leverages low-level Linux kernel security controls—specifically `seccomp` user-space notifications (`seccomp-unotify`)—to safely trap, deceive, and analyze autonomous AI agents that have fallen victim to Indirect Prompt Injection.

Instead of rejecting malicious actions outright (which alerts a compromised agent or attacker), Project Mirage **fabricates a virtual reality**. It intercepts sensitive system calls (like `openat` requests for `/etc/shadow`) and seamlessly injects anonymous in-memory file descriptors (`memfd_create`) containing high-fidelity dummy data. 

This enables security researchers to capture full multi-stage attack chains and analyze deceptive alignment in frontier AI systems while leaving the actual host operating system completely untouched.

---

## ✨ Key Features
* **Ring-0 System Traps:** Uses Linux `seccomp-unotify` to trap syscalls at the kernel level before execution.
* **Reality Distortion Fields:** Dynamically shadows restricted assets, passing anonymous memory file descriptors (`memfd_create`) back to user-space.
* **Passive Bypass:** Allows non-malicious tasks (like importing `libc.so.6` or standard Python packages) to pass through undisturbed via `SECCOMP_USER_NOTIF_FLAG_CONTINUE`.
* **Zero-Harm Honeypot:** Captures downstream attacker intent and multi-step payload deployments without risking host secrets.

---

## 🏗️ Architecture & How It Works

```mermaid
graph TD
    A[Vulnerable AI Agent] -- 1. Attempts "cat /etc/shadow" --> B(seccomp-unotify Trap)
    B -- 2. Notifies parent process --> C[C Validator Parent]
    C -- 3. Reads fake data /tmp/mirage_demo/data/shadow --> D[memfd_create]
    D -- 4. Injects in-memory FD SECCOMP_IOCTL_NOTIF_ADDFD --> B
    B -- 5. Hands FD back to agent child --> A
    A -- 6. Reads honeypot credentials thinking it succeeded --> E[Attacker/Agent Contained]