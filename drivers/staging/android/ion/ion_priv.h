/*
 * drivers/staging/android/ion/ion_priv.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ION_PRIV_H
#define _ION_PRIV_H

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/rbtree_latch.h>
#include <linux/seq_file.h>

#include "msm_ion_priv.h"
#include <linux/sched.h>
#include <linux/shrinker.h>
#include <linux/types.h>
#ifdef CONFIG_ION_POOL_CACHE_POLICY
#include <asm/cacheflush.h>
#endif
#include <linux/device.h>

#include "ion.h"

/**
 * struct mem_map_data - represents information about the memory map for a heap
 * @node:		list node used to store in the list of mem_map_data
 * @addr:		start address of memory region.
 * @addr:		end address of memory region.
 * @size:		size of memory region
 * @client_name:		name of the client who owns this buffer.
 *
 */
struct mem_map_data {
	struct list_head node;
	ion_phys_addr_t addr;
	ion_phys_addr_t addr_end;
	unsigned long size;
	const char *client_name;
};

struct ion_handle {
	struct ion_buffer *buffer;
	struct ion_client *client;
	struct latch_tree_node rnode;
	struct rcu_head rcu;
	atomic_t refcount;
	int id;
};

/**
 * struct ion_heap_ops - ops to operate on a given heap
 * @allocate:		allocate memory
 * @free:		free memory. Will be called with
 *			ION_PRIV_FLAG_SHRINKER_FREE set in buffer flags when
 *			called from a shrinker. In that case, the pages being
 *			free'd must be truly free'd back to the system, not put
 *			in a page pool or otherwise cached.
 * @phys		get physical address of a buffer (only define on
 *			physically contiguous heaps)
 * @map_dma		map the memory for dma to a scatterlist
 * @unmap_dma		unmap the memory for dma
 * @map_kernel		map memory to the kernel
 * @unmap_kernel	unmap memory to the kernel
 * @map_user		map memory to userspace
 * @unmap_user		unmap memory to userspace
 *
 * allocate, phys, and map_user return 0 on success, -errno on error.
 * map_dma and map_kernel return pointer on success, ERR_PTR on
 * error. @free will be called with ION_PRIV_FLAG_SHRINKER_FREE set in
 * the buffer's private_flags when called from a shrinker. In that
 * case, the pages being free'd must be truly free'd back to the
 * system, not put in a page pool or otherwise cached.
 */
struct ion_heap_ops {
	int (*allocate)(struct ion_heap *heap,
			struct ion_buffer *buffer, unsigned long len,
			unsigned long align, unsigned long flags);
	void (*free)(struct ion_buffer *buffer);
	int (*phys)(struct ion_heap *heap, struct ion_buffer *buffer,
		    ion_phys_addr_t *addr, size_t *len);
	struct sg_table * (*map_dma)(struct ion_heap *heap,
				     struct ion_buffer *buffer);
	void (*unmap_dma)(struct ion_heap *heap, struct ion_buffer *buffer);
	void * (*map_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);
	void (*unmap_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);
	int (*map_user)(struct ion_heap *mapper, struct ion_buffer *buffer,
			struct vm_area_struct *vma);
	int (*shrink)(struct ion_heap *heap, gfp_t gfp_mask, int nr_to_scan);
	void (*unmap_user)(struct ion_heap *mapper, struct ion_buffer *buffer);
	int (*print_debug)(struct ion_heap *heap, struct seq_file *s,
			   const struct list_head *mem_map);
};

/**
 * heap flags - flags between the heaps and core ion code
 */
#define ION_HEAP_FLAG_DEFER_FREE (1 << 0)

/**
 * private flags - flags internal to ion
 */
/*
 * Buffer is being freed from a shrinker function. Skip any possible
 * heap-specific caching mechanism (e.g. page pools). Guarantees that
 * any buffer storage that came from the system allocator will be
 * returned to the system allocator.
 */
#define ION_PRIV_FLAG_SHRINKER_FREE (1 << 0)

/**
 * struct ion_heap - represents a heap in the system
 * @node:		rb node to put the heap on the device's tree of heaps
 * @dev:		back pointer to the ion_device
 * @type:		type of heap
 * @ops:		ops struct as above
 * @flags:		flags
 * @id:			id of heap, also indicates priority of this heap when
 *			allocating.  These are specified by platform data and
 *			MUST be unique
 * @name:		used for debugging
 * @shrinker:		a shrinker for the heap
 * @priv:		private heap data
 * @free_list:		free list head if deferred free is used
 * @free_list_size	size of the deferred free list in bytes
 * @lock:		protects the free list
 * @waitqueue:		queue to wait on from deferred free thread
 * @task:		task struct of deferred free thread
 * @debug_show:		called when heap debug file is read to add any
 *			heap specific debug info to output
 *
 * Represents a pool of memory from which buffers can be made.  In some
 * systems the only heap is regular system memory allocated via vmalloc.
 * On others, some blocks might require large physically contiguous buffers
 * that are allocated from a specially reserved heap.
 */
