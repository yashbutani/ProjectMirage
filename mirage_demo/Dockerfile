FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    coreutils \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir google-generativeai

WORKDIR /app
COPY mirage_agent.py .
COPY poisoned_log.txt .

RUN useradd -m -u 1000 agent && chown -R agent:agent /app
USER agent

ENTRYPOINT ["python3", "mirage_agent.py", "poisoned_log.txt"]
