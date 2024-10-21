The OS Subsystems Project consists of four main components, each implementing a crucial part of an operating system. These subsystems are written in C++ and cover areas such as linking, process scheduling, I/O scheduling, and memory management.

Linker (Linker.cpp):
This subsystem is responsible for linking object files and managing symbol resolution. It combines various modules into a single executable by resolving addresses for external symbols and handling relocations. It processes input files, generates a final executable, and ensures proper memory mapping and symbol linking.

Scheduler (Scheduler.cpp):
Implements a process scheduling algorithm for managing CPU time allocation to various processes. It includes handling context switching and process states, ensuring efficient process execution. This part of the system manages the ready queue, selects processes for execution, and ensures that the CPU is utilized effectively according to the chosen scheduling policy.

I/O Scheduler (ioschedvaish.cpp):
Manages the scheduling of I/O requests, particularly disk operations. It implements algorithms for optimizing I/O performance by reducing disk seek times and improving throughput. The I/O scheduler organizes I/O requests into an efficient sequence, likely using algorithms such as First-Come-First-Serve (FCFS) or Shortest Seek Time First (SSTF) to ensure optimal access times and minimize latency.

Memory Management (vaishlab3.cpp):
This component focuses on virtual memory management, specifically implementing page replacement algorithms like Clock or Enhanced Second-Chance (working similarly to Least Recently Used). It maintains a page table for each process, tracks page references, and handles page faults. The system also manages the frame table, keeping track of which memory frames are in use and ensuring efficient memory utilization by replacing pages when necessary.

Tech Stack:
Programming Language: C++

Core Concepts:
Linking: Resolving addresses, handling external symbols, and producing executable files from object files.
Process Scheduling: Allocating CPU time to processes and handling context switching.
I/O Scheduling: Organizing and optimizing the execution order of I/O requests.
Memory Management: Implementing paging and page replacement algorithms for managing virtual memory.
