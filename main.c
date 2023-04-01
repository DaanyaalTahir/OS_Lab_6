#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "tlb.h"


#define MASK_VALID_BIT      ((unsigned int)0x80000000)
#define MASK_FRAME_NUMBER   ((unsigned int)0x7fffffff)
#define MASK_OFFSET         (0x000000ff)
#define MASK_PAGE_NUM       (0x0000ff00)
#define OUTPUT_FILENAME             ""
#define BACKING_STORE_FILENAME      "BACKING_STORE.bin"
#define LOGICAL_ADDRESSES_FILENAME  "addresses.txt"
#define BUFFER_LEN                  256
#define NUM_PAGE_TABLE_ENTRY        256                 // 2^8
#define NUM_PHYS_MEM_ENTRY          (NUM_PAGE_TABLE_ENTRY)
#define PAGE_SIZE                   256                 // 2^8 in bytes
#define EMPTY_PAGE_ENTRY            0
#define VALID_PAGE_ENTRY            0x80000000
#define INVALID_PAGE_ENTRY          0x00000000
#define FRAME_SIZE                  (PAGE_SIZE)
#define NUM_TLB_TABLE_ENTRY         16
#define BACKING_STORE_SIZE          ((unsigned int)(0x10000))

static unsigned int tlb_hit_rate = 0;
static unsigned int page_fault = 0;
static unsigned int current_frame_number = 0;
static int logical_addresses_size = 1;
static unsigned int *logical_addresses;
static unsigned int *page_table;
static unsigned char *physical_memory;


void read_logical_addresses(const char *filename, int *const size,unsigned int * const logical_addresses);
unsigned int get_page_number(int bin_val);
unsigned int get_offset(int bin_val);
unsigned int *init_page_table(unsigned int * const tbl);
int swap_in(unsigned int page_num, unsigned char * const mem, unsigned int * const page_table, unsigned int *curr_frm_num);
void write_to_physical_memory(unsigned char *buff, unsigned int offset, unsigned char * const mem);
void update_current_frame_num(unsigned int *curr_frm_num);
void update_page_table(unsigned int page_num, int frame_num, unsigned int * const page_table);
unsigned int get_frame_address_from_page_table(unsigned int page_num, const unsigned int * const page_table);
unsigned int consult_page_table(unsigned int page_num, bool *is_valid, const unsigned int * const page_table);
void check_page_table_entry_validity(unsigned int page_num, bool *is_valid, const unsigned int * const page_table);
unsigned char physical_memory_seek(unsigned int phys_addr, const unsigned char * const mem);
unsigned int generate_phys_addr_translation(unsigned int frame_addr, unsigned int offset);
void print_results();

int main(int argc, char **argv)
{
    g_tlb = init_tlb();
    page_table = (unsigned int *) malloc(NUM_PAGE_TABLE_ENTRY*sizeof(unsigned int));
    physical_memory = (unsigned char *) malloc(BACKING_STORE_SIZE * sizeof(unsigned char));

    // initialize page table
    init_page_table(page_table);

    // read logical address from addresses.txt
    logical_addresses = (unsigned int *) malloc(logical_addresses_size * sizeof(unsigned int));
    read_logical_addresses(LOGICAL_ADDRESSES_FILENAME, &logical_addresses_size, logical_addresses);

    for (int i = 0; i < logical_addresses_size; i++)
    {
        unsigned int page_n = get_page_number(logical_addresses[i]);
        unsigned int offset = get_offset(logical_addresses[i]);
        printf("Virtual address = %*u, page number = %*u, offset = %*u, ", 4, logical_addresses[i], 4, page_n, 4, offset);
        bool is_valid;
        unsigned int frame_addr;

        // Check if the address is already in TLB
        tlb_entry_t *tlb_result = look_up(g_tlb, page_n);

        if (tlb_result != NULL) // TLB Hit
        {
            tlb_hit_rate++;
            frame_addr = get_frame_addr(tlb_result);
        }
        else // TLB Miss
        {
            frame_addr = consult_page_table(page_n, &is_valid, page_table);
            if (!is_valid) // Page Fault
            {
                page_fault++;
                swap_in(page_n, physical_memory, page_table, &current_frame_number);
                frame_addr = consult_page_table(page_n, &is_valid, page_table);
            }
            enqueue(g_tlb, page_n, frame_addr);
        }

        // Generate physical address and read value from physical memory
        unsigned int phys_addr_trans = generate_phys_addr_translation(frame_addr, offset);
        printf("physical address translation %*u, ", 8,  phys_addr_trans);
        char ret_val = physical_memory_seek(phys_addr_trans, physical_memory);
        printf("Value = %*d\n", 4, (int) ret_val);
    }

    print_results();

    free(logical_addresses);
    return 0;

}

