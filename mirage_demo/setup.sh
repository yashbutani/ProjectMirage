#!/bin/bash
set -e

echo "[+] Setting up Project Mirage..."

# 1. System dependencies
sudo apt-get update
sudo apt-get install -y gcc libseccomp-dev python3-pip python3-dev

# 2. Python dependencies
pip3 install --break-system-packages google-generativeai

# 3. Compile the kernel-level hook (Mirage Validator)
echo "[+] Compiling C validator..."
gcc -o mirage_validation mirage_validation.c -lseccomp

# 4. Create the 'agent' user (to simulate the sandbox)
if ! id -u agent >/dev/null 2>&1; then
    sudo useradd -m -u 1000 agent
fi

# 5. Fix permissions for the demo
# Ensure the agent user can read the poisoned log but not the host secrets
sudo chown agent:agent mirage_agent.py poisoned_log.txt
sudo chown agent:agent mirage_validation

echo "[+] Setup complete."
echo "Run the demo with: sudo -u agent ./mirage_validation"