struct ion_heap {
	struct plist_node node;
	enum ion_heap_type type;
	struct ion_heap_ops *ops;
	unsigned long flags;
	unsigned int id;
	const char *name;
	struct shrinker shrinker;
	void *priv;
	struct workqueue_struct *wq;
};

/**
 * ion_device_create - allocates and returns an ion device
 * @custom_ioctl:	arch specific ioctl function if applicable
 *
 * returns a valid device or -PTR_ERR
 */
struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg));

/**
 * ion_device_destroy - free and device and it's resource
 * @dev:		the device
 */
void ion_device_destroy(struct ion_device *dev);

/**
 * ion_device_add_heap - adds a heap to the ion device
 * @dev:		the device
 * @heap:		the heap to add
 */
void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap);

struct pages_mem {
	struct page **pages;
	u32 size;
	void (*free_fn)(const void *);
};

/**
 * some helpers for common operations on buffers using the sg_table
 * and vaddr fields
 */
void *ion_heap_map_kernel(struct ion_heap *, struct ion_buffer *);
void ion_heap_unmap_kernel(struct ion_heap *, struct ion_buffer *);
int ion_heap_map_user(struct ion_heap *, struct ion_buffer *,
			struct vm_area_struct *);
int ion_heap_buffer_zero(struct ion_buffer *buffer);
int ion_heap_pages_zero(struct page *page, size_t size, pgprot_t pgprot);

int msm_ion_heap_high_order_page_zero(struct device *dev, struct page *page,
				      int order);
struct ion_heap *get_ion_heap(int heap_id);
int msm_ion_heap_sg_table_zero(struct device *dev, struct sg_table *sg,
			       size_t size);
int msm_ion_heap_pages_zero(struct page **pages, int num_pages);
int msm_ion_heap_alloc_pages_mem(struct pages_mem *pages_mem);
void msm_ion_heap_free_pages_mem(struct pages_mem *pages_mem);

long msm_ion_custom_ioctl(struct ion_client *client,
			  unsigned int cmd,
			  unsigned long arg);

int ion_heap_is_system_secure_heap_type(enum ion_heap_type type);
int get_secure_vmid(unsigned long flags);
int get_vmid(unsigned long flags);
bool is_secure_vmid_valid(int vmid);
unsigned int count_set_bits(unsigned long val);
int populate_vm_list(unsigned long flags, unsigned int *vm_list, int nelems);

/**
 * Functions to help assign/unassign sg_table for System Secure Heap
 */

int ion_system_secure_heap_unassign_sg(struct sg_table *sgt, int source_vmid);
int ion_system_secure_heap_assign_sg(struct sg_table *sgt, int dest_vmid);
int ion_system_secure_heap_prefetch(struct ion_heap *heap, void *data);
int ion_system_secure_heap_drain(struct ion_heap *heap, void *data);

/**
 * ion_heap_init_shrinker
 * @heap:		the heap
 *
 * If a heap sets the ION_HEAP_FLAG_DEFER_FREE flag or defines the shrink op
 * this function will be called to setup a shrinker to shrink the freelists
 * and call the heap's shrink op.
 */
void ion_heap_init_shrinker(struct ion_heap *heap);

/**
 * functions for creating and destroying the built in ion heaps.
 * architectures can add their own custom architecture specific
 * heaps as appropriate.
 */

struct ion_heap *ion_heap_create(struct ion_platform_heap *);
void ion_heap_destroy(struct ion_heap *);
struct ion_heap *ion_system_heap_create(struct ion_platform_heap *);
void ion_system_heap_destroy(struct ion_heap *);

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *);
void ion_system_contig_heap_destroy(struct ion_heap *);

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *);
void ion_carveout_heap_destroy(struct ion_heap *);

struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap *);
void ion_chunk_heap_destroy(struct ion_heap *);
#ifdef CONFIG_CMA
struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *);
void ion_cma_heap_destroy(struct ion_heap *);
#else
static inline struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *h)
{
	return NULL;
}

static inline void ion_cma_heap_destroy(struct ion_heap *h) {}
#endif

struct ion_heap *ion_system_secure_heap_create(struct ion_platform_heap *heap);
void ion_system_secure_heap_destroy(struct ion_heap *heap);

