#include <iostream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
#include <unordered_map>
#include <sstream>
#include <deque>
#include <vector>
#include <climits>
// Code by Vaishnavi V 

using namespace std;

int number_of_frames = 16;
char algo = '\0';
string in_file;
string rand_file;
const int MAX_VPAGES = 64;

bool o_flag = false;
bool p_flag = false;
bool f_flag = false;
bool s_flag = false;

int* random_values;
int ind;
int size_of_random_values_array;

unsigned long long inst_count;
unsigned long long ctx_switches;
unsigned long long process_exits;
unsigned long long cost;


struct instruction_pair
{
    char type;
    int value;
};

struct frame_t
{
    int process_id=-1;
    int v_page=-1;
    int frame_number=-1;
    unsigned int age=0;
};

struct pte_t {
    unsigned int PRESENT:1;
    unsigned int REFERENCED:1;
    unsigned int MODIFIED:1;
    unsigned int WRITE_PROTECT:1;
    unsigned int PAGEDOUT:1;
    unsigned int FRAME_NO:7;
    unsigned int is_file_mapped:1;

    // Constructor to initialize bit-fields
    pte_t() : PRESENT(0), REFERENCED(0), MODIFIED(0), WRITE_PROTECT(0), PAGEDOUT(0), FRAME_NO(0), is_file_mapped(0) {}
};

struct VMA {
    int start_vpage,
    end_vpage;
    bool write_protected,
    file_mapped;
};

struct pstats {
    unsigned long unmaps, maps, ins, outs, fins, fouts,
    zeros, 
    segv,
    segprot;
};


class Process {
public:
    int process_id;
    std::vector<VMA> vmas;
    pte_t page_table[MAX_VPAGES];
    pstats process_stats;
    
    Process(int id) : process_id(id) {
        // Initialize page_table elements
        for (int i = 0; i < MAX_VPAGES; ++i) {
            
            page_table[i].REFERENCED = 
            page_table[i].PAGEDOUT = 
            page_table[i].MODIFIED = 
            page_table[i].WRITE_PROTECT = 
            page_table[i].PRESENT = 
            page_table[i].FRAME_NO = 0;
        }
        
    }
};

unordered_map<int, Process*> processTable;
deque<frame_t*> free_frames;
deque<instruction_pair> instruction_queue;

frame_t* frame_table;
Process* current_running_process=nullptr;
pte_t* current_page_table =nullptr;


void read_random_num_file(const string& filename) {
    ifstream input_file(filename);

    if (!input_file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        return;
    }

    string line;
    if (getline(input_file, line)) {
        size_of_random_values_array = stoi(line);
        random_values = new int[size_of_random_values_array];

        int index = 0;
        while (getline(input_file, line) && index < size_of_random_values_array) {
            random_values[index] = stoi(line);
            ++index;
        }

        if (index != size_of_random_values_array) {
            cerr << "Warning: File contains fewer random values than expected." << endl;
        }
    } else {
        cerr << "Error reading size from file: " << filename << endl;
    }

    input_file.close();
}

int get_random_values(int number_of_frames) {
    // Ensure the index stays within bounds of the array
    ind %= size_of_random_values_array;
    
    // Get the random value at the current index
    int random_value = random_values[ind];
    
    // Increment the index for the next call
    ++ind;
    
    // Return the value modulo the number of frames
    return random_value % number_of_frames;
}


void printStats(int processId, const pstats* statistics) {
    cout << "PROC[" << processId << "]: U=" << statistics->unmaps
         << " M=" << statistics->maps
         << " I=" << statistics->ins
         << " O=" << statistics->outs
         << " FI=" << statistics->fins
         << " FO=" << statistics->fouts
         << " Z=" << statistics->zeros
         << " SV=" << statistics->segv
         << " SP=" << statistics->segprot
         << endl;
}



class Pager {
public:
    virtual frame_t* select_victim_frame() = 0; 
    virtual void reset_age(int frame_number) = 0;

};

class FIFO_Pager : public Pager {

int hand = 0;
    void reset_age(int frame_number) override {
    }

    frame_t* select_victim_frame() override {
        hand = hand % number_of_frames;
        frame_t* frame_return = &frame_table[hand];
        hand = (hand + 1) % number_of_frames;
        return frame_return;
    }
};

class CLOCK_PAGER : public Pager {

