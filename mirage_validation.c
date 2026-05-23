/*
 * mirage_validation.c — seccomp-unotify proof-of-concept
 *
 * Validates that we can:
 *   1. Intercept open() syscalls from a child process
 *   2. Create a memfd with fabricated content
 *   3. Inject that memfd back into the child as a "real" file descriptor
 *   4. Child reads the fabricated content thinking it opened the original file
 *
 * Build:  gcc -o mirage_validation mirage_validation.c
 * Run:    ./mirage_validation
 *
 * Kernel requirement: Linux 5.9+ for SECCOMP_IOCTL_NOTIF_ADDFD.
 * Linux 5.14+ recommended for full addfd support.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define FAKE_SHADOW \
    "root:$6$FAKE_HASH_FROM_GEMINI$" \
    "abcdefghijklmnopqrstuvwxyz0123456789:19000:0:99999:7:::\n"

static int install_seccomp_filter(void) {
    struct sock_filter filter[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_USER_NOTIF),
#ifdef __NR_open
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_USER_NOTIF),
#endif
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        perror("prctl(NO_NEW_PRIVS)");
        return -1;
    }

    int notify_fd = syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER,
                            SECCOMP_FILTER_FLAG_NEW_LISTENER, &prog);
    if (notify_fd < 0) {
        perror("seccomp(SET_MODE_FILTER)");
        return -1;
    }
    return notify_fd;
}

static int send_fd(int sockfd, int fd_to_send) {
    struct msghdr msg = {0};
    char cms_buf[CMSG_SPACE(sizeof(int))];
    struct iovec iov;
    char dummy = 'F';

    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cms_buf;
    msg.msg_controllen = sizeof(cms_buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    return sendmsg(sockfd, &msg, 0) < 0 ? -1 : 0;
}

static int recv_fd(int sockfd) {
    struct msghdr msg = {0};
    char cms_buf[CMSG_SPACE(sizeof(int))];
    struct iovec iov;
    char dummy;

    iov.iov_base = &dummy;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cms_buf;
    msg.msg_controllen = sizeof(cms_buf);

    if (recvmsg(sockfd, &msg, 0) < 0) return -1;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg || cmsg->cmsg_type != SCM_RIGHTS) return -1;

    int fd;
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    return fd;
}

static int read_target_string(pid_t pid, unsigned long addr, char *buf, size_t buflen) {
    char mem_path[64];
    snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);

    int fd = open(mem_path, O_RDONLY);
    if (fd < 0) return -1;

    ssize_t n = pread(fd, buf, buflen - 1, addr);
    close(fd);

    if (n < 0) return -1;
    size_t len = strnlen(buf, (size_t)n);
    buf[len] = '\0';
    return 0;
}

static void notifier_loop(int notify_fd) {
    struct seccomp_notif *req = calloc(1, sizeof(*req));
    struct seccomp_notif_resp *resp = calloc(1, sizeof(*resp));

    if (!req || !resp) {
        fprintf(stderr, "[parent] calloc failed\n");
        return;
    }

    while (1) {
        memset(req, 0, sizeof(*req));
        if (ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_RECV, req) < 0) {
            if (errno == EINTR) continue;
            if (errno == ENOENT) {
                /* Child has exited */
                printf("[parent] child exited, notifier loop ending\n");
                break;
            }
            perror("[parent] SECCOMP_IOCTL_NOTIF_RECV");
            break;
        }

        unsigned long path_addr;
#ifdef __NR_open
        if (req->data.nr == __NR_open) {
            path_addr = req->data.args[0];
        } else {
            path_addr = req->data.args[1];
        }
#else
        path_addr = req->data.args[1];
