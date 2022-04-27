# Virtual Memory Simulator
This is a virtual memory simulator written in C++. It lets the user store and load information. It uses a page table, swaps data when the virtual memory is full, and differentiates data from the stack and heap.

## Usage
Compile with:
```
g++ main.cpp sim_mem.cpp -o sim_mem
```

Run with
```
./sim_mem
```

## Flow Diagram
![Flow Diagram](https://user-images.githubusercontent.com/99280430/165580309-4b8c16f9-a519-40be-8682-0fffdedcf048.png)

