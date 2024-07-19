#include<iostream>
#include <list> 
#include <iterator> 
#include<getopt.h>
#include<fstream>
#include<string.h>
#include<stdio.h>
#include<vector>

enum Trans_to {
    TRANS_TO_READY, 
    TRANS_TO_RUN,
    TRANS_TO_BLOCK, 
    TRANS_TO_PREEMPT}; 
using namespace std;
enum State {
    CREATED,
    READY,
    RUNNING,
    BLOCKED, 
    DONE,
    PREMPT}; 
bool verbose; 
bool param_e; 
bool param_t; 
int quantum = 10000; 
int max_prio=4; 
int last_event_finish;
int total_cpu_utilization=0;
int total_io_utilization=0;
int total_tt;
int total_cw;
bool isPrePrio=false;
int io_start = 0;
int io_cnt=0;
bool call_scheduler;
int current_time;

int time_in_prev_state;
int cpu_burst;

Trans_to trans_to;
string scheduler_type;
vector<int> random_number;
int total_rand, ofs =0;

 
class Process
{
    public:
        int static_priority; // static priority
        int dynamic_priority; // dynamic priority
        int state_ts; 
        int cpu_burst_rem; 
        int pid; 
        State state; 
        int AT, TC, CB, IO; //Arrival time, Total CPU time, CPU Burst,  IO Burst
        int CW; // CPU waiting
        int FT; // Finish time
        int IT; // IO time
        int CT; // CPU time or total time when cpu is used
        int RT, TT; // Response time, Turnaround time
   
        Process(int id, int AT, int TC, int CB, int IO)
        {
            pid = id;
            this->AT = AT;
            RT = -1;
            this->TC = TC;
            CT = IT = CW = 0;
            cpu_burst_rem = 0;
            this->IO = IO;
            this->CB = CB;
            state = CREATED;
        }

        void printInfo()
        {
            printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",pid, AT, TC, CB, IO, static_priority, FT, TT, IT, CW);
        }
};

Process *proc_running = NULL;
Process *proc;
vector<Process> process_queue;

class Scheduler
{
    protected:
        list<Process *> run_queue;

    public:
    // Runtime polymorphism
        virtual void addProcess(Process * process)=0;
	    virtual Process * getNextProcess()=0;

        void printRunQueue()
        {   int counter =0;
            cout << "SCHED(" << run_queue.size() << "): ";
            list<Process *>::iterator i ;
            for(i = run_queue.begin(); i!= run_queue.end(); )
            {
                Process *p = *i;
                printf("%d:%d ",p->pid,p->state_ts);
                i++;
                counter ++;
            }
            cout << endl;
        }
};

Scheduler *scheduler;

class FCFS_LCFS_RR: public Scheduler
{
    public:
       
        Process * getNextProcess()
        {
            Process *p;
            if(!run_queue.empty()&& scheduler_type =="LCFS")
            {
                p = run_queue.back();
                run_queue.pop_back();
            }
            else if(!run_queue.empty()&& scheduler_type =="FCFS")
            {
                p = run_queue.front();
                run_queue.erase(run_queue.begin());
            }
            else if(!run_queue.empty() && scheduler_type=="RR")
            {
                p = run_queue.front();
                run_queue.pop_front();
            }
            else
            {
                return NULL;
            }
            return p;
        }

         void addProcess(Process * process)
        {
            run_queue.push_back(process);
        }

};


// Shortest remaining time first
class SRTF: public Scheduler
{
    public:

        Process * getNextProcess()
        {
            Process *p;
            if(!run_queue.empty())
            {
                p = run_queue.front();
                run_queue.pop_front();
            }
            else
            {
                return NULL;
            }
            return p;
        }

        void addToEmptyQueue(Process * process )
        {
             run_queue.push_back(process);
        }

        void addToIt( Process * process , list<Process *>::iterator it )
        {
            run_queue.insert(it,process);
        }

        void addProcess(Process * process)
        {
            int flag = 0;
            int pos = 0;
            list<Process *>::iterator i;
            for(i = run_queue.begin() ; i!= run_queue.end(); i++)
            {
                pos++;
                Process *p = *i;
                int x = process->TC - process->CT;
                int y = p->TC - p->CT;
                if( x < y )
                {
                    flag=1;
                    break;
                }
            }
            if(flag==0)
            {
                addToEmptyQueue(process);
            }
            else
            {
                list<Process *>::iterator it = run_queue.begin();
                advance(it,pos-1);
                addToIt(process, it);
                
            }
        }


};