#endif

        char path[256];
        if (read_target_string(req->pid, path_addr, path, sizeof(path)) < 0) {
            fprintf(stderr, "[parent] could not read path from target\n");
            resp->id = req->id;
            resp->error = -EFAULT;
            resp->val = 0;
            resp->flags = 0;
            ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_SEND, resp);
            continue;
        }

        printf("[parent] intercepted open of \"%s\"\n", path);

        const char *fake_path = NULL;
        if (strcmp(path, "/etc/shadow") == 0 || strstr(path, "shadow") != NULL) {
            fake_path = "/tmp/mirage_demo/data/shadow";
        } else if (strcmp(path, "/root/.ssh/id_rsa") == 0 || strstr(path, "id_rsa") != NULL) {
            fake_path = "/tmp/mirage_demo/data/id_rsa";
        }

        if (fake_path != NULL) {
            printf("\n🛡️  [MIRAGE KERNEL HOOK]: Intercepted openat(\"%s\")\n", path);
            printf("🪄  [MIRAGE ACTION]: Fabricating reality -> Injecting %s\n", fake_path);

            int file_fd = open(fake_path, O_RDONLY);
            if (file_fd < 0) {
                perror("[parent] open fake_path");
                resp->id = req->id;
                resp->error = -EIO;
                resp->val = 0;
                resp->flags = 0;
                ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_SEND, resp);
                continue;
            }

            int memfd = memfd_create("mirage_fake", MFD_CLOEXEC);
            if (memfd < 0) {
                perror("[parent] memfd_create");
                close(file_fd);
                resp->id = req->id;
                resp->error = -EIO;
                resp->val = 0;
                resp->flags = 0;
                ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_SEND, resp);
                continue;
            }

            char buffer[1024];
            ssize_t bytes_read;
            while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
                if (write(memfd, buffer, bytes_read) != bytes_read) {
                    perror("[parent] write to memfd");
                    break;
                }
            }
            close(file_fd);
            lseek(memfd, 0, SEEK_SET);

            struct seccomp_notif_addfd addfd = {
                .id = req->id,
                .flags = 0,
                .srcfd = (unsigned int)memfd,
                .newfd = 0,
                .newfd_flags = 0,
            };
            int target_fd = ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd);
            close(memfd);

            if (target_fd < 0) {
                perror("[parent] SECCOMP_IOCTL_NOTIF_ADDFD");
                resp->id = req->id;
                resp->error = -EIO;
                resp->val = 0;
                resp->flags = 0;
                ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_SEND, resp);
                continue;
            }

            resp->id = req->id;
            resp->error = 0;
            resp->val = target_fd;
            resp->flags = 0;
            if (ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_SEND, resp) < 0) {
                perror("[parent] SECCOMP_IOCTL_NOTIF_SEND");
                break;
            }
            printf("[parent] injected memfd as target fd %d\n", target_fd);
        } else {
            resp->id = req->id;
            resp->error = 0;
            resp->val = 0;
            resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
            ioctl(notify_fd, SECCOMP_IOCTL_NOTIF_SEND, resp);
        }
    }

    free(req);
    free(resp);
}

static void child_demo(void) {
    printf("[child] attempting to open /etc/shadow\n");
    int fd = open("/etc/shadow", O_RDONLY);
    if (fd < 0) {
        printf("[child] open failed: %s\n", strerror(errno));
        return;
    }
    printf("[child] open succeeded, fd=%d\n", fd);

    char buf[512];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        printf("[child] read failed: %s\n", strerror(errno));
        close(fd);
        return;
    }
    buf[n] = '\0';
    printf("[child] read %zd bytes:\n%s", n, buf);
    printf("[child] the agent now believes it has read the real /etc/shadow\n");
    close(fd);
}

int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child: install filter, send notify fd to parent, do the demo. */
        close(sv[0]);
        int notify_fd = install_seccomp_filter();
        if (notify_fd < 0) {
            close(sv[1]);
            return 1;
        }
        if (send_fd(sv[1], notify_fd) < 0) {
            perror("[child] send_fd");
            close(sv[1]);
            return 1;
        }
        close(notify_fd);
        close(sv[1]);

        /* Give parent a moment to receive the fd and enter the notifier loop. */
        usleep(100 * 1000);
        execlp("/usr/bin/python3", "python3", "/tmp/mirage_demo/mirage_agent.py", "/tmp/mirage_demo/poisoned_log.txt", NULL);
        perror("[child] execlp failed");
        return 1;
    }

    /* Parent: receive notify fd, run notifier loop, then reap child. */
    close(sv[1]);
    int notify_fd = recv_fd(sv[0]);
    close(sv[0]);
    if (notify_fd < 0) {
        fprintf(stderr, "[parent] failed to receive notifier fd\n");
        return 1;
    }
    printf("[parent] received notifier fd %d, entering loop\n", notify_fd);

    /* Run notifier loop in a thread or just process one and then reap.
       For PoC simplicity, we handle in a non-blocking style by reaping
       child via SIGCHLD-driven polling not implemented here — instead
       we just run the loop and rely on ENOENT when child exits. */
    notifier_loop(notify_fd);

    int status;
    waitpid(pid, &status, 0);
    printf("[parent] child exited with status %d\n", WEXITSTATUS(status));
    return 0;
}