    int hand = 0;

    void reset_age(int frame_number) override {
        // No operation needed for CLOCK
    }

    frame_t* select_victim_frame() override {
        hand = hand % number_of_frames;
        int temp = hand;
        bool first_iteration = true;
do {
    if (processTable[frame_table[temp].process_id]->page_table[frame_table[temp].v_page].REFERENCED == 1) {
        processTable[frame_table[temp].process_id]->page_table[frame_table[temp].v_page].REFERENCED = 0;
    } else {
        hand = (temp + 1) % number_of_frames;
        return &frame_table[temp];
    }
    temp = (temp + 1) % number_of_frames;
    first_iteration = false;
} while (temp != hand || first_iteration);

        
        hand = (temp + 1) % number_of_frames;
        return &frame_table[temp];
    }
};

class RANDOM_PAGER : public Pager {
private:
    int hand = 0;

public:
    void reset_age(int frame_number) override {
        // No operation needed for RANDOM
    }

    frame_t* select_victim_frame() override {
        int random_index = get_random_values(number_of_frames);
        return &frame_table[random_index];
    }
};


class NRU_PAGER: public Pager{
    
    int hand=0;
    int last=0;
    
    void reset_age(int frame_number)
    {
       //no code needed
    }
        frame_t* select_victim_frame()
        {
            hand=hand%number_of_frames;
            int c=0;
            int temp=hand;
            int class_0=-1,class_1=-1,class_2=-1,class_3=-1;
            int class_hand[] = {class_0, class_1, class_2, class_3};
            while(c==0 || temp!=hand)
            {
                c=1;
                Process* p=processTable[frame_table[temp].process_id];
                pte_t pte=p->page_table[frame_table[temp].v_page];
                int pred_class=2*pte.REFERENCED+pte.MODIFIED;
                switch(pred_class){
                case 0:
                {
                    if(class_hand[0]==-1)
                    {
                        class_hand[0]=temp;
                    }
                    break;
                }
                case 1:
                {
                    if(class_hand[1]==-1)
                    {
                        class_hand[1]=temp;
                    }
                    break;
                }
                case 2:
                {
                    if(class_hand[2]==-1)
                    {
                        class_hand[2]=temp;
                    }
                    break;
                }
                case 3:
                {
                    if(class_hand[3]==-1)
                    {
                        class_hand[3]=temp;
                    }
                    break;
                }
            }
               
                temp++;
                temp=temp%number_of_frames;
            }
            
           int i = 0;
do {
    if (class_hand[i] != -1) {
        temp = class_hand[i];
        break;
    }
    i++;
} while (i < 4);

hand = temp + 1;

if (inst_count - 1 - last >= 47) {
    last = inst_count;
    int j = 0;
    do {
        processTable[frame_table[j].process_id]->page_table[frame_table[j].v_page].REFERENCED = 0;
        j++;
    } while (j < number_of_frames);
}

            return(&frame_table[temp]);
        }
    
};

class Ager : public Pager {
private:
    int hand = 0;

public:
    void reset_age(int frame_number) override {
        frame_table[frame_number].age = 0;
    }

    frame_t* select_victim_frame() override {
        hand %= number_of_frames;
        int temp_hand = hand;
        int iteration_count = 0;
        unsigned int minimum_age = UINT_MAX;
        int minimum_age_hand = 0;

        do {
            if (iteration_count > 0 && temp_hand == hand) break;
            iteration_count= iteration_count+1;

            frame_table[temp_hand].age >>= 1;
            if (processTable[frame_table[temp_hand].process_id]->page_table[frame_table[temp_hand].v_page].REFERENCED == 1) {
                frame_table[temp_hand].age |= 0x80000000;
                processTable[frame_table[temp_hand].process_id]->page_table[frame_table[temp_hand].v_page].REFERENCED = 0;
            }
            if ( minimum_age > frame_table[temp_hand].age ) {
                minimum_age_hand = temp_hand;
                minimum_age = frame_table[minimum_age_hand].age;
            }
            temp_hand = (temp_hand + 1) % number_of_frames;
        } while (true);

        hand = minimum_age_hand + 1;
        return &frame_table[minimum_age_hand];
    }
};

class workingSetPager : public Pager {
private:
    int hand = 0;

public:
    void reset_age(int frame_number) override {
        frame_table[frame_number].age = inst_count;
    }

