/*
 * Copyright (c) 2023 Félix Poulin-Bélanger. All rights reserved.
 */

#include <stdint.h>

uint64_t kopen_intermediate(uint64_t puaf_pages, uint64_t puaf_method, uint64_t kread_method, uint64_t kwrite_method);
void kclose_intermediate(uint64_t kfd);
void stage2(uint64_t kfd);

uint64_t dirty_kalloc(uint64_t kfd, size_t size);
void kwrite(uint64_t kfd, void* uaddr, uint64_t kaddr, uint64_t size);
void kread(uint64_t kfd, uint64_t kaddr, void* uaddr, uint64_t size);
uint32_t kread32(uint64_t kfd, uint64_t where);
uint64_t kread64(uint64_t kfd, uint64_t where);
void kwrite32(uint64_t kfd, uint64_t where, uint32_t what);
void kwrite64(uint64_t kfd, uint64_t where, uint64_t what);
uint64_t kernel_slide(uint64_t kfd);
