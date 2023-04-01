#ifndef VIRTUALMEMORYMANAGER_TLB_H
#define VIRTUALMEMORYMANAGER_TLB_H

#define TLB_MAX_NUM_ENTRY   0x10
#define MASK_PAGE_NUM_IN_TLB   0xffff0000
#define MASK_FRAME_ADDR_IN_TLB 0x0000ffff

typedef struct tlb_entry {
    unsigned int data;
    struct tlb_entry *next;
} tlb_entry_t;

typedef struct {
    unsigned int size;
    tlb_entry_t *head;
} tlb_t;

static tlb_t *g_tlb;

tlb_t *init_tlb();
tlb_entry_t *enqueue(tlb_t *tlb, unsigned int page_num, unsigned int frame_addr);
tlb_entry_t *dequeue(tlb_t *tlb);
tlb_entry_t *look_up(tlb_t *tlb, unsigned int page_num);
unsigned int get_frame_addr(tlb_entry_t *entry);
unsigned int get_page_num(tlb_entry_t *entry);


#endif 