    frame_t* select_victim_frame() override {
        hand %= number_of_frames;
        int initial_hand = hand;
        bool first_iteration = true;
        unsigned int max_age = 0;
        int candidate_hand = initial_hand;

        do {
            if (!first_iteration && hand == initial_hand) break;
            first_iteration = false;

            if (processTable[frame_table[hand].process_id]->page_table[frame_table[hand].v_page].REFERENCED == 0) {
                unsigned int age_diff = inst_count - frame_table[hand].age;

                if (age_diff >= 50) {
                    candidate_hand = hand;
                    break;
                } else if (age_diff > max_age) {
                    max_age = age_diff;
                    candidate_hand = hand;
                }
            } else {
                frame_table[hand].age = inst_count;
                processTable[frame_table[hand].process_id]->page_table[frame_table[hand].v_page].REFERENCED = 0;
            }
            hand = (hand + 1) % number_of_frames;
        } while (true);

        hand = candidate_hand + 1;
        return &frame_table[candidate_hand];
    }
};

    
Pager* pager=nullptr;
extern int optind;

void frame_table_printer() {
    cout << "FT:";
    int frame_index = 0;
    do {
        if (frame_table[frame_index].process_id == -1) {
            cout << " *";
        } else {
            cout << " " << frame_table[frame_index].process_id << ":" << frame_table[frame_index].v_page;
        }
        frame_index++;
    } while (frame_index < number_of_frames);
    cout << "\n";
}


void page_table_printer() {
    int i = 0;
    do {
        cout << "PT[" << i << "]:";
        pte_t* page_table = processTable[i]->page_table;
        int j = 0;
        do {
            if (page_table[j].PRESENT != 0) {
                char R = page_table[j].REFERENCED ? 'R' : '-';
                char M = page_table[j].MODIFIED ? 'M' : '-';
                char S = (page_table[j].PAGEDOUT == 1 && page_table[j].is_file_mapped == 0) ? 'S' : '-';

                cout << " " << j << ":" << R << M << S;
            } else {
                if (page_table[j].PAGEDOUT == 1 && page_table[j].is_file_mapped == 0) {
                    cout << " #";
                } else {
                    cout << " *";
                }
            }
            j++;
        } while (j < MAX_VPAGES);
        cout << "\n";
        i++;
    } while (i < processTable.size());
}



