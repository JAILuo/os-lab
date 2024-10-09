#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct proc_node {
    int pid;
    int ppid;
    char name[256];
    struct proc_node *child;   // 指向第一个子进程
    struct proc_node *next;    // 指向下一个兄弟进程
} proc_node;

proc_node *root_node = NULL;

proc_node* create_proc_node(int pid, int ppid, const char *name) {
    proc_node *node = malloc(sizeof(proc_node));
    if (node) {
        node->pid = pid;
        node->ppid = ppid;
        strncpy(node->name, name, sizeof(node->name));
        node->child = NULL;
        node->next = NULL;
    }
    return node;
}

proc_node *find_proc_node(proc_node *root, int pid) {
    if (root == NULL) return NULL;
    if (root->pid == pid) return root;

    proc_node *child = root->child;
    while (child) {
        proc_node *result = find_proc_node(child, pid);
        if (result) return result;
        
        child = child->next;
    }
    return find_proc_node(root->next, pid);
}

void add_proc_node(proc_node *root, proc_node *node) {
    if (root == NULL) {
        // 如果树是空的，新节点成为根节点
        root = node;
    } else {
        // 查找父进程节点
        proc_node *current = root;
        while (current) {
            if (current->pid == node->ppid) {
                // 找到父进程，将新节点添加为子节点
                node->next = current->child;
                current->child = node;
                return;
            }
            current = current->next;
        }
        // 如果没有找到父进程，将新节点添加为根节点的兄弟节点
        current = root;
        while (current->next) {
            current = current->next;
        }
        current->next = node;
    }
}

void read_proc(const char *proc_dir) {
    char path[256] = {0};
    int fd = 0;
    ssize_t bytes_read = 0;
    char buf[1024] = {0};

    snprintf(path, sizeof(path), "/proc/%s/stat", proc_dir);

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open file");
        return;
    }
    
    bytes_read = read(fd, buf, sizeof(buf));
    if (bytes_read == -1) {
        perror("Failed to read file");
    } else {
        assert(bytes_read <= 1024);
        buf[bytes_read] = '\0';
        int pid, ppid;
        char process_stat;
        char name[256];
        sscanf(buf, "%d (%255[^)]) %c %d", &pid, name, &process_stat, &ppid);
        //printf("pid: %d  name: %s  process_stat: %c  ppid: %d\n",
        //       pid, name, process_stat, ppid);

        proc_node *new_node = create_proc_node(pid, ppid, name);

        if (root_node== NULL) {
            root_node = new_node;
        } else {
            add_proc_node(root_node, new_node);
        }
        //printf("Process path: %s:\nbuf: %s\n", path, buf);
    }
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
        if (entry->d_type == DT_DIR) {
            // use number to judge process, ok?
            if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
                //printf("PID: %s\n", entry->d_name);
                read_proc(entry->d_name);
            }
        }
    }
    closedir(dir);
}

void free_proc_tree(proc_node *node) {
    if (node == NULL) return;
    free_proc_tree(node->child);
    free_proc_tree(node->next);
    free(node);
}

void print_proc_tree(proc_node *node, int level) {
    if (node == NULL) return;
    for (int i = 0; i < level; i++) {
        printf("  "); // 缩进表示层级
    }
    printf("%s (PID: %d, PPID: %d)\n", node->name, node->pid, node->ppid);
    print_proc_tree(node->child, level + 1);  // 递归打印子节点
    print_proc_tree(node->next, level);       // 打印兄弟节点
}

int main(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        assert(argv[i]);
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    assert(!argv[argc]);

    read_proc_dir();
    print_proc_tree(root_node, 0);
    free_proc_tree(root_node);

    return 0;
}

