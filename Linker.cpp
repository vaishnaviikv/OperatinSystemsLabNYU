//In this program :
//Implementation : a 2 pass linker that combines assembler code in a fictitious assembler language into a single module.
// Developed by Vaishnavi V

#include <iostream>
#include <map>
#include <fstream>
#include <regex>
#include <set>
#include <vector>
#include <iomanip>

using namespace std;

map<string, int> symbol_map;
map<int, int> address_map;
map<string, int> symbol_to_module;
multimap<string, int> symbol_by_module;

int module_id=0;
int current_line=1, char_offset=1, previous_line=1;
int base_address=0;
int parse_index=0;
int previous_offset=1;
int instructions_per_module=0;
int instruction_count=0;
int instruction_index=0;
bool execution_halted=false;
bool module_flag=true;
bool second_pass=false;
bool execution_flag=true;
string token_delimiters = " \t\n";
string token_value;
string buffer;
string pass_order;
const string delimiter=" \t\n";
vector<string> used_symbols;
vector<string> module_warnings;
set<string> repeated_symbols;
set<int> symbols_used;
set<string> all_symbols;

static string errstr[] = {
    "NUM_EXPECTED",
    "SYM_EXPECTED",
    "MARIE_EXPECTED",
    "SYM_TOO_LONG",
    "TOO_MANY_DEF_IN_MODULE",
    "TOO_MANY_USE_IN_MODULE",
    "TOO_MANY_INSTR"};
    

void display_symbol_table();
int tokenize(const string&);
void add_symbol(string, int);
int parse_int(string);
void process_list(string,bool);
void pass(string,string);
int process_instruction_list(string);
string extract_token( string, const string&);
void update_position(string, int);
void handle_parse_error(int);
void check_warnings(int);
void populate_memory_map(int , int );
void check_symbol_usage();

int tokenize(const string& input_stream)
{
    int end;
    while(parse_index < input_stream.length())
    {
        end = input_stream.find_first_of(token_delimiters, parse_index);
        if(end == -1)
        {
            end = input_stream.size();
        }
        if(end != parse_index)
        {
            return(end);
        }
        update_position(input_stream, end);
        parse_index = end + 1;
    }
    return(-1);
}

void add_symbol(string symbol, int value) {
    // Use emplace to check and insert in one operation to avoid double lookup
    auto result = symbol_map.emplace(symbol, 0); // Initialize with zero initially

    if (result.second) { // Check if insertion took place
        int base = 0;
        auto it = address_map.find(module_id);
        if (it != address_map.end()) {
            base = it->second;
        }

        // Update the mapped value with the correct base + value since we initialized to 0
        result.first->second = base + value;

        // Insert into supporting maps
        symbol_to_module.emplace(symbol, module_id);
        symbol_by_module.emplace(symbol, module_id);
    } else {
        // If the symbol was already in the map, handle the redefinition warning
        repeated_symbols.insert(symbol);
        symbol_by_module.emplace(symbol, module_id);
        string log = "Warning: Module " + to_string(module_id) + ": " + symbol + " redefinition ignored\n";
        module_warnings.push_back(log);
    }
}


int parse_int(string input_stream) {
    int end = tokenize(input_stream);

    // Early return for parse index exceeding input size
    if (parse_index >= input_stream.size()) {
        if (module_flag) {
            execution_halted = true;
            return -1;
        }
        handle_parse_error(0);
        return -1; 
    }

    string val = input_stream.substr(parse_index, end - parse_index);
    update_position(input_stream, end);
    regex digitPattern("^[0-9]+$");

    if (!regex_match(val, digitPattern)) {
        handle_parse_error(0);
        return -1; 
    }

    int actual_value = stoi(val);
    token_value = to_string(actual_value);
    parse_index = end + 1;
    return actual_value;
}


string extract_token(string input_stream, const string& type) {
    int end = tokenize(input_stream);
    if (parse_index < input_stream.size()) {
        string val = input_stream.substr(parse_index, end - parse_index);
        token_value = val;
        update_position(input_stream, end);
        
        // Decide the validation based on type
        if (type == "symbol") {
            if (!regex_match(val, regex("[a-zA-Z][a-zA-Z0-9]*")) || val.size() >= 17) {
                handle_parse_error(1); // Error for symbol
            }
        } else if (type == "marie") {
            regex validChars("[MARIE]");
            if (!(regex_match(val, validChars) && val.length() == 1)) {
                handle_parse_error(2); // Error for MARIE
            }
        }
        
        parse_index = end + 1;
        return val;
    } else {
        handle_parse_error(type == "symbol" ? 1 : 2);
        return "";
    }
}