// priority scheduler
class PRIO: public Scheduler
{
   
    vector<list<Process *> >  *active_queue, *expired_queue, queue1, queue2; 
    int num_run, num_exp;
  
    public:
        void createActiveQueue(Process * process)
        {
            for(int i=0; i<max_prio; i++)
            {
                queue1.push_back(list<Process *>());
                queue2.push_back(list<Process *>());
            }

            active_queue = &queue1;
            expired_queue = &queue2;
            num_run=num_exp=0;
            
        }

        void addExpired(Process * process)
        {
              process->dynamic_priority = process->static_priority-1;
              expired_queue->at(process->dynamic_priority).push_back(process);
              num_exp++;
        }

        void addActive(Process * process)
        {
            active_queue->at(process->dynamic_priority).push_back(process);
            num_run++;
        }
      
        void addProcess(Process * process)
        {
            if(active_queue==NULL)
            {
            createActiveQueue(process);
            
            }

            if(process->state==PREMPT)
            {
                process->dynamic_priority--;
            }

            process->state = READY;

            if(process->dynamic_priority<0)
            {
              addExpired(process);
            }

            else
            {
            addActive(process);
            } 
        }

        Process * getNextProcess()
        {
            if(num_run==0) 
            {
            num_run = num_exp;
            num_exp = 0; 
            vector<list<Process *> > *temp = expired_queue;
            vector<list<Process *> > *temp2 = active_queue;
            expired_queue = temp2 ;
            active_queue = temp;

        } 

            Process *p = NULL;
            int count_queue = -1;
            bool check;
            for(int i=0; i<active_queue->size(); i++)
            {   check = active_queue->at(i).empty();
                if(!check)
                {
                    count_queue=i;
                }
            }
            
            if(count_queue>=0)
            {   num_run--;
                p = active_queue->at(count_queue).front();
                active_queue->at(count_queue).pop_front(); 
            }

            return p;
        }
};


class Event
{
    public:
        int timestamp;
        Process *process;
        Trans_to transition;
        

        Event(int timestamp, Process *process, Trans_to trans_to )
        {
            this->timestamp = timestamp;
            this->process = process;
            this->transition = trans_to;
        }
        State old_state;
};

Event *e;

class DES
{
    public:
        list <Event> event_queue;

        void putEvent(Event event)
        {
            
            if(param_e)
            {
                string trans;
                 switch(event.transition) 
            {
                case TRANS_TO_BLOCK:
                    trans = "BLOCK"; 
                    break;
                case TRANS_TO_RUN :
                     trans = "RUNNG";
                     break;
                case TRANS_TO_READY:
                    trans = "READY";
                    break;
                case TRANS_TO_PREEMPT:
                    trans = "PREEMPT";
                    break;
            }
                cout << "AddEvent(" << event.timestamp << ":" << event.process->pid << ":" << trans << "): ";

                printQueue();
                printf("==> ");
            }

            
            int pos=0;
            int flag= 1;
            list<Event>::iterator i ;
            for(i = event_queue.begin() ; i!= event_queue.end(); i++)
            {
                pos++;
                Event e = *i;
                if(event.timestamp<e.timestamp)
                {
                    flag=0;
                    break;
                }
            }
            
            if(flag==0)
            {
                

                list<Event>::iterator it = event_queue.begin();
                advance(it,pos-1);
                event_queue.insert(it,event);
            }
            else
            {
                event_queue.push_back(event);
            }

            if(param_e)
            {
                printQueue();
                cout << endl;
            }
        }

        Event * getEvent()
        {
            if(!event_queue.empty())
            {
                return &event_queue.front();
                
            }

            return NULL;
        }

        void pop()
        {
            event_queue.pop_front();
        }

        int getNextEventTime()
        {
            if(!event_queue.empty())
            {
                return event_queue.front().timestamp;
                 
            }
            
           return -1;
        }
        void deleteEmptyFutureEvents(Process *p,list<Event>::iterator it )
        {
            event_queue.erase(it);
        }
        void deleteFutureEvents(Process *p)
        {
            int pos = 0;
            int flag = 1;
            list<Event>::iterator i;
            for(i = event_queue.begin() ; i!= event_queue.end(); i++)
            {
                pos++;
                Event e = *i;
                if(e.process==p)
                {
                    flag = 0;
                    break;
                }
            }
            int pos1 = pos -1;

            if(flag==0)
            {
                list<Event>::iterator it ;
                it = event_queue.begin();
                advance(it,pos1);
                deleteEmptyFutureEvents(p,it);
                
            }
        }

