#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <assert.h>
#include <string.h>

#include "sim_mem.h"

using namespace std;

sim_mem::sim_mem(char exe_file_name[], char swap_file_name[], int text_size,
                int data_size, int bss_size, int heap_stack_size, int num_of_pages, int page_size) {
    if (exe_file_name == NULL || swap_file_name == NULL) {
        perror("Error: exe or swap is NULL\n");
        exit(1);
    }
    swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    program_fd = open(exe_file_name, O_RDONLY, 0);
    if (program_fd < 0 || swapfile_fd < 0) {
        perror("Error - open() failed\n");
        exit(1);
    }
    this -> num_of_pages = num_of_pages;

    this -> bss_size = bss_size;
    this -> text_size = text_size;
    this -> data_size = data_size;
    this -> page_size = page_size;
    this -> heap_stack_size = heap_stack_size;

    this -> page_table = (page_descriptor * ) malloc(sizeof(page_descriptor) * num_of_pages);
    assert(page_table != NULL);

    this -> frames = (bool * ) malloc(sizeof(bool) * (MEMORYSIZE / page_size));
    assert(frames != NULL);

    // Initiate main memory
    int i;
    for (i = 0; i < MEMORYSIZE; i++)
        main_memory[i] = '0';

    // Initiate page table
    for (i = 0; i < num_of_pages; i++) {
        page_table[i].D = 0;
        page_table[i].V = 0;
        page_table[i].swap_index = -1;
        page_table[i].frame = -1;

        if (i < (text_size / page_size)) // read
            page_table[i].P = 0;
        else
            page_table[i].P = 1; // write
    }

    // Initiate swap file
    for (i = 0; i < (page_size * num_of_pages); i++) {
        int check = write(swapfile_fd, "0", 1);
        if (check == -1) // error
        {
            perror("Error\n");
            free(page_table);
            free(frames);
            close(swapfile_fd);
            close(program_fd);
            exit(1);
        }
    }

    // Initiate freeFrame
    for (i = 0; i < MEMORYSIZE / page_size; i++)
        frames[i] = true;
}

char sim_mem::load(int address) {
    if (address >= page_size * num_of_pages || address < 0) // address not valid
    {
        printf("The address %d is not valid\n", address);
        return '\0';
    }

    int offSet = address % page_size;
    int page = address / page_size;

    if (page_table[page].V == 1) //  in memory
    {
        int physical_address = page_table[page].frame * page_size + offSet;
        return main_memory[physical_address];
    }

    // not in memory

    // from exe
    if (page_table[page].P == 0 || (page_table[page].P == 1 && page_table[page].D == 0 && address < text_size + data_size)) {
        int frame = this -> freeFrame();
        frames[frame] = false; // frame not empty
        page_order.push(page);

        page_table[page].frame = frame;
        page_table[page].V = 1;

        // copy from exe to buff
        char * buff = (char * ) malloc(sizeof(char) * (page_size + 1));
        assert(buff != NULL);
        lseek(program_fd, page * page_size, SEEK_SET);
        if (read(program_fd, buff, page_size) < 0) {
            perror("Error in load\n"); // read error
            free(buff);
            return '\0';
        }

        // copy from buff to main memory
        int physical_address = frame * page_size;
        for (int i = 0; i < page_size; i++) {
            main_memory[physical_address + i] = buff[i];
        }

        free(buff);
        return main_memory[frame * page_size + offSet];
    }

        // from swap
    else if (page_table[page].P == 1 && page_table[page].D == 1) {
        int frame = this -> freeFrame();
        frames[frame] = false; // frame not empty
        page_order.push(page);

        page_table[page].V = 1;
        page_table[page].frame = frame;

        // copy from swap to buff
        char * buff = (char * ) malloc(sizeof(char) * (page_size + 1));
        assert(buff != NULL);
        lseek(swapfile_fd, page * page_size, SEEK_SET);
        if (read(swapfile_fd, buff, page_size) < 0) {
            // read error
            perror("Error in load: read error\n");
            free(buff);
            return '\0';
        }

        // copy from buf to main memory
        int physical_address = frame * page_size;
        int i;
        for (i = 0; i < page_size; i++) {
            main_memory[physical_address + i] = buff[i];
        }

        free(buff);
        return main_memory[frame * page_size + offSet];
    }

        // bss - init page to zero
    else if (page_table[page].P == 1 && page_table[page].D == 0 && address < text_size + data_size + bss_size) {
        int frame = this -> freeFrame();
        frames[frame] = false; // frame not empty
        page_order.push(page);

        page_table[page].frame = frame;
        page_table[page].V = 1;

        int physical_address = frame * page_size;
        int i;
        for (i = 0; i < page_size; i++) {
            main_memory[physical_address + i] = '0';
        }
        return main_memory[physical_address + offSet];
    }

    // error heap or stack
    printf("Can't init new page from load (from heap or stack)\n");
    return '\0';
}