void process_list(string input_stream, bool isDefinitionList) {
  
    if(execution_halted){
            return;
            }
    int count = parse_int(input_stream);  // Get either the number of definitions or the number of uses

    if (isDefinitionList) {
        // Handle the definition list logic
        if (!second_pass) {
            if (address_map.find(module_id) == address_map.end()) {
                address_map.insert(make_pair(module_id, instructions_per_module + base_address));
            }
            
            if (count >= 17) {
                handle_parse_error(4);
            }
            previous_offset = token_value.size() + previous_offset ;
            
            for (int i = 0; i < count; i++) {
                string symbol = extract_token(input_stream,"symbol");
                previous_offset = token_value.size() + previous_offset ;
                int value = parse_int(input_stream);
                previous_offset = token_value.size() + previous_offset ;
                add_symbol(symbol, value);
            }
        } else {
            // In the second pass, we still need to parse the symbols and values
            // but do not add them to the symbol table.
            
            for (int i = 0; i < count; i++) {
                string symbol = extract_token(input_stream,"symbol");
                int value = parse_int(input_stream);
            }
        }
    } else {
        // Handle the use list logic
        
        if (count > 16) {
            handle_parse_error(5);
        }
        previous_offset = token_value.size() + previous_offset ;
        for (int i = 0; i < count; i++) {
            string symbol = extract_token(input_stream,"symbol");
            used_symbols.push_back(symbol);
            previous_offset = token_value.size() + previous_offset ;
        }
    }
}

void error_line()
{
    cout << setw(3) << setfill('0') << instruction_index << ": " << 9999 << " Error: " << "Illegal opcode; treated as 9999" << endl;
}

void populate_memory_map(int opcode, int result)
{
    cout << setw(3) << setfill('0') << instruction_index;        // Formatting the key part
                    cout << ": " << setw(4) << setfill('0') << (opcode * 1000) + result ;
}

int process_instruction_list(string input_string)
{
    if(execution_halted){
            return(0);
        }
    int instruction_count = parse_int(input_string);

    if(!second_pass)
    {   int width = 512 - base_address;
        if(instruction_count > width){
            handle_parse_error(6);
        }
        previous_offset = token_value.size() + previous_offset ;
        
        for(int i = 0; i < instruction_count; i++)
        {
            string mode = extract_token(input_string, "marie");
            previous_offset = token_value.size() + previous_offset ;
            
            int instruction = parse_int(input_string);
            previous_offset = token_value.size() + previous_offset ;
        }
        return(instruction_count);
    }
    else
    {
        if(module_id == 0)
        {
            cout << "Memory Map" << endl;
        }
        
        for(int i = 0; i < instruction_count; i++)
        {
            string mode = extract_token(input_string,"marie");
            int instruction = parse_int(input_string);
            int opcode = instruction / 1000;
            int operand = instruction % 1000;
            switch(mode[0]) 
           { case 'R': {
                
                if(opcode > 9)
                {
                    error_line();
                }
                else if(operand >= instruction_count)
                {   int result = address_map[module_id];
                    populate_memory_map(opcode, result);
                    cout << " Error: " << " Relative address exceeds module size; relative zero used" << endl;
                }
                else
                {
                    int result = address_map[module_id] + operand;
                    populate_memory_map(opcode, result);
                    cout << endl;
                }
                
            }break;
            case 'M':{
                
                
                if(opcode > 9)
                {
                     error_line();
                }
                else if (address_map.find(operand) == address_map.end()) {
                    
                    cout << setw(3) << setfill('0') << instruction_index;        // Formatting the key part
                    cout << ": " << setw(4) << setfill('0') << (opcode * 1000) << " Error: " << "Illegal module operand ; treated as module=0" << endl;
                    
                    
                }
                else{
                    int result = address_map[operand];
                    populate_memory_map(opcode, result);
                    cout << endl;
                    
                    
                    
                }
                
            }break;
            case 'A':
            {
               
                if(opcode > 9)
                {
                    error_line();
                }
                else if(operand >= 512)
                {
                    cout << setw(3) << setfill('0') << instruction_index;        // Formatting the key part
                    cout << ": " << setw(4) << setfill('0') << (opcode * 1000) << " Error: " << "Absolute address exceeds machine size; zero used" << endl;  // Formatting the value part
                    
                    
                }
                else{
                    int result = operand;
                    populate_memory_map(opcode, result);
                    cout << endl;  // Formatting the value part
                    
                }
                
            }break;
            case 'I':
            {
                if(opcode > 9)
                {
                    error_line();
                }
                else if(operand >= 900)
                {   int result =999;
                   populate_memory_map(opcode, result);
                   cout << " Error: " << "Illegal immediate operand; treated as 999" << endl;  // Formatting the value part
                }
                else
                {   int result = operand;
                   populate_memory_map(opcode, result);
                   cout << endl;  // Formatting the value part
                    
                }
            }break;
           case 'E': 
            {
                int abs_address_reference_var = 0;
                if(opcode > 9)
                {
                     error_line();
                }
                else if(operand >= used_symbols.size())
                {
                    abs_address_reference_var = 0;
                    int result = abs_address_reference_var;
                    populate_memory_map(opcode, result);
                    cout << " Error: " << "External operand exceeds length of uselist; treated as relative=0" << endl;  // Formatting the value part
                }
                
                else if(symbol_map.find(used_symbols[operand]) != symbol_map.end())
                {
                    string reference_var = used_symbols[operand];
                    symbols_used.insert(operand);
                    all_symbols.insert(used_symbols[operand]);
                    abs_address_reference_var = symbol_map[reference_var];
                    int result = abs_address_reference_var;
                   populate_memory_map(opcode, result);
                   cout << endl; 
                     
                    
                }
                else{
                    symbols_used.insert(operand);
                    all_symbols.insert(used_symbols[operand]);
                    abs_address_reference_var = 0;
                    int result = abs_address_reference_var;
                    populate_memory_map(opcode, result);
                    cout << " Error: " << used_symbols[operand]<< " is not defined; zero used" << std::endl; // Formatting the value part

                }
                
            } break;
           }
            instruction_index = instruction_index + 1;
        }
        return(instruction_count);
        
    }
    
}


