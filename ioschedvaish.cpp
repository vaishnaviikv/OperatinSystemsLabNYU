#include <unistd.h>

#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>

int track_head = 0;
int active_io = -1;
int simul_time = 0;

struct io_operation {
    int arr_time,
    track,
    start_time,
    completed_time;

  io_operation(int arr_time, int track) {
    this->arr_time = arr_time;
    this->track = track;
    this->start_time = -1;
    this->completed_time = -1;
}

};

class Scheduler {
public:
    std::list<int> io_queue;

    Scheduler() = default;
    virtual ~Scheduler() = default;

    virtual void add(int io_index) = 0;
    virtual int next() = 0;
    virtual bool is_empty() = 0;
};

// Globals
std::vector<io_operation> io_operations;
Scheduler* sched = nullptr;

void read_input_file(const std::string& filename) {
    std::ifstream file;
    file.open(filename);
    
    if (!file.is_open()) {
        // Handle error if needed
        return;
    }

    while (!file.eof()) {
        std::string line;
        std::getline(file, line);

        if (!line.empty() && line[0] != '#') {
            std::istringstream iss(line);
            int arr_time = 0, track = 0;
            iss >> arr_time >> track;

            if (iss) {
                io_operations.push_back(io_operation(arr_time, track));
            }
        }
    }

    file.close();
}


void print_summary() {
    int total_movement = 0,
    max_wait_time = 0;
    double total_turnaround_time = 0.0,
    total_wait_time = 0.0;

    size_t i = 0;
    while (i < io_operations.size()) {
        const io_operation& io = io_operations.at(i);
        std::printf("%5zu: %5d %5d %5d\n", i, io.arr_time, io.start_time, io.completed_time);

        int movement = io.completed_time - io.start_time,
        turnaround_time = io.completed_time - io.arr_time,
        wait_time = io.start_time - io.arr_time;

        total_movement = total_movement + movement;
        total_turnaround_time = total_turnaround_time + static_cast<double>(turnaround_time);
        total_wait_time = total_wait_time + static_cast<double>(wait_time);

        if (wait_time > max_wait_time) {
            max_wait_time = wait_time;
        }

        ++i;
    }

    double num_requests = static_cast<double>(io_operations.size()),
    io_utilization = static_cast<double>(total_movement) / static_cast<double>(simul_time),
    avg_turnaround_time = total_turnaround_time / num_requests,
    avg_wait_time = total_wait_time / num_requests;

    std::printf("SUM: %d %d %.4f %.2f %.2f %d\n", simul_time, total_movement,
           io_utilization, avg_turnaround_time, avg_wait_time, max_wait_time);
}

class FIFO : public Scheduler {
public:
    FIFO() = default;
    ~FIFO() override = default;

    void add(int io_index) override { 
        io_queue.push_back(io_index); 
    }

    int next() override {
        if (io_queue.empty()) {
            return -1;
        }

        int next_io = io_queue.front();
        io_queue.pop_front();
        return next_io;
    }

    bool is_empty() override { 
        return io_queue.empty(); 
    }

private:
    std::list<int> io_queue;
};


class SSTF : public Scheduler {
public:
    SSTF() = default;
    ~SSTF() override = default;

    void add(int io_index) override { 
        io_queue.push_back(io_index); 
    }

    int next() override {
        if (io_queue.empty()) {
            return -1;
        }

        auto min_it = io_queue.begin();
        int min_distance = std::numeric_limits<int>::max();
        int min_io = -1;

        for (auto it = io_queue.begin(); it != io_queue.end(); ++it) {
                int distance = (io_operations[*it].track > track_head) 
               ? io_operations[*it].track - track_head 
               : track_head - io_operations[*it].track;            
               if ( min_distance > distance ) {
                min_distance = distance;
                min_io = *it;
                min_it = it;
            }
        }

        io_queue.erase(min_it);
        return min_io;
    }

    bool is_empty() override { 
        return io_queue.empty(); 
    }

private:
    std::list<int> io_queue;
};


class LOOK : public Scheduler {
public:
    int direction = 1;

    LOOK() = default;
    ~LOOK() override = default;

    void add(int io_index) override { 
        io_queue.push_back(io_index); 
    }

    int next() override {
        if (io_queue.empty()) {
            return -1;
        }

        int min_distance = std::numeric_limits<int>::max();
        auto min_distance_it = io_queue.end();

        for (auto it = io_queue.begin(); it != io_queue.end(); ++it) {
            int io = *it;

            if ((direction == 1 && io_operations[io].track < track_head) ||
                (direction == -1 && io_operations[io].track > track_head)) {
                continue;
            }

        int distance;
        if (io_operations[io].track > track_head) {
         distance = io_operations[io].track - track_head;
        } else {
         distance = track_head - io_operations[io].track;
        }

if (distance < min_distance) {
    min_distance = distance;
    min_distance_it = it;
}

        }

        if (min_distance_it == io_queue.end()) {
            direction = -direction;
            return next();
        }

        int selected_io = *min_distance_it;
        io_queue.erase(min_distance_it);
        return selected_io;
    }

