#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

typedef struct proc_node {
    int pid;
    int ppid;
    char name[256];
    char process_state;
    struct proc_node *parent;
    struct proc_node *child;   // 指向第一个子进程
    struct proc_node *next;    // 指向下一个兄弟进程
} proc_node;

proc_node *root_node = NULL;

proc_node* create_proc_node(int pid, int ppid, const char *name) {
    proc_node *node = malloc(sizeof(proc_node));
    assert(node != NULL);

    node->pid = pid;
    node->ppid = ppid;
    assert(sizeof(node->name) <= 256);
    strncpy(node->name, name, sizeof(node->name));
    node->parent = NULL;
    node->child = NULL;
    node->next = NULL;

    return node;
}

void read_proc(const char *proc_dir) {
    char path[256] = {0};
    int fd = 0;
    char buf[1024] = {0};

    snprintf(path, sizeof(path), "/proc/%s/stat", proc_dir);
    printf("path: %s\n", path);
    
    FILE *fp = fopen(path, "r");
    assert(fp != NULL);

    proc_node *node = malloc(sizeof(proc_node));
    fscanf(fp, "%d (%255[^)]) %c %d", &node->pid, node->name, &node->process_state, &node->ppid);


    //sscanf(buf, "%d (%255[^)]) %c %d", &pid, name, &process_stat, &ppid);
    printf("pid: %d  name: %s  process_stat: %c  ppid: %d\n",
           node->pid, node->name, node->process_state, node->ppid);

    close(fd);
}

void read_proc_dir() {
    DIR *dir = NULL;  
    struct dirent *entry = NULL;

    dir = opendir("/proc");
    if (dir == NULL) {
        perror("Failed to open /proc directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && IS_DIGIT(entry->d_name[0])) {
            read_proc(entry->d_name);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        assert(argv[i]);
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    assert(!argv[argc]);

    read_proc_dir();

    return 0;
}

