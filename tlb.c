#include <stdlib.h>
#include <stdio.h>
#include "tlb.h"

tlb_t *init_tlb()
{
    tlb_t *tlb = (tlb_t *) malloc(sizeof(tlb_t));
    tlb->size = 0;
    tlb->head = NULL;
    return tlb;
}

/**
 * Creates a new entry in the translation lookaside buffer which is 32-bit unsigned integer
 * 16 most significant bits represent page number and 16 least significant bits represent frame address.
 */
tlb_entry_t *enqueue(tlb_t *tlb, unsigned int page_num, unsigned int frame_addr)
{
    if (!tlb) {
        return NULL;
    }
    if (tlb->size >= TLB_MAX_NUM_ENTRY) {
        tlb_entry_t *d = dequeue(tlb);
        free(d);
    }
    tlb_entry_t *entry = (tlb_entry_t *) malloc(sizeof(tlb_entry_t));
    if (!entry) {
        return NULL;
    }
    entry->data = ((page_num << 0x10) & MASK_PAGE_NUM_IN_TLB) | (frame_addr & MASK_FRAME_ADDR_IN_TLB);
    entry->next = NULL;
    if (!tlb->head) {
        tlb->head = entry;
        tlb->size++;
        return entry;
    }
    tlb_entry_t *curr_tlb_entr = tlb->head;
    while (curr_tlb_entr->next) {
        curr_tlb_entr = curr_tlb_entr->next;
    }
    curr_tlb_entr->next = entry;
    tlb->size++;
    return entry;
}


/**
 * Deletes the first tlb entry in the queueu.
 */
tlb_entry_t *dequeue(tlb_t *tlb)
{
    if (!tlb || tlb->size <= 0) {
        return NULL;
    }

    tlb_entry_t *curr_tlb_entr = tlb->head;
    if (!curr_tlb_entr) {
        return NULL;
    }

    tlb->head = curr_tlb_entr->next;
    tlb->size--;

    return curr_tlb_entr;
}


/**
 * Checks the translation lookaside buffer entries if the provided page number is present
 */
tlb_entry_t *look_up(tlb_t *tlb, unsigned int page_num)
{
    if (!tlb) {
        return NULL;
    }
    tlb_entry_t *curr = tlb->head;
    while (curr) {
        if (get_page_num(curr) == page_num) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}


/**
 * Masks the given tlb entry to extract frame address from 32-bit unsigned integer.
 */
unsigned int get_frame_addr(tlb_entry_t *entry)
{
    return (entry->data & MASK_FRAME_ADDR_IN_TLB);
}


/**
 * Masks the given tlb entry to extract page number from 32-bit unsigned integer.
 */
unsigned int get_page_num(tlb_entry_t *entry)
{
    return ((entry->data & MASK_PAGE_NUM_IN_TLB) >> 16);
}