void pass(string input_string, string pass_order)
{
    // Reset line tracking variables
    current_line = 1;
    previous_line = 1;
    char_offset = 1;
    previous_offset = 1;

    // Reset parsing and instruction indices
    parse_index = 0;
    instruction_index = 0;
    instruction_count = 0;

    // Reset module tracking variables
    module_id = 0;
    base_address = 0;
    instructions_per_module = 0;

    // Reset flags
    module_flag = true;
    execution_halted = false;
    
    while(parse_index < input_string.size())
    {
        process_list(input_string, true);
        process_list(input_string,false);
        if(pass_order=="one")
        { int module_instruction_count = process_instruction_list(input_string);
        check_warnings(module_instruction_count);
        vector<string>::iterator itr;
        for(itr = module_warnings.begin(); itr != module_warnings.end(); ++itr){
            cout << itr->c_str();
        }
        module_warnings.clear();
        base_address = base_address + module_instruction_count;
        
    }
    else if(pass_order=="two"){
        process_instruction_list(input_string);
        for(int i = 0; i < used_symbols.size(); i++)
        {
            if(symbols_used.find(i) == symbols_used.end())
            {
                cout << "Warning: Module " << module_id << ": uselist[" << i << "]=" << used_symbols[i] << " was not used\n";

            }
        }
        symbols_used.clear();
    }
    used_symbols.clear();
     module_id = module_id + 1;
    }
}


void check_warnings(int instruction_count) {
    for (auto itr = symbol_by_module.begin(); itr != symbol_by_module.end(); ++itr) {
        auto symbol_iter = symbol_map.find(itr->first);
        if (symbol_iter != symbol_map.end()) {
            int symbol_location = symbol_iter->second;
            if (itr->second == module_id && symbol_location >= base_address + instruction_count) {
                string log = "Warning: Module " + to_string(module_id) + ": " +
                             itr->first + "=" + to_string(symbol_location - address_map[module_id]) +
                             " valid=[0.." + to_string(instruction_count - 1) + "] assume zero relative\n";
                module_warnings.push_back(log);
                // Update the symbol's location to the base address of the module
                symbol_iter->second = base_address;
            }
        }
    }
}

void handle_parse_error(int errcode) {

printf("Parse Error line %d offset %d: %s\n", previous_line, previous_offset, errstr[errcode].c_str());
std::exit(EXIT_FAILURE);
}


void update_position(string input_stream, int end) {
    // Update the offset for characters processed so far
    previous_offset = char_offset;
    char_offset = char_offset + end - parse_index + 1;  // Adjust char_offset based on the characters processed

    // Check if the current character is within the bounds of the input stream
    if (end < input_stream.size()) {
        previous_line = current_line;  // Backup the current line before possibly incrementing

        // If the character at the 'end' position is a newline, update line and reset char_offset
        if (input_stream[end] == '\n') {
            current_line++;
            char_offset = 1;  // Reset to start at new line
        }
    }
}


void display_symbol_table()
{
cout << "Symbol Table" << endl;
    for (const auto& pair : symbol_map) {
        cout << pair.first << "=" << pair.second;
        if(repeated_symbols.find(pair.first) != repeated_symbols.end()){
                    cout << " Error: This variable is multiple times defined; first value used";
                }
        cout << '\n';
    }
cout << '\n';
}

void check_symbol_usage() {
    bool check = true;
    for (const auto& pair : symbol_map) {
        if (all_symbols.find(pair.first) == all_symbols.end()) {  // Check if the symbol was never used
            if (check) {
                cout << endl;  // Ensuring the first warning starts on a new line
                check = false;  // Toggle the flag after the first unused symbol warning
            }
            // Output the warning message for unused symbols
            cout << "Warning: Module " << symbol_to_module[pair.first] << ": " << pair.first << " was defined but never used\n";
        }
    }
}


int main(int argc, const char * argv[]) {

ifstream inputFile(argv[1]);

string temp_string="";
string input_string="";
while(getline(inputFile,temp_string))
{
    input_string = input_string + temp_string + "\n";
}

// Handle Case : If input is empty or contains only whitespaces and newline characters
if (regex_match(input_string, regex("^[\t\n]*$"))) {
    return 1;
}

pass(input_string, "one");

//display symbol table
display_symbol_table();
    
// Clearing the referenced_variable from previous pass
used_symbols.clear();
second_pass = true;

pass(input_string,"two");

check_symbol_usage();

return 1;
}