void sim_mem::store(int address, char value) {
    int page = address / page_size;
    int offset = address % page_size;

    // address not valid
    if (address >= page_size * num_of_pages || address < 0) {
        printf("address %d is not in memory range\n", address);
        return;
    }

    // no permission
    if (page_table[page].P == 0) {
        printf("No permission to store here(text area)\n");
        return;
    }

    //  in memory
    if (page_table[page].V == 1) {
        int physical_address = page_table[page].frame * page_size + offset;
        main_memory[physical_address] = value;
        page_table[page].D = 1;
        return;
    }

    // not in memory

    // from exe
    if (page_table[page].P == 1 && page_table[page].D == 0 && address < text_size + data_size) {
        int frame = this -> freeFrame();
        frames[frame] = false; // frame not empty
        page_order.push(page);

        page_table[page].frame = frame;
        page_table[page].V = 1;

        // copy from exe to buff
        char * buff = (char * ) malloc(sizeof(char) * (page_size + 1));
        assert(buff != NULL);
        lseek(program_fd, page * page_size, SEEK_SET);
        if (read(program_fd, buff, page_size) < 0) {
            // read error
            perror("Error in store: read error\n");
            free(buff);
            return;
        }

        // Copy from buf to main memory
        int physical_address = frame * page_size;
        int i;
        for (i = 0; i < page_size; i++) {
            main_memory[physical_address + i] = buff[i];
        }

        free(buff);
        main_memory[physical_address + offset] = value;
        page_table[page].D = 1;
        return;
    }

        // from swap
    else if (page_table[page].P == 1 && page_table[page].D == 1) {
        int frame = this -> freeFrame();
        frames[frame] = false; // frame not empty
        page_order.push(page);

        page_table[page].V = 1;
        page_table[page].frame = frame;

        // Copy from swap to buff
        char * buff = (char * ) malloc(sizeof(char) * (page_size + 1));
        assert(buff != NULL);
        lseek(swapfile_fd, page * page_size, SEEK_SET);
        if (read(swapfile_fd, buff, page_size) < 0) {
            // read error
            perror("Error in store: read error\n");
            free(buff);
            return;
        }

        // Copy from buff to main memory
        int physical_address = frame * page_size;
        for (int i = 0; i < page_size; i++) {
            main_memory[physical_address + i] = buff[i];
        }

        free(buff);
        main_memory[physical_address + offset] = value;
        page_table[page].D = 1;
        return;
    }

        // Init new page
    else if (page_table[page].P == 1 && page_table[page].D == 0 && address >= text_size + data_size) {
        int frame = this -> freeFrame();
        frames[frame] = false; // frame not empty
        page_order.push(page);

        page_table[page].frame = frame;
        page_table[page].V = 1;

        int physical_address = frame * page_size;
        for (int i = 0; i < page_size; i++) {
            main_memory[physical_address + i] = '0';
        }

        main_memory[physical_address + offset] = value;
        page_table[page].D = 1;
        return;
    }
}

//This function print the physical memory
void sim_mem::print_memory() {
    int i;
    printf("\n Physical memory\n");
    for (i = 0; i < MEMORYSIZE; i++) {
        printf("[%c]\n", main_memory[i]);
    }
}

//This function print the swap
void sim_mem::print_swap() {
    char * str = (char * ) malloc(this -> page_size * sizeof(char));
    int i;
    printf("\n Swap memory\n");
    lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
    while (read(swapfile_fd, str, this -> page_size) == this -> page_size) {
        for (i = 0; i < page_size; i++) {
            printf("%d - [%c]\t", i, str[i]);
        }
        printf("\n");
    }
    free(str);
}

//This function print the page table
void sim_mem::print_page_table() {
    int i;
    printf("\n page table \n");
    printf("Valid\t Dirty\t Permission \t Frame\t Swap index\n");
    for (i = 0; i < num_of_pages; i++) {
        printf("[%d]\t[%d]\t[%d]\t[%d]\t[%d]\n", page_table[i].V, page_table[i].D, page_table[i].P,
            page_table[i].frame, page_table[i].swap_index);
    }
}

sim_mem::~sim_mem() {
    free(page_table);
    free(frames);
    close(swapfile_fd);
    close(program_fd);
}

// Private functions

int sim_mem::frameOverride() {
    int page = page_order.front();
    page_table[page].V = 0;

    if (page_table[page].D == 1) // if pop page is dirty - transfer to swap
    {
        // copy the memory to buff
        int check;
        char * buff;
        int physical_address = page_table[page].frame * page_size;
        buff = (char * ) malloc(sizeof(char) * (page_size * num_of_pages + 1));
        assert(buff != NULL);
        char * buff2 = (char * ) malloc(sizeof(char) * (page_size * (num_of_pages - page - 1)));
        assert(buff2 != NULL);

        // copy from swap
        lseek(swapfile_fd, 0, SEEK_SET);
        check = read(swapfile_fd, buff, page * page_size);
        if (check < 0) {
            perror("Error in frameOverride\n");
        }

        // copy from main memory
        int i;
        for (i = 0; i < page_size; i++) {
            buff[page * page_size + i] = main_memory[physical_address + i];
        }

        // copy from the rest of swap
        lseek(swapfile_fd, (page + 1) * page_size, SEEK_SET);
        check = read(swapfile_fd, buff2, (num_of_pages - page - 1) * page_size);
        if (check < 0) {
            perror("Error in frameOverride\n");
        }

        for (i = 0; i < (num_of_pages - page - 1) * page_size; i++) {
            buff[(page + 1) * page_size + i] = buff2[i];
        }

        ftruncate(swapfile_fd, 0);
        check = write(swapfile_fd, buff, page_size * num_of_pages);
        if (check < 0) {
            perror("Error in frameOverride\n");
        }

        free(buff);
        free(buff2);
    }

    int frame = page_table[page].frame;
    page_order.pop();
    return frame;
}

int sim_mem::freeFrame() {
    int i;
    for (i = 0; i < MEMORYSIZE / page_size; i++) {
        if (frames[i] == true) {
            frames[i] = false;
            return i;
        }
    }

    //  override oldest frame
    int frame = this -> frameOverride();
    return frame;
}