    bool is_empty() override { 
        return io_queue.empty(); 
    }

private:
    std::list<int> io_queue;
};


class CLOOK : public Scheduler {
public:
    CLOOK() = default;
    ~CLOOK() override = default;

    void add(int io_index) override { 
        io_queue.push_back(io_index); 
    }

    int next() override {
        if (io_queue.empty()) {
            return -1;
        }

        std::vector<int> distances;
        distances.reserve(io_queue.size());

        for (const auto& io : io_queue) {
           int distance = io_operations[io].track - track_head;
            distances.emplace_back(distance);

        }

auto smallest_positive_it = distances.begin();
for (auto it = distances.begin(); it != distances.end(); ++it) {
    if (*it >= 0 && (*smallest_positive_it < 0 || *it < *smallest_positive_it)) {
        smallest_positive_it = it;
    }
}


        auto selected_it = (smallest_positive_it != distances.end() && *smallest_positive_it >= 0)
                           ? smallest_positive_it
                           : std::min_element(distances.begin(), distances.end());

        auto io_it = std::next(io_queue.begin(), std::distance(distances.begin(), selected_it));
        int selected_io = *io_it;
        io_queue.erase(io_it);

        return selected_io;
    }

    bool is_empty() override { 
        return io_queue.empty(); 
    }

private:
    std::list<int> io_queue;
};

class FLOOK : public Scheduler {
public:
    int direction = 1;
    std::list<int> add_queue;

    FLOOK() = default;
    ~FLOOK() override = default;

    void add(int io_index) override { 
        add_queue.push_back(io_index); 
    }

    int next() override {
        if (io_queue.empty()) {
            io_queue.swap(add_queue);
        }

        if (io_queue.empty()) {
            return -1;
        }

        int min_distance = std::numeric_limits<int>::max();
        auto selected_it = io_queue.end();

        for (auto it = io_queue.begin(); it != io_queue.end(); ++it) {
            int io = *it;
            if ((direction == 1 && io_operations[io].track < track_head) ||
                (direction == -1 && io_operations[io].track > track_head)) {
                continue;
            }

        int distance;
if (io_operations[io].track >= track_head) {
    distance = io_operations[io].track - track_head;
} else {
    distance = track_head - io_operations[io].track;
}

if (distance < min_distance) {
    min_distance = distance;
    selected_it = it;
}

        }

        if (selected_it == io_queue.end()) {
            direction = -direction;
            return next();
        }

        int selected_io = *selected_it;
        io_queue.erase(selected_it);
        return selected_io;
    }

  bool is_empty() override {
    if (io_queue.size() == 0) {
        return true;
    }
    return false;
}

private:
    std::list<int> io_queue;
};


void simulation() {
    int io_ptr = 0;  // index to the next IO request to be processed

    while (true) {
        // Add new IO to the scheduler if it's time
        if (io_ptr < io_operations.size() && io_operations[io_ptr].arr_time == simul_time) {
            sched->add(io_ptr);
            io_ptr++;
        }

    // Check if the active IO request has completed
if (active_io != -1) {
    if (io_operations[active_io].track == track_head) {
        io_operations[active_io].completed_time = simul_time;
        active_io = -1;
    }
}

// Process the next IO request if none is active
while (active_io == -1) {
    int next_io = sched->next();
    if (next_io != -1) {
        io_operations[next_io].start_time = simul_time;
        active_io = next_io;

        if (io_operations[next_io].track == track_head) {
            io_operations[next_io].completed_time = simul_time;
            active_io = -1;
        }
    } else {
        if (io_ptr >= io_operations.size()) {
            return;
        }
        break;
    }
}

        // Move the track head towards the active IO's track
        if (active_io >= 0) {
            track_head += (io_operations[active_io].track > track_head) ? 1 : -1;
        }

        // Increment the simulation time
        simul_time++;
    }
}

int main(int argc, char* argv[]) {
    int c;
    char alg = '\0';

    while ((c = getopt(argc, argv, "s:vqf")) != -1) {
        if (c == 's') {
            alg = optarg[0];
            if (alg == 'N') {
                sched = new FIFO();
            } else if (alg == 'S') {
                sched = new SSTF();
            } else if (alg == 'L') {
                sched = new LOOK();
            } else if (alg == 'C') {
                sched = new CLOOK();
            } else if (alg == 'F') {
                sched = new FLOOK();
            } else {
                std::cerr << "Invalid scheduler algorithm specified." << std::endl;
                return 1;
            }
        } else if (c == 'v' || c == 'q' || c == 'f') {
            // Handle other options if needed
        } else {
            std::cerr << "Invalid option specified." << std::endl;
            return 1;
        }
    }

 if (optind < argc) {
    // Proceed with normal execution
} else {
    std::cerr << "No input file specified." << std::endl;
    return 1;
}

    std::string inputfile = argv[optind];
    read_input_file(inputfile);
    simulation();
    print_summary();

    return 0;
}