// Print results 
void print_results(){
    printf("Number of translated addresses = %u\n", logical_addresses_size);
    printf("Number of Page Faults = %u\n", page_fault);
    printf("Page Fault rate = %0.3f \n", (float)(((int)page_fault) / (float) logical_addresses_size));
    printf("Number of TLB Hits = %u\n", tlb_hit_rate);
    printf("TLB Hit rate = %0.3f\n", (float)(((int)tlb_hit_rate) / (float) logical_addresses_size));
}

void read_logical_addresses(const char *filename, int *const size, unsigned int *arr)
{
    // Open the file for reading
    FILE *fd = fopen(filename, "r");
    if (fd == NULL) {
        printf("Error opening the file");
    }

    // Define a buffer to hold each line of the file
    char buffer[BUFFER_LEN];
    
    // Initialize a variable to keep track of the current index of the array
    int curr_index = *size - 1;

    // Read each line of the file, convert it to an integer, and add it to the array
    while (fgets(buffer, BUFFER_LEN, fd) != NULL)
    {
        // Resize the array to fit the new element
        arr = (unsigned int *) realloc(arr, (curr_index + 1) * sizeof(unsigned int));

        // Convert the buffer to an integer and add it to the array
        *(arr + curr_index) = (unsigned int) atoi(buffer);

        // Clear the buffer
        memset(buffer, 0, BUFFER_LEN);

        // Update the current index of the array
        curr_index++;
    }

    // Update the size of the array
    *size = curr_index;

    // Store the array pointer in a global variable for later use
    logical_addresses = arr;
}


// Extracts page number from 32-bit unsigned integer
unsigned int get_page_number(int bin_val)
{
    return ((bin_val >> 8) & 0xff);
}

// Extract offset number (8-bit (max 255)) from virtual/logical address by masking last 8 bit.
unsigned int get_offset(int bin_val)
{
    return (bin_val & 0xff);
}

/**
 * Initialize the page table, every entry's most significant bit is set to INVALID
 * and rest of bits are set to 0 to inditicate empty page table entry
*/
unsigned int *init_page_table(unsigned int *tbl)
{
    if (!tbl) {
        tbl = (unsigned int *) calloc(NUM_PAGE_TABLE_ENTRY, sizeof(unsigned int));
    }
    for (int i = 0; i < NUM_PAGE_TABLE_ENTRY; i++)
    {
        tbl[i] = INVALID_PAGE_ENTRY;
    }
    return tbl;
}


/**
 * It handles the page fault that occurs when a cpu requests a frame address from page table but
 * the given page number is not present in the page table. The page corresponds to the page number
 * should bring in from Secondary Storage/HDD/Backing Store into the physical memory (ex. RAM).
 * Page table should be updated with the newly brought in frame address.
 */