        void printQueue()
        {   
            string trans, old;
            
            for(list<Event>::iterator i = event_queue.begin() ; i!= event_queue.end(); i++)
            {
                Event e = *i;
                switch(e.transition) 
            {
                case TRANS_TO_BLOCK:
                    trans = "BLOCK"; 
                    break;
                case TRANS_TO_RUN :
                     trans = "RUNNG";
                     break;
                case TRANS_TO_READY:
                    trans = "READY";
                    break;
                case TRANS_TO_PREEMPT:
                    trans = "PREEMPT";
                    break;
            }
                cout << "AddEvent(" << e.timestamp << ":" << e.process->pid << ":" << trans << "): ";
            }

        }
};


void readFileZeroPrio(string file_name)
{
    ifstream file;
    file.open(file_name);
    string line;
    bool first_tok = true;

         while(file>>line)
        {
            if(first_tok)
            {
                total_rand = stoi(line);
                first_tok = false;
            }
            else
            {
                random_number.push_back(stoi(line));
               
            }

        }
}

// Reads the file and creates process objects 
void readFile(string file_name, int prio)
{
    ifstream file;
    file.open(file_name);
    string line;
    if(file.is_open())
    {  if(prio == 0) {

        readFileZeroPrio(file_name);

          }

        else {
        int i=0, AT, TC, CB, IO;
        while(file>>line)
        {   
            AT = stoi(line);
            file>>line;
            TC = stoi(line);
            file>>line;
            CB = stoi(line);
            file>>line;     
            IO = stoi(line);

            Process p(i,AT,TC,CB,IO);
            if(ofs==total_rand) {
                ofs=0;
            }
            p.static_priority = 1 + (random_number[ofs++] % prio);
            p.dynamic_priority = p.static_priority-1;
            p.state_ts = AT;
            process_queue.push_back(p);
            
            i++;
        } }

        file.close();
    }
    else
    {
        cout << "File " << file_name << " could not be opened." << endl;
    }    
}



void scheduler_call(DES *des)
{ int time = des->getNextEventTime() ;
  if(time == current_time)
            {
                return;
            }

            call_scheduler = false;

            if(proc_running==NULL)
            {   
                proc_running = scheduler->getNextProcess();
                
                if(proc_running==NULL)
                    return;

                Event e(current_time, proc_running, TRANS_TO_RUN);
                e.old_state = READY;
                des->putEvent(e);
            }
}