struct ion_heap *ion_cma_secure_heap_create(struct ion_platform_heap *heap);
void ion_cma_secure_heap_destroy(struct ion_heap *heap);

/**
 * kernel api to allocate/free from carveout -- used when carveout is
 * used to back an architecture specific custom heap
 */
ion_phys_addr_t ion_carveout_allocate(struct ion_heap *heap, unsigned long size,
				      unsigned long align);
void ion_carveout_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size);
/**
 * The carveout heap returns physical addresses, since 0 may be a valid
 * physical address, this is used to indicate allocation failed
 */
#define ION_CARVEOUT_ALLOCATE_FAIL -1

/**
 * functions for creating and destroying a heap pool -- allows you
 * to keep a pool of pre allocated memory to use from your heap.  Keeping
 * a pool of memory that is ready for dma, ie any cached mapping have been
 * invalidated from the cache, provides a significant performance benefit on
 * many systems
 */

/**
 * struct ion_page_pool - pagepool struct
 * @high_count:		number of highmem items in the pool
 * @low_count:		number of lowmem items in the pool
 * @high_items:		list of highmem items
 * @low_items:		list of lowmem items
 * @mutex:		lock protecting this struct and especially the count
 *			item list
 * @gfp_mask:		gfp_mask to use from alloc
 * @order:		order of pages in the pool
 * @list:		plist node for list of pools
 * @inode:		inode for ion_pool pseudo filesystem
 *
 * Allows you to keep a pool of pre allocated pages to use from your heap.
 * Keeping a pool of pages that is ready for dma, ie any cached mapping have
 * been invalidated from the cache, provides a significant performance benefit
 * on many systems
 */
struct ion_page_pool {
	int high_count;
	int low_count;
	struct list_head high_items;
	struct list_head low_items;
	spinlock_t lock;
	struct device *dev;
	gfp_t gfp_mask;
	unsigned int order;
	struct plist_node list;
	struct inode *inode;
};

struct ion_page_pool *ion_page_pool_create(struct device *dev, gfp_t gfp_mask,
					   unsigned int order, bool movable);
void ion_page_pool_destroy(struct ion_page_pool *);
void *ion_page_pool_alloc(struct ion_page_pool *a, bool *from_pool);
void *ion_page_pool_alloc_pool_only(struct ion_page_pool *a);
void ion_page_pool_free(struct ion_page_pool *a, struct page *b);
void ion_page_pool_free_immediate(struct ion_page_pool *, struct page *);
int ion_page_pool_total(struct ion_page_pool *pool, bool high);
size_t ion_system_heap_secure_page_pool_total(struct ion_heap *heap, int vmid);

#ifdef CONFIG_ION_POOL_CACHE_POLICY
static inline void ion_page_pool_alloc_set_cache_policy
				(struct ion_page_pool *pool,
				struct page *page){
	void *va = page_address(page);

	if (va)
		set_memory_wc((unsigned long)va, 1 << pool->order);
}

static inline void ion_page_pool_free_set_cache_policy
				(struct ion_page_pool *pool,
				struct page *page){
	void *va = page_address(page);

	if (va)
		set_memory_wb((unsigned long)va, 1 << pool->order);

}
#else
static inline void ion_page_pool_alloc_set_cache_policy
				(struct ion_page_pool *pool,
				struct page *page){ }

static inline void ion_page_pool_free_set_cache_policy
				(struct ion_page_pool *pool,
				struct page *page){ }
#endif


/** ion_page_pool_shrink - shrinks the size of the memory cached in the pool
 * @pool:		the pool
 * @gfp_mask:		the memory type to reclaim
 * @nr_to_scan:		number of items to shrink in pages
 *
 * returns the number of items freed in pages
 */
int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
			  int nr_to_scan);

/**
 * ion_pages_sync_for_device - cache flush pages for use with the specified
 *                             device
 * @dev:		the device the pages will be used with
 * @page:		the first page to be flushed
 * @size:		size in bytes of region to be flushed
 * @dir:		direction of dma transfer
 */
static inline void ion_pages_sync_for_device(struct device *dev,
					     struct page *page, size_t size,
					     enum dma_data_direction dir)
{
	dma_sync_single_for_device(dev, page_to_phys(page), size, dir);
}

int ion_walk_heaps(struct ion_client *client, int heap_id,
		   enum ion_heap_type type, void *data,
		   int (*f)(struct ion_heap *heap, void *data));

struct ion_handle *ion_handle_get_by_id(struct ion_client *client, int id);

void ion_handle_put(struct ion_handle *handle, int count);

static inline bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return buffer->flags & ION_FLAG_CACHED;
}

#endif /* _ION_PRIV_H */