int swap_in(unsigned int page_num, unsigned char * const mem, unsigned int * const page_table, unsigned int *curr_frm_num)
{
    // Calculate the offset in the physical memory where the new frame will be loaded
    unsigned int phys_mem_offset = *curr_frm_num * FRAME_SIZE;
    
    // Open the Backing Store file
    FILE *fd = fopen(BACKING_STORE_FILENAME, "rb");
    if (!fd)
        return -1;
    
    // Allocate a buffer to read in the new page
    unsigned char buffer[FRAME_SIZE];
    
    // Seek to the appropriate location in the Backing Store and read in the new page
    fseek(fd, page_num * FRAME_SIZE, SEEK_SET);
    size_t read = fread(buffer, 1, FRAME_SIZE, fd);
    
    // Check if the read was successful
    if (read != FRAME_SIZE) 
    {
        fclose(fd);
        return -1;
    }
    
    // Check if the memory pointer is not null before writing to the physical memory
    if (!mem)
    {
        fclose(fd);
        return -1;
    }
    
    // Write the new page to the physical memory
    write_to_physical_memory(buffer, phys_mem_offset, mem);
    
    // Update the page table with the new frame address
    update_page_table(page_num, phys_mem_offset, page_table);
    
    // Increment the frame number
    *curr_frm_num += 1;
    
    // Close the Backing Store file
    fclose(fd);
    
    // Return success
    return 0;
}



/**
 * Writes a char buffer that has the length FRAME_SIZE to the given physical memory,
 * starting from given offset.
 */
void write_to_physical_memory(unsigned char *buff, unsigned int offset, unsigned char *mem)
{
    for (int i = 0; i < FRAME_SIZE; i++)
    {
        *(mem + offset + i) = *(buff + i);
    }
}

/**
 * Increment the current free frame number by 1 and don't exceed the maximum num of frame entry
 * 
 */
void update_current_frame_num(unsigned int *curr_frm_num)
{
    *curr_frm_num = ((*curr_frm_num) + 1) % NUM_PHYS_MEM_ENTRY;
}

/**
 * Update the page table entry associated with the provided page number
 * with the given frame address (starting position of the frame in the physical memory)
 */
void update_page_table(unsigned int page_num, int frame_addr, unsigned int * const page_table)
{
    page_table[page_num] = frame_addr | MASK_VALID_BIT;
}

/**
 * Get the frame address from page table with the provided page number (index)
 */
unsigned int get_frame_address_from_page_table(unsigned int page_num, const unsigned int * const page_table)
{
    return page_table[page_num] & MASK_FRAME_NUMBER;
}

/**
 * It is a look up function to check if the given page number is valid or invalid in the page table
 * then return the frame address if valid, or page fault occurs otherwise.
 */
unsigned int consult_page_table(unsigned int page_num, bool *is_valid, const unsigned int * const page_table)
{
    // Check if the given page number is valid or not
    check_page_table_entry_validity(page_num, is_valid, page_table);
    // If the page number is valid, return the corresponding frame address
    if ((*is_valid) == true)
        return get_frame_address_from_page_table(page_num, page_table);
    // If the page number is not valid, page fault occurs, return -1
    return -1;
}


/**
 * Check if the provided page number is valid (which means the page is in the physical memory and no page fault)
 * or invalid (which means the requested logical address is not mapped to physical address yet and swap in operation might be needed)
 */
void check_page_table_entry_validity(unsigned int page_num, bool *is_valid, const unsigned int * const page_table)
{
    // If the valid bit of the page table entry is set, then the page is in physical memory and it's valid
    if ((page_table[page_num] & MASK_VALID_BIT) == MASK_VALID_BIT) {
        *is_valid = true;
        return;
    }
    // If the valid bit is not set, the page is not in physical memory and it's invalid
    *is_valid = false;
}


/**
 * Get the 1-byte value at the given physical address (frame address + offset) in the physical memory.
 */
unsigned char physical_memory_seek(unsigned int phys_addr, const unsigned char * const mem)
{
    return *(mem + phys_addr);
    
}


/**
 * Adds the provided offset (with in FRAME_SIZE range) to the given Frame address
 */
unsigned int generate_phys_addr_translation(unsigned int frame_addr, unsigned int offset)
{
    if (offset > FRAME_SIZE)
        return -1;
    return frame_addr + offset;
}