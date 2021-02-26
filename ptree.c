#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched/task.h>
#include <linux/prinfo.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#define PRINFO_COMM_LEN 64

SYSCALL_DEFINE2(ptree, struct prinfo*, buf, int*, nr){
    // Check if buf or nr is NULL
    if(buf==NULL||nr==NULL) return -EINVAL;
    if(!access_ok(nr, sizeof(int))) return -EFAULT;

    // Define local variables
    int num_of_entries = 0;
    int num_read;
    copy_from_user(&num_read, nr, sizeof(int));
    if(num_read < 1) return -EINVAL;
    if(!access_ok(buf, sizeof(struct prinfo)*num_read)) return -EFAULT;
    
    struct task_struct *cur = get_current();
    struct task_struct *rt = cur;

    struct prinfo* tbuf = (struct prinfo*)kmalloc(sizeof(struct prinfo)*num_read, GFP_KERNEL);
    if(tbuf==NULL) {
        printk("ptree: kmalloc() error\n");
        return -1;
    }

    struct prinfo tpr;

    // Find root process
    for(;rt->parent!=rt;rt=rt->parent)
        printk("ptree: %s %d\n",rt->comm, rt->pid);
    printk("ptree: Found root: %s %d\n", rt->comm, rt->pid);

    // Fill prinfo struct var. with task struct
    void fill_prinfo(struct prinfo *to, struct task_struct *from, int depth){
        struct task_struct *tmp;
        
        to->state = from->state;
        to->pid = from->pid;
        to->parent_pid = from->real_parent->pid;
        if(list_empty(&from->children)) to->first_child_pid = 0;
        else{
            tmp = list_entry(from->children.next, struct task_struct, sibling);
            to->first_child_pid = tmp->pid;
        }
        // if(list_empty(from->sibling)) to->next_sibling_pid = 0;
        // else{
        //     // how can we check this is last entry of siblings?
        // }
        // To get uid, we need to use struct task_struct->cred->uid.
        to->uid = from->cred->uid.val;
        memset(to->comm, 0, PRINFO_COMM_LEN);
        memset(to->comm, ' ', depth*2<PRINFO_COMM_LEN?depth*2:0);
        memcpy(to->comm + depth*2, from->comm, 
            TASK_COMM_LEN<PRINFO_COMM_LEN-depth*2?TASK_COMM_LEN:PRINFO_COMM_LEN-depth*2);
    }

    // Traverse all task struct from root
    void traverse_process(struct task_struct *root, int depth){
        if(num_read<=num_of_entries) return;
        struct task_struct *entry;
        struct prinfo tmplt = {0,};
        struct list_head *ptr = &(root->children);
        // printk("ptree: traversing %s\n",root->comm);
        if(list_empty(ptr)) return;

        // Iterate all entries of list & save it to the buffer
        list_for_each(ptr, &root->children){
            if(num_read<=num_of_entries) return;
            entry = list_entry(ptr, struct task_struct, sibling);
            fill_prinfo(&tmplt, entry, depth);
            // Cannot check if there're no sibling inside fill_prinfo,
            // so we check if this entry is last sibling
            if(ptr->next != &root->children) 
                tmplt.next_sibling_pid = list_entry(ptr->next, struct task_struct, sibling)->pid;
            else
                tmplt.next_sibling_pid = 0;
            
            memcpy(tbuf+num_of_entries++, &tmplt, sizeof(struct prinfo));
            // printk("ptree: recorded prinfo to buffer. num_of_entries: %d\n",num_of_entries);
            traverse_process(entry, depth+1);
        }
    }

    fill_prinfo(&tpr, rt, 0);
    tpr.next_sibling_pid = 0;
    memcpy(tbuf+num_of_entries++, &tpr, sizeof(struct prinfo));
    // printk("ptree: traverse start\n");
    traverse_process(rt, 1);
    printk("ptree: traverse complete. num_of_entries=%d\n", num_of_entries);

    // Copy entries & update nr of user space
    if(num_read!=num_of_entries) copy_to_user(nr, &num_of_entries, sizeof(int));
    copy_to_user(buf, tbuf, num_of_entries * sizeof(struct prinfo));
    
    // free temp. bufer space & return
    kfree(tbuf);
    return num_of_entries;
}
