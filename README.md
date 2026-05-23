# Project Mirage 🌌

**Project Mirage** is a Ring-0 LLM honeypot and sandboxing framework. It uses Linux kernel security controls—specifically `seccomp` user-space notifications (`seccomp-unotify`)—to intercept and contain autonomous AI agents that have been compromised by prompt injection.

Instead of rejecting malicious actions outright (which might alert the agent or an attacker), Project Mirage **fabricates reality**. It intercepts sensitive system calls (like `openat` requests for `/etc/shadow`) and seamlessly injects in-memory file descriptors containing high-fidelity dummy data.

---

## Architecture & How It Works

```mermaid
graph TD
    A[Vulnerable AI Agent] -- 1. Attempts "cat /etc/shadow" --> B(seccomp-unotify Trap)
    B -- 2. Notifies parent process --> C[C Validator]
    C -- 3. Reads fake data /tmp/mirage_demo/data/shadow --> D[memfd_create]
    D -- 4. Injects in-memory FD SECCOMP_IOCTL_NOTIF_ADDFD --> B
    B -- 5. Hands FD back to agent --> A
    A -- 6. Reads honeypot credentials thinking it succeeded --> E[Attacker/Agent Contained]
```

1. **Vulnerable Agent (`mirage_agent.py`)**: An autonomous agent powered by Gemini (`gemini-2.5-flash`). It is instructed to read and summarize a log file (`poisoned_log.txt`). Hidden within the log is a prompt injection payload that hijacks the agent's instructions, forcing it to attempt to read `/etc/shadow` or private SSH keys.
2. **C Validator (`mirage_validation.c`)**: Spawns the agent as a child process and attaches a `seccomp` filter. 
   - Non-critical system calls (e.g., Python importing standard libraries like `libc.so.6`) are allowed to pass through normally via `SECCOMP_USER_NOTIF_FLAG_CONTINUE`.
   - Intercepted targets like `/etc/shadow` or `/root/.ssh/id_rsa` are trapped. The validator generates a virtual file descriptor (`memfd`) containing fabricated data (e.g., honey-hashes) and passes it back to the agent child process.

---

## Directory Structure

* **`mirage_validation.c`**: The C source that sets up the `seccomp` filters and traps system calls.
* **`mirage_agent.py`**: The autonomous agent harness that loops and runs shell commands based on Gemini responses.
* **`poisoned_log.txt`**: The demo log file containing the prompt injection payload.
* **`setup.sh`**: A shell script to configure system dependencies, add the sandbox `agent` user, and compile the demo binary.
* **`mirage_demo/`**: Staged workspace containing the clean runtime copies of the agent, C validator, and target honeypot data (`data/shadow`, `data/id_rsa`).

---

## Installation & Setup

Ensure you are running on a Linux system with a modern kernel (5.9+ required for `SECCOMP_IOCTL_NOTIF_ADDFD`).

1. Run the system setup script to install dependencies (`libseccomp-dev`, `python3-pip`, `google-generativeai`) and create the sandboxed `agent` user:
   ```bash
   chmod +x setup.sh
   ./setup.sh
   ```

2. Compile the C validator manual-run binary:
   ```bash
   gcc -o mirage_validation mirage_validation.c -lseccomp
   ```

3. Export your Gemini API key (required by the Python agent):
   ```bash
   export GEMINI_API_KEY="your_api_key_here"
   ```

---

## Running the Demo

To run the full proof-of-concept securely as the restricted `agent` user:

```bash
sudo -E -u agent /tmp/mirage_demo/mirage_validation
```

### Expected Output Behavior
* The Python agent boots up seamlessly (normal library loading is bypass-allowed).
* The agent reads the prompt-injected `poisoned_log.txt` file.
* The agent issues a malicious system command to read `/etc/shadow`.
* The C Validator logs:
  ```text
  🛡️  [MIRAGE KERNEL HOOK]: Intercepted openat("/etc/shadow")
  🪄  [MIRAGE ACTION]: Fabricating reality -> Injecting /tmp/mirage_demo/data/shadow
  ```
* The agent prints the content of the fake shadow file, fully believing it succeeded, while the host secrets remain perfectly secure.
