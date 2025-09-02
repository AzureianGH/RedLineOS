#ifndef PALLOC_H
#define PALLOC_H

#include <extendedint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

void palloc_init(struct limine_memmap_response* memmap);
void* palloc_allocate_page(void); // Allocates a single 4KiB page
void palloc_free_page(void* page); // Frees a single 4KiB page
bool palloc_is_page_allocated(void* page); // Checks if a page is allocated or free
size_t palloc_get_free_page_count(void); // Returns the number of free pages
size_t palloc_get_total_page_count(void); // Returns the total number of pages
size_t palloc_get_used_page_count(void); // Returns the number of used pages
void* palloc_zero_allocate_page(void); // Allocates a single 4KiB page and zeroes it

#endif  /* PALLOC_H */