void simulation(DES *des)
{
    
    while(!((e = des->getEvent())==NULL))
    {
        proc = e->process;
        current_time = e->timestamp;
        time_in_prev_state = current_time - proc->state_ts;
        proc->state_ts = current_time;
        trans_to = e->transition;

        if(trans_to ==TRANS_TO_READY) {
                                    string old;
                                    switch (e->old_state) {
                                    case CREATED:
                                        old = "CREATED";
                                        break;
                                    case BLOCKED:
                                        old = "BLOCK";
                                        break;
                                    case RUNNING:
                                        old = "RUNNG";
                                        break;
                                    default : 
                                    old = "UNKNOWN"; 
                                    }
                                
                                    if(verbose)
                                        cout << current_time << " " << proc->pid << " " << time_in_prev_state << ": " << old << " -> " << "READY" << endl;

                                     if(e->old_state==RUNNING)
                                    {
                                        proc->CT = proc->CT + time_in_prev_state;
                                        total_cpu_utilization = time_in_prev_state + total_cpu_utilization;
                                    }
                                    else if(e->old_state==BLOCKED)
                                    {
                                        proc->dynamic_priority = proc->static_priority - 1;
                                        proc->IT = proc->IT + time_in_prev_state;
                                        io_cnt= io_cnt -1;
                                        if(io_cnt ==0)
                                        {
                                            total_io_utilization = total_io_utilization + current_time - io_start;
                                        }  
                                    }

                                    if(isPrePrio && proc_running!=NULL)
                                    {
                                        if(proc->state==CREATED)
                                        {
                                            int cts;
                                            list<Event>::iterator i ;
                                            
                                            for(i = des->event_queue.begin(); i != des->event_queue.end();i++)
                                            {
                                                Event e = *i;
                                                if(e.process==proc_running)
                                                {
                                                    cts = e.timestamp;
                                                }
                                            }
                                            bool check = proc_running->dynamic_priority < proc->dynamic_priority ;
                                            if(check)
                                            {
                                                if (cts > current_time ){
                                                des->deleteFutureEvents(proc_running);
                                                Event e(current_time, proc_running, TRANS_TO_PREEMPT);
                                                e.old_state = RUNNING;
                                                des->putEvent(e);
                                                }
                                            }
                                             
                                        }

                                        if( proc ->state == BLOCKED)
                                        {
                                            int cts;
                                            list<Event>::iterator i ;
                                            for( i = des->event_queue.begin(); i != des->event_queue.end();i++)
                                            {
                                                Event e = *i;
                                                if(e.process==proc_running)
                                                {
                                                    cts = e.timestamp;
                                                }
                                            }
                                            bool check = proc_running->dynamic_priority < proc->dynamic_priority;
                                            if(check)
                                            {
                                                if (current_time < cts ){
                                                des->deleteFutureEvents(proc_running);
                                                Event e(current_time, proc_running, TRANS_TO_PREEMPT);
                                                e.old_state = RUNNING;
                                                des->putEvent(e);
                                                }
                                            }
                                        }
                                    }

                                    scheduler->addProcess(proc);
                                    call_scheduler = true;
                                    
                                }
            else if(trans_to ==TRANS_TO_RUN)  { 
                                    proc->state = RUNNING;
                                    proc_running = proc;
                                    if(0< proc->cpu_burst_rem)
                                    {
                                        cpu_burst = proc->cpu_burst_rem;
                                    }
                                    else
                                    {
                                        
                                        if(ofs==total_rand) ofs=0;
                                        cpu_burst = 1 + (random_number[ofs++] % proc->CB); 
                                        
                                       
                                        if(proc->TC - proc->CT < cpu_burst )
                                        {
                                            cpu_burst = - proc->CT + proc->TC ;
                                        }
                                        
                                    }

                                    proc->CW = time_in_prev_state + proc->CW ;
                                    total_cw = time_in_prev_state + total_cw ;
                                    proc->cpu_burst_rem = cpu_burst;

                                    if(verbose)
                                    {
                                        printf("%d %d %d: %s -> %s cb=%2d rem=%d prio=%d\n",current_time, proc->pid, time_in_prev_state, "READY", "RUNNG" ,cpu_burst, proc->TC-proc->CT, proc->dynamic_priority);
                                    }

                                    if(quantum >= cpu_burst)
                                    {   int x  = current_time+cpu_burst;
                                        Event e(x, proc, TRANS_TO_BLOCK);
                                        e.old_state = RUNNING;
                                        des->putEvent(e);
                                    }
                                    else
                                    {   int x = current_time + quantum;
                                        Event e(x, proc, TRANS_TO_PREEMPT);
                                        e.old_state = RUNNING;
                                        des->putEvent(e);
                                    }

                                    
                                }  
            else if(trans_to ==TRANS_TO_BLOCK)    {
                                        proc_running = NULL;
                                        proc->CT = proc->cpu_burst_rem + proc->CT ;
                                        total_cpu_utilization = time_in_prev_state + total_cpu_utilization;
                                        proc->cpu_burst_rem = proc->cpu_burst_rem - time_in_prev_state;
                                        

                                        if(proc->TC-proc->CT == 0)
                                        {
                                            proc->state = DONE;
                                            proc->FT = current_time;
                                            proc->TT = proc->FT - proc->AT;

                                            last_event_finish = current_time;
                                            total_tt = proc->FT - proc->AT+ total_tt ;   

                                            if (verbose)
                                            {
                                                 printf("%d %d %d: Done \n", current_time, proc->pid, time_in_prev_state);
                                            }

                                        }
                                        else
                                        {
                                            proc->state = BLOCKED;

                                            if(io_cnt==0)
                                                io_start = current_time;

                                            io_cnt++;

                                             if(ofs==total_rand) ofs=0;
                                            int ib = 1 + (random_number[ofs++] % proc->IO); 
                                            Event e(current_time+ib, proc, TRANS_TO_READY);
                                            e.old_state = BLOCKED;
                                            
                                            des->putEvent(e);
                                            if(verbose)
                                            {
                                                printf("%d %d %d: %s -> %s ib=%2d rem=%d \n",current_time, proc->pid, time_in_prev_state,  "RUNNG", "BLOCK", ib, proc->TC-proc->CT);
                                            }
                                        }

                                        call_scheduler = true;
                                        
                                        
                                    }
            else if(trans_to ==TRANS_TO_PREEMPT)  {
                                        proc->cpu_burst_rem = proc->cpu_burst_rem  - time_in_prev_state;
                                        proc->CT = time_in_prev_state + proc->CT;
                                         proc->state = PREMPT;
                                        proc_running = NULL;

                                        if(verbose)
                                        {
                                            printf("%d %d %d: %s -> %s cb=%2d rem=%d prio=%d\n",current_time, proc->pid, time_in_prev_state, "RUNNG", "READY", proc->cpu_burst_rem, proc->TC-proc->CT, proc->dynamic_priority);
                                        }

                                       
                                        total_cpu_utilization = time_in_prev_state + total_cpu_utilization;
                                        scheduler->addProcess(proc);
                                        call_scheduler = true;
                                        
                                    }
        
        des->pop();
    

        if(call_scheduler)
        {
            scheduler_call(des);
          
        }
        
    }
}

