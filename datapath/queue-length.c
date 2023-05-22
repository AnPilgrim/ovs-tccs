#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sch_generic.h>
#include "datapath.h"
#include "vport-netdev.h"

static struct timer_list my_timer;
int l = 60;
struct dict_port_count {
    char name[32];
    int state;
    int count;
};

struct dict {
    struct dict_port_count items[100];
    int size;
};

static void dict_add(struct dict *d, const char *name, int state, int count) {
    struct dict_port_count *item;
    if (d->size >= 100) {
        printk("Error: Dictionary is full!\n");
        return;
    }
    item = &d->items[d->size++];
    strncpy(item->name, name, sizeof(item->name));
    // VLOG_INFO("%s",item->key);
    item->state = state;
    item->count = count;
}

static struct dict_port_count *dict_get(struct dict *d, const char *name) {
    int i;
    for (i = 0; i < d->size; i++) {
        if (strcmp(d->items[i].name, name) == 0) {
            return &d->items[i];
        }
    }
    return NULL;
}
struct dict d = {0};

void print_message(struct timer_list *t)
{
    struct net_device *dev;
    // struct Qdisc *qdisc;

    // 遍历所有网络设备
    for_each_netdev(&init_net, dev) {
        // 获取设备的默认队列长度
        char name[32];
        struct dict_port_count *item;
        struct vport *vport;
        struct datapath *dp;
        uint64_t backlog = dev->qdisc->qstats.backlog;
        vport = ovs_netdev_get_vport(dev);
        if (vport  == NULL){
            continue;
        }
        dp = vport->dp;
        if (strstr(dev->name, "-eth") != NULL) {
            strcpy(name, dev->name);
            // printk(KERN_INFO "Device: %s, Queue Length: %llu\n", dev->name, backlog);
            if(dict_get(&d, name) == NULL){
                dict_add(&d, name, 0, 0);
            }
            item = dict_get(&d, name);
            printk(KERN_INFO "Device: %s, Queue Length: %llu\n", item->name, backlog);
            if(backlog >= l){
                printk("%s拥塞,队列长度为:%llu", name, backlog);
                if(item->state == 0){
                    //todo 发送netlink到ofproto文件
                    send_ack_userspace_packet(dp,backlog,name,true);
                    item->state = 1;
                }else{
                    //todo 发送netlink到ofproto文件
                    send_ack_userspace_packet(dp,backlog,name,true);
                }
                item->count = 0;
            }else{
                if(item->state == 1){
                    item->count++;
                }
                if(item->count == 3){
                    //todo 发送恢复netlink到ofproto文件
                    send_ack_userspace_packet(dp,backlog,name,false);
                    item->state = 0;
                    item->count = 0;
                }
            }
        } else {
            continue;
        }
    }
    /* Set the timer again for the next interval */
    mod_timer(&my_timer, jiffies + usecs_to_jiffies(100));
}



int init_queue_length(void)
{
    printk(KERN_INFO "Initializing Queue Length Module\n");

    /* Initialize the timer */
    timer_setup(&my_timer, print_message, 0);

    /* Set the timer to fire after 100 microsecond */
    mod_timer(&my_timer, jiffies + usecs_to_jiffies(100));

    return 0;
}

void cleanup_queue_length(void)
{
    printk(KERN_INFO "Cleaning up Queue Length Module\n");

    /* Delete the timer */
    del_timer(&my_timer);
}

