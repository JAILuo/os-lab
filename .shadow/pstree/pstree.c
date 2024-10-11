#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CHECK_DIR(c) (((c)->d_type == DT_DIR) && isdigit(*((c)->d_name)))

typedef struct proc_node {
    int pid;
    int ppid;
    char name[256];
    char process_state;
    struct proc_node *parent;
    struct proc_node *child;   // 指向第一个子进程
    struct proc_node *next;    // 指向下一个兄弟进程
} proc_node;

proc_node root_node = {
    .pid = 1, .ppid = 0, 
    .name = "systemd", .process_state = 'X',
    .parent = NULL, .child = NULL, .next = NULL
};

void printParentProcesses(proc_node* proc) {
    /* Print the vertical lines of parent processes */
    if (proc->parent) printParentProcesses(proc->parent);
    printf("%s%*s",
           (proc == &root_node? "" : (proc->next ? " | " : "   ")),
           (int) strlen(proc->name), "");
}

void printProcess(proc_node* proc) {
    //printf("proc->pid: %d  name: %s\n", proc->pid, proc->name);
    printf("%s%s%s",
    (proc == &root_node? "" : (proc == proc->parent->child ? (proc->next ? "-+-" : "---") : (proc->next ? " |-" : " `-"))),
    proc->name,
    proc->child ? "" : "\n");

    // order same as find_process
    if (proc->child) printProcess(proc->child);
    if (proc->next) {
        if (proc->next->parent) printParentProcesses(proc->next->parent);
        printProcess(proc->next);
    }
}

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

proc_node *find_node(pid_t pid, proc_node *cur) {
    if (cur == NULL) cur = &root_node;
    // 1. End of recursion 
    if (cur->pid == pid) return cur;

    // 2. Recursion 
    // 2.1 search all child proc of the current node.
    proc_node *next_child = cur->child;
    while (next_child) {
        proc_node *result = find_node(pid, next_child);
        if (result) return result;
        next_child = next_child->child;
    }

    // 2.2 search all sibling proc of the current node.
    proc_node *next_sibling = cur->next;
    while (next_sibling) {
        proc_node *result = find_node(pid, next_sibling);
        if (result) return result;
        next_sibling = next_sibling->next;
    }

    return NULL;
}

void add_proc_node(proc_node *proc) {
    // 0. remove duplication
    proc_node *self = find_node(proc->pid, NULL);
    if (self) return;

    // 1. check proc if has parent
    //    if the proc's parent dead, proc should be orphan.
    //    we don't consider this.
    proc_node *parent = find_node(proc->ppid, NULL);
    if (parent) {
        printf("test.....\n");
        proc->parent = parent;

        // 2. then parent if proc has child
        proc_node *child = parent->child;
        if (child == NULL) {
            parent->child = proc;
        } else {
            // Parent has child, so the proc should have sibling(next)
            // Find the last child in the list and add the new process there.
            // proc_node *last_child = child;
            // while (last_child->next) {
            //     last_child = last_child->next;
            // }
            // last_child->next = proc;
              proc->next = child;
              parent->child = proc;
        }
    }
}


proc_node *read_proc(const char *proc_dir, proc_node *parent) {
    char path[256] = {0};

    if (parent) {
        snprintf(path, sizeof(path), "/proc/%d/task/%.16s/stat", parent->pid, proc_dir);
    } else {
        snprintf(path, sizeof(path), "/proc/%.16s/stat", proc_dir);
    }
    //printf("path: %s\n", path);
    
    FILE *fp = fopen(path, "r");
    assert(fp != NULL);

    int pid, ppid;
    char name[256] = {0};
    char process_state;
    fscanf(fp, "%d (%255[^)]) %c %d", &pid, name, &process_state, &ppid);

    //printf("pid: %d  name: %s  process_stat: %c  ppid: %d\n",
    //       pid, name, process_state, ppid);

    proc_node *node = create_proc_node(pid, ppid, name);
    if (parent) {
        //printf("parent->pid: %d\n", parent->pid);
        node->ppid = parent->pid;
        snprintf(node->name, sizeof(node->name), "%s", parent->name);
    }
    add_proc_node(node);

    fclose(fp);
    return node;
}

void read_proc_dir() {
    DIR *dir = NULL;  

    dir = opendir("/proc");
    if (dir == NULL) {
        perror("Failed to open /proc directory");
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (CHECK_DIR(entry)) {
            proc_node *parent = read_proc(entry->d_name, NULL);
            if (parent == NULL) continue;
            
            char child_proc[128] = {0};
            snprintf(child_proc, sizeof(child_proc), "/proc/%.16s/task", entry->d_name);
            DIR *child_proc_dir = opendir(child_proc);
            if (child_proc_dir) {
                struct dirent *child_entry = NULL;
                while ((child_entry = readdir(child_proc_dir)) != NULL ) {
                    if (CHECK_DIR(child_entry)) read_proc(child_entry->d_name, parent);
                }
            }
            closedir(child_proc_dir);
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

    printProcess(&root_node);

    return 0;
}

