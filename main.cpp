#include <iostream>

#include "sim_mem.h"

char main_memory[MEMORYSIZE];

int main() {
    char val;
    sim_mem mem_sm((char * )
                           "exec_file", (char * )
                           "swap_file", 25, 50, 25, 25, 25, 5);
    mem_sm.store(98, 'X');
    mem_sm.load(4);
    for (int i = 0; i < MEMORYSIZE; i++)  // shows what happens when the memory is full (over)
        mem_sm.store(i+3, 'Y');
    mem_sm.load(9);
    mem_sm.print_page_table();
    val = mem_sm.load(98);
    mem_sm.print_memory();
    mem_sm.print_swap();
}