void read_input_file(const string& input_file) {
    fstream file(input_file);
    if (!file.is_open()) {
        cout << "The file was not opened due to an error" << endl;
        return;
    }

    string line;
    int no_of_processes = 0;
    int no_of_vmas = 0;

    while (getline(file, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        no_of_processes = stoi(line);
        for (int k = 0; k < no_of_processes; ++k) {
            processTable[k] = new Process(k);
        }

        for (int i = 0; i < no_of_processes && getline(file, line);) {
            if (line.front() == '#') {
                continue;
            }
            no_of_vmas = stoi(line);

            for (int j = 0; j < no_of_vmas && getline(file, line);) {
                if (line.front() == '#') {
                    continue;
                }
                std::istringstream iss(line);
                int value1, value2, value3, value4;
                bool v3 = false, v4 = false;

                if (iss >> value1 >> value2 >> value3 >> value4) {
                    v3 = (value3 == 1);
                    v4 = (value4 == 1);
                    processTable[i]->vmas.push_back(VMA{value1, value2, v3, v4});
                }
                ++j;
            }
            ++i;
        }

        while (getline(file, line)) {
            if (line.empty() || line.front() == '#') {
                continue;
            }
           std::istringstream iss(line);
char instruction_type;
int instruction_value;

if (iss >> instruction_type >> instruction_value) {
    instruction_queue.push_back({instruction_type, instruction_value});
}

        }
    }

    file.close();
}


bool check_part_of_vma(int vpage, pte_t *pte) {
    const vector<VMA>& vmas1 = current_running_process->vmas;
    for (const auto& vma : vmas1) {
        if (vpage >= vma.start_vpage && vpage <= vma.end_vpage) {
            pte->WRITE_PROTECT = vma.write_protected ? 1 : 0;
            pte->is_file_mapped = vma.file_mapped ? 1 : 0;
            return true;
        }
    }
    return false;
}

void pager_init(char algo) {
    switch (algo) {
        case 'c':
            pager = new CLOCK_PAGER();
            break;
        case 'f':
            pager = new FIFO_Pager();
            break;
        case 'r':
            pager = new RANDOM_PAGER();
            break;
        case 'e':
            pager = new NRU_PAGER();
            break;
        case 'a':
            pager = new Ager();
            break;
        case 'w':
            pager = new workingSetPager();
            break;
        default:
            cerr << "Unknown algorithm: " << algo << endl;
            exit(EXIT_FAILURE);
    }
}

void free_frame_generator() {
    int frame_index = 0;
    do {
        frame_table[frame_index].frame_number = frame_index;
        free_frames.push_back(&frame_table[frame_index]);
        frame_index++;
    } while (frame_index < number_of_frames);
}


frame_t* get_frame() {
    frame_t* frame;
    if (!free_frames.empty()) {
        frame = free_frames.front();
        free_frames.pop_front();
    } else {
        frame = pager->select_victim_frame();
    }
    pager->reset_age(frame->frame_number);
    return frame;
}        

bool O_option = false, F_option = false, S_option = false, P_option = false;

void parse_command_line_options(int argc, char* argv[], int& num_frames, char& algo) {
  int opt;
    while ((opt = getopt(argc, argv, "f:a:o:")) != -1) {
        if (opt == 'f') {
            num_frames = atoi(optarg);
        } else if (opt == 'a') {
            algo = optarg[0];
        } else if (opt == 'o') {
            for (char* ptr_string = optarg; *ptr_string != '\0'; ptr_string++) {
                if (*ptr_string == 'O') {
                    O_option = true;
                } else if (*ptr_string == 'F') {
                    F_option = true;
                } else if (*ptr_string == 'S') {
                    S_option = true;
                } else if (*ptr_string == 'P') {
                    P_option = true;
                } else {
                    std::cerr << "Unknown option in -o: " << *ptr_string << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            std::cerr << "Usage: " << argv[0] << " -f<num_frames> -a<algo> [-o<options>] inputfile randomfile" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    if (optind + 2 != argc) {
        std::cerr << "Usage: " << argv[0] << " -f<num_frames> -a<algo> [-o<options>] inputfile randomfile" << std::endl;
        exit(EXIT_FAILURE);
    }

}

// From free queue
void process_free_queue(frame_t* newframe, int current_instruction_value) {
    auto& current_page = current_running_process->page_table[current_instruction_value];

    current_page.FRAME_NO = newframe->frame_number;
    newframe->process_id = current_running_process->process_id;
    newframe->v_page = current_instruction_value;

    if (current_page.PAGEDOUT == 1 || current_page.is_file_mapped == 1) {
        if (current_page.is_file_mapped == 1) {
            current_running_process->process_stats.fins += 1;
            cost += 2350;
            std::cout << " FIN" << std::endl;
        } else {
            cost += 3200;
            current_running_process->process_stats.ins += 1;
            std::cout << " IN" << std::endl;
        }
    } else {
        current_running_process->process_stats.zeros += 1;
        cost += 150;
        std::cout << " ZERO" << std::endl;
    }
}

void pte_reset(pte_t *pte_old)
{
       pte_old->FRAME_NO = 0;
       pte_old->MODIFIED = 0;
     pte_old->PRESENT = 0;
   pte_old->REFERENCED = 0;
}

void process_reset(Process *current_process_to_exit, int pag)
{
    current_process_to_exit->page_table[pag].PRESENT = 0;
    current_process_to_exit->page_table[pag].PAGEDOUT = 0;
    current_process_to_exit->page_table[pag].REFERENCED = 0;
    current_process_to_exit->page_table[pag].MODIFIED = 0;
    current_process_to_exit->page_table[pag].WRITE_PROTECT = 0;

}

void instruction_processor(){
int inst_counter=0;
while(!instruction_queue.empty())
    {
         
        inst_count++;
        instruction_pair instruction=instruction_queue.front();
        char current_instruction_type=instruction.type;
        int current_instruction_value=instruction.value;
        instruction_queue.pop_front();
        cout << inst_count-1 << ": ==> " << current_instruction_type << " " << current_instruction_value << endl;
        switch(current_instruction_type){
        case 'c':
        
            ctx_switches=ctx_switches+1;
            cost=cost+130;
            current_running_process=processTable[current_instruction_value];
            current_page_table=current_running_process->page_table;
            break;
        
        case 'r': {
    cost += 1;
    pte_t *pte = &current_running_process->page_table[current_instruction_value];

    if (pte->PRESENT == 0) {
        bool is_valid_vpage = check_part_of_vma(current_instruction_value, pte);

        if (is_valid_vpage == true) {
            frame_t *newframe = get_frame();
            int old_process_id = newframe->process_id;

            if (old_process_id == -1) {
                // From free queue
                process_free_queue(newframe,current_instruction_value );
            } else {
                pte_t *pte_old = &processTable[old_process_id]->page_table[newframe->v_page];
                processTable[old_process_id]->process_stats.unmaps += 1;
                cost += 410;
                std::cout << " UNMAP " << newframe->process_id << ":" << newframe->v_page << std::endl;

                if (pte_old->MODIFIED) {
                    pte_old->PAGEDOUT = 1;
                    if (pte_old->is_file_mapped) {
                        processTable[old_process_id]->process_stats.fouts += 1;
                        cost += 2800;
                        std::cout << " FOUT" << std::endl;
                    } else {
                        cost += 2750;
                        processTable[old_process_id]->process_stats.outs += 1;
                        std::cout << " OUT" << std::endl;
                    }
                }

                pte_reset(pte_old);

                current_running_process->page_table[current_instruction_value].FRAME_NO = newframe->frame_number;
                if (current_running_process->page_table[current_instruction_value].PAGEDOUT == 1 || current_running_process->page_table[current_instruction_value].is_file_mapped == 1) {
                    if (current_running_process->page_table[current_instruction_value].is_file_mapped == 1) {
                        current_running_process->process_stats.fins += 1;
                        cost += 2350;
                        std::cout << " FIN" << std::endl;
                    } else {
                        cost += 3200;
                        current_running_process->process_stats.ins += 1;
                        std::cout << " IN" << std::endl;
                    }
                } else {
                    current_running_process->process_stats.zeros += 1;
                    cost += 150;
                    std::cout << " ZERO" << std::endl;
                }

                newframe->process_id = current_running_process->process_id;
                newframe->v_page = current_instruction_value;
            }

            pte->PRESENT = 1;
            current_running_process->page_table[current_instruction_value].REFERENCED = 1;
            current_running_process->process_stats.maps += 1;
            cost += 350;
            std::cout << " MAP " << newframe->frame_number << std::endl;
        } else {
            current_running_process->process_stats.segv += 1;
            cost += 440;
            std::cout << " SEGV" << std::endl;
            continue;
        }
    } else {
        // Already present in frame, so just set the bits of reference
        current_running_process->page_table[current_instruction_value].REFERENCED = 1;
    }
    break;
}


case 'w': {
    cost += 1;
    pte_t *pte = &current_running_process->page_table[current_instruction_value];

    if (pte->PRESENT == 0) {
        bool is_valid_vpage = check_part_of_vma(current_instruction_value, pte);

        if (is_valid_vpage) {
            frame_t *newframe = get_frame();
            int old_process_id = newframe->process_id;

            if (old_process_id == -1) {
                current_running_process->page_table[current_instruction_value].FRAME_NO = newframe->frame_number;
                newframe->process_id = current_running_process->process_id;
                newframe->v_page = current_instruction_value;

                if (current_running_process->page_table[current_instruction_value].PAGEDOUT == 1 || current_running_process->page_table[current_instruction_value].is_file_mapped == 1) {
                    if (current_running_process->page_table[current_instruction_value].is_file_mapped == 1) {
                        current_running_process->process_stats.fins++;
                        cost += 2350;
                        cout << " FIN" << endl;
                    } else {
                        cost += 3200;
                        current_running_process->process_stats.ins++;
                        cout << " IN" << endl;
                    }
                } else {
                    current_running_process->process_stats.zeros++;
                    cost += 150;
                    cout << " ZERO" << endl;
                }
            } else {
                pte_t *pte_old = &processTable[old_process_id]->page_table[newframe->v_page];
                processTable[old_process_id]->process_stats.unmaps++;
                cost += 410;

                cout << " UNMAP " << newframe->process_id << ":" << newframe->v_page << endl;

                if (pte_old->MODIFIED) {
                    pte_old->PAGEDOUT = 1;

                    if (pte_old->is_file_mapped) {
                        processTable[old_process_id]->process_stats.fouts++;
                        cost += 2800;
                        cout << " FOUT" << endl;
                    } else {
                        cost += 2750;
                        processTable[old_process_id]->process_stats.outs++;
                        cout << " OUT" << endl;
                    }
                }

                pte_reset(pte_old);

                current_running_process->page_table[current_instruction_value].FRAME_NO = newframe->frame_number;

                if (current_running_process->page_table[current_instruction_value].PAGEDOUT == 1 || current_running_process->page_table[current_instruction_value].is_file_mapped == 1) {
                    if (current_running_process->page_table[current_instruction_value].is_file_mapped == 1) {
                        current_running_process->process_stats.fins++;
                        cost += 2350;
                        cout << " FIN" << endl;
                    } else {
                        cost += 3200;
                        current_running_process->process_stats.ins++;
                        cout << " IN" << endl;
                    }
                } else {
                    current_running_process->process_stats.zeros++;
                    cost += 150;
                    cout << " ZERO" << endl;
                }

                newframe->process_id = current_running_process->process_id;
                newframe->v_page = current_instruction_value;
            }

            pte->PRESENT = 1;
            current_running_process->page_table[current_instruction_value].REFERENCED = 1;
            current_running_process->process_stats.maps++;
            cost += 350;
            cout << " MAP " << newframe->frame_number << endl;

            if (current_running_process->page_table[current_instruction_value].WRITE_PROTECT) {
                current_running_process->process_stats.segprot++;
                cost += 410;
                cout << " SEGPROT" << endl;
            } else {
                current_running_process->page_table[current_instruction_value].MODIFIED = 1;
            }
        } else {
            current_running_process->process_stats.segv++;
            cost += 440;
            cout << " SEGV" << endl;
            continue;
        }
    } else {
        current_running_process->page_table[current_instruction_value].REFERENCED = 1;

        if (current_running_process->page_table[current_instruction_value].WRITE_PROTECT) {
            current_running_process->process_stats.segprot++;
            cost += 410;
            cout << " SEGPROT" << endl;
        } else {
            current_running_process->page_table[current_instruction_value].MODIFIED = 1;
        }
    }

    break;
}

       case 'e': {
    process_exits++;
    cost += 1230;
    cout << "EXIT current process " << current_running_process->process_id << endl;

    Process* current_process_to_exit = processTable[current_instruction_value];

   int pag = 0;
do {
    if (current_process_to_exit->page_table[pag].PRESENT == 1) {
        current_process_to_exit->process_stats.unmaps++;
        cost += 410;
        cout << " UNMAP " << current_process_to_exit->process_id << ":" << pag << endl;

        if (current_process_to_exit->page_table[pag].MODIFIED && current_process_to_exit->page_table[pag].is_file_mapped) {
            current_process_to_exit->process_stats.fouts++;
            cost += 2800;
            cout << " FOUT" << endl;
        }

        frame_table[current_process_to_exit->page_table[pag].FRAME_NO].process_id = -1;
        frame_table[current_process_to_exit->page_table[pag].FRAME_NO].v_page = -1;
        free_frames.push_back(&frame_table[current_process_to_exit->page_table[pag].FRAME_NO]);
    }

    process_reset(current_process_to_exit,pag);
  

    pag++;
} while (pag < MAX_VPAGES);

    break;
}

        }
        
        inst_counter=inst_counter+1;

    }
}

void print_process_stats()
{
  for (int proces=0;proces<processTable.size();proces++) {
        printStats(processTable[proces]->process_id,&processTable[proces]->process_stats);
        
    }
    cout << "TOTALCOST " << inst_count << " " << ctx_switches << " " << process_exits << " " << cost << " " << sizeof(pte_t) << endl;
    

}

int main(int argc, char *argv[]) {

    
    int opt;
        int num_frames = 16;
        char algo = '\0';
      

    parse_command_line_options(argc, argv, num_frames, algo);

    in_file = argv[optind];
    rand_file = argv[optind + 1];
    
    pager_init(algo);
    number_of_frames=num_frames;
    

    read_input_file(in_file);
    read_random_num_file(rand_file);
    
    
    //frame table
    
    frame_table=new frame_t[number_of_frames];
    free_frame_generator();
    
    instruction_processor();
    
    page_table_printer();
    frame_table_printer();

    print_process_stats();
   

}














