#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>

#define CHECK_DIR(c) (((c)->d_type == DT_DIR) && isdigit(*((c)->d_name)))

typedef struct proc_node {
    char name[256];
    struct proc_node *parent;
    struct proc_node *child;   // 指向第一个子进程
    struct proc_node *next;    // 指向下一个兄弟进程
    int pid;
    int ppid;
} proc_node;

static proc_node root_node = {
    .pid = 1, .ppid = 0, 
    .name = "systemd",
    .parent = NULL, .child = NULL, .next = NULL
};

const struct option table[] = {
    {"show-pids"        , no_argument, NULL, 'p'},
    {"numeric-sort"     , no_argument, NULL, 'n'},
    {"version"          , no_argument, NULL, 'V'},
};
static bool op_show_pids = false;
static bool op_numeric = false;

void parse_option(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        assert(argv[i]);
    }
    assert(!argv[argc]);

    int o;
    while ( (o = getopt_long(argc, argv, "-pnV", table, NULL)) != -1) {
        switch (o) {
        case 'p': op_show_pids = true; break;
        case 'n': op_numeric = true; break;
        case 'V': printf("own pstree implementation version\n"); break;
        default:
                  printf("Usage: %s [OPTION...]\n"
                         "[OPTION...]: [-p, --show-pids] [-n, --numeric-sort:] [-V, --version]\n",
                         argv[0]);
                  exit(0);
        }
    }
}

// learn from github, I have truoble in printing the whole tree
void printParentProcesses(proc_node* proc) {
    if (proc->parent) printParentProcesses(proc->parent);
    fprintf(stderr, "%s%*s",
           (proc == &root_node? "" : (proc->next ? " │ " : "   ")),
           (int) strlen(proc->name), "");
}

/**
 * Traverse the process tree.(preorder traversal)
 * if proc is root_node, print nothing,
 * else if proc is the first child, and if it has a sibling,     print─┬─, else───
 * else if proc is not the first child, and if it has a sibling, print ├─, else └─
 * notice the space
 *
 * The font is directly copied from the pstree displayed in the terminal.
 */
void printProcess(proc_node* proc) {
    fprintf(stderr, "%s%s%s",
           (proc == &root_node ? "" : (proc == proc->parent->child ? 
                                      (proc->next ? "─┬─" : "───") : 
                                      (proc->next ? " ├─" : " └─")
                                     )),
           proc->name,
           proc->child ? "" : "\n");

    // order same as find_process
    if (proc->child) printProcess(proc->child);
    if (proc->next) {
        if (proc->next->parent) printParentProcesses(proc->next->parent);
        printProcess(proc->next);
    }
}

proc_node *find_node(pid_t pid, proc_node *cur) {
    if (cur == NULL) cur = &root_node;

    // 1. End of recursion
    if (cur->pid == pid) return cur;

    // 2. Recursion
    // 2.1 search all child proc of the current node.
    proc_node *result = NULL;
    if (cur->child) {
        result = find_node(pid, cur->child);
        if (result) return result;
    }
    // Ohhhh
    // In fact, the recursion here already continuously iterates the child nodes,
    // so there is no need for while.

    // 2.2 search all sibling proc of the current node.
    if (cur->next) {
        result = find_node(pid, cur->next);
        if (result) return result;
    }

    return NULL; // Not found
}

proc_node* create_proc_node(int pid, int ppid, const char *name) {
    proc_node *existing_node = find_node(pid, NULL);
    if (existing_node) {
        printf("has added: %s\n", name);
        return NULL;
    }

    proc_node *node = malloc(sizeof(proc_node));
    assert(node != NULL);

    node->pid = pid;
    node->ppid = ppid;

    assert(strlen(name) < sizeof(node->name));
    strncpy(node->name, name, sizeof(node->name));
    node->name[sizeof(node->name) - 1] = '\0';
    if (op_show_pids) {
        char add_pid[16] = {0};
        sprintf(add_pid, "(%d)", node->pid);
        strcat(node->name, add_pid); // should enough
    }

    node->parent = NULL;
    node->child = NULL;
    node->next = NULL;

    printf("[create] name: %s  pid: %d  ppid: %d\n", node->name, node->pid, node->ppid);
    return node;
}

void add_proc_node(proc_node *proc) {
    if (proc == NULL) return;
    // 0. remove duplication
    proc_node *self = find_node(proc->pid, NULL);
    if (self) return;

    // 1. check proc if has parent
    //    if the proc's parent dead, proc should be orphan.
    //    we don't consider this.
    proc_node *parent = find_node(proc->ppid, NULL);
    if (parent) {
        proc->parent = parent;
        // 2. then parent if proc has child
        proc_node *child = parent->child;
        if (child == NULL) {
            parent->child = proc;
        } else {
            // Parent has child, so the proc should have sibling(next)
            // Find the last child in the list and add the new process there.
            if (op_numeric) {
                if (proc->pid < child->pid) {
                    proc->next = child;
                    parent->child = proc;
                } else {
                    while (child->next && proc->pid > child->next->pid) child = child->next;
                    proc->next = child->next;
                    child->next = proc;
                }
            } else {
                proc_node *last_child = child;
                while (last_child->next) {
                    last_child = last_child->next;
                }
                last_child->next = proc;
            }
        }
    }
}

/**
 * the flow same as find_node, 
 * remember not to free root_node
 */
void free_proc_tree(proc_node *node) {
    if (node == NULL || node == &root_node) return;
    
    if (node->child) {
        free_proc_tree(node->child);
        node->child = NULL;
    }

    if (node->next) {
        free_proc_tree(node->next);
        node->next = NULL;
    }

    if (node != &root_node) {
        printf("[free] name: %s  pid: %d  ppid: %d\n", node->name, node->pid, node->ppid);
        free(node);
        node = NULL;
    } else {
        node->child = NULL;
        node->next = NULL;
    }
}

proc_node *read_proc(const char *proc_dir, proc_node *parent) {
    char path[256] = {0};
    char name[256] = {0};
    char process_state = 'X';
    int pid = 0, ppid = 0;

    if (parent) {
        snprintf(path, sizeof(path), "/proc/%d/task/%.16s/stat", parent->pid, proc_dir);
    } else {
        snprintf(path, sizeof(path), "/proc/%.16s/stat", proc_dir);
    }
    //printf("path: %s\n", path);
    
    FILE *fp = fopen(path, "r");
    assert(fp != NULL);
    fscanf(fp, "%d (%255[^)]) %c %d", &pid, name, &process_state, &ppid);

    //printf("pid: %d  name: %s  process_stat: %c  ppid: %d\n",
    //       pid, name, process_state, ppid);

    proc_node *node = create_proc_node(pid, ppid, name);
    if (node == NULL) {
        fclose(fp);
        return NULL;
    }
    if (parent) {
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
            // main process or single process/thread
            proc_node *parent = read_proc(entry->d_name, NULL);
            if (parent == NULL) continue;
            
            // multi-thread
            char child_proc[128] = {0};
            snprintf(child_proc, sizeof(child_proc), "/proc/%.16s/task", entry->d_name);
            DIR *child_proc_dir = opendir(child_proc);
            if (child_proc_dir) {
                struct dirent *child_entry = NULL;
                while ((child_entry = readdir(child_proc_dir)) != NULL ) {
                    if (CHECK_DIR(child_entry)) 
                        read_proc(child_entry->d_name, parent);
                }
            }
            closedir(child_proc_dir);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    parse_option(argc, argv);

    read_proc_dir();

    //printProcess(&root_node);

    free_proc_tree(&root_node);

    return 0;
}