double getPercentage( double a , int b)
{
 return (a*100)/b;
}

int main(int argc, char *argv[])
{
   
    int opt;
    bool Hasquantum = verbose = false;

    while((opt = getopt (argc, argv, "vtes:")) != -1)
    {
        
            if(opt =='v')
            verbose = true;
            else if(opt =='t')
            param_t = true;
            else if(opt =='e')
            param_e = true;          
            else if(opt =='s')
              {char temp;
                      if(optarg!=NULL)
                      {
                             if (optarg[0] == 'F')
                                {  scheduler_type="FCFS";
                                            scheduler = new FCFS_LCFS_RR();
                                         }
                                else if(optarg[0] == 'L')
                                {scheduler_type="LCFS";
                                            scheduler = new FCFS_LCFS_RR();
                                        }
                                else if(optarg[0] == 'S') {scheduler_type="SRTF";
                                            scheduler = new SRTF();
                                            }
                                else if(optarg[0] == 'R') { scheduler_type = "RR";
                                            scheduler = new FCFS_LCFS_RR();
                                            sscanf(optarg,"%c%d",&temp,&quantum);
                                            Hasquantum=true;
                                            }
                                else if(optarg[0] == 'P') {scheduler_type = "PRIO";
                                            sscanf(optarg,"%c%d:%d",&temp,&quantum,&max_prio);
                                            scheduler = new PRIO();
                                            Hasquantum=true;
                                            }
                                else if(optarg[0] == 'E') {scheduler_type = "PREPRIO";
                                            sscanf(optarg,"%c%d:%d",&temp,&quantum,&max_prio);
                                            scheduler = new PRIO();
                                            isPrePrio = Hasquantum = true;
                                            
                                            }
                                else if(optarg[0] == 'L')
                                { printf("\n Invalid scheduler name");
                                            return 1;}
                             
                      }
              }
                      
            else if(opt =='?') 
            if(optopt == 's')
            fprintf (stderr, "opt -%c requires an argument.\n", optopt);
                     
    }

    

    string fileName = argv[optind];
    string randFileName = argv[optind+1];

    readFile(argv[optind+1], 0);
    readFile(fileName, max_prio);

    DES des;
    for(int i=0; i<process_queue.size();i++)
    {
        Event event(process_queue[i].AT,&process_queue[i],TRANS_TO_READY);
        event.old_state = CREATED;
        des.putEvent(event);
    }

    simulation(&des);
    
    cout << scheduler_type;

    if(Hasquantum)
    {
        cout << " " << quantum;
    }    
    cout << endl;

    double avg_tt = ((double)total_tt/process_queue.size());
    double avg_cw = ((double)total_cw/process_queue.size());
    double process_queue_size = process_queue.size();

    for(int i=0; i<process_queue_size; i++)
    {
        process_queue.at(i).printInfo();
    }
   
    double cpu_utilization = getPercentage((double)total_cpu_utilization,last_event_finish);
    double io_utilization = getPercentage((double)total_io_utilization,last_event_finish);

    double throughput = (process_queue_size * 100.0)/last_event_finish;
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", last_event_finish, cpu_utilization, io_utilization, avg_tt, avg_cw, throughput);

}