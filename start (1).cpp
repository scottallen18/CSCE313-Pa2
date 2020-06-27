#include <linux/limits.h>
#include <fcntl.h>

#include <iostream>
#include <vector>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
using namespace std;


string prev_working_directory; 
vector<pid_t> children;

void printVector(vector<int> input){
    for(int i = 0; i < input.size(); i ++){
        cout << input[i] << endl;
    }
}

string frontTrim (string input){
    int i =0;
    while (i < input.size() && input [i] == ' ')
        i++;
    if (i < input.size())
        input = input.substr (i);
    else{
        return "";
    }
    return input;
}

string backTrim (string input){
    int i = input.size() - 1;
    while (i>=0 && input[i] == ' ')
        i--;
    if (i >= 0)
        input = input.substr (0, i+1);
    else
        return "";
    
    return input;
}

string trim (string input){
    //This function eliminates any whitespaces before and after the string
    input = frontTrim(input);
    input = backTrim(input);
    return input;
}

char** vec_to_char_array (vector<string> parts){
    char ** result = new char * [parts.size() + 1]; // add 1 for the NULL at the end
    for (int i=0; i<parts.size(); i++){
        // allocate a big enough string
        result [i] = new char [parts [i].size() + 1]; // add 1 for the NULL byte
        strcpy (result [i], parts[i].c_str());
    }
    result [parts.size()] = NULL;
    return result;
}

vector<string> split (string line, string separator=" "){
    //This function breaks up a string and puts it into a vector so that commmands
    //are easier to read
    vector<string> result;
	while (line.size()){
        size_t found = line.find(separator);
        if(line[0] == '\''){
        	//cout << "BEGIN: Found a quotation mark" << endl;
        	char curr;
			long count = 1;	        	
        	while(curr != '\'' && curr != '"'){
        		curr = line[count];
                //write unprintable ascii code instead to keep it safe
        		if(curr ==  '@'){
        			line[count] = ' ';
        		}
        		else if(curr == '^'){
        			line[count] = '|';
        		}
        		count ++;
        	}
        	string result_input = line.substr(1, count-2);
        	result.push_back(result_input);

        	line = line.substr(count);
        }
        else if(line[0] == '"'){
            //cout << "BEGIN: Found a quotation mark" << endl;
        	char curr;
			long count = 1;	        	
        	while(curr != '\'' && curr != '"'){
        		curr = line[count];
                //write unprintable ascii code instead to keep it safe
        		if(curr ==  '@'){
        			line[count] = ' ';
        		}
        		else if(curr == '^'){
        			line[count] = '|';
        		}
        		count ++;
        	}
        	string result_input = line.substr(1, count-2);
        	result.push_back(result_input);

        	line = line.substr(count);
        }
        else{
	        if (found == string::npos){
	            string lastpart = trim (line);
	            if (lastpart.size()>0){
	                result.push_back(lastpart);
	            }
	            break;
	        }
	        string segment = trim (line.substr(0, found));
	        line = line.substr (found+1);

	        if (segment.size() != 0){ 
	            result.push_back (segment);
			}
		}
    }
    return result;
}

int find_num_pipes(string input){
    int count = 0;
    for(int i = 0; i < input.length(); i ++){
        if (input[i] == '|') count ++;
    }
    return count;
}

void execute(string input){
    vector<string> commands = split(input);
    int write;
    int read;

    if(commands[0] == "cd"){
        if(commands[1] == "-"){
    		chdir(prev_working_directory.c_str());
    	}
    	else{
    		char curr_directory[PATH_MAX]; 
	        prev_working_directory = getcwd(curr_directory, sizeof(curr_directory));
	        chdir(commands[1].c_str());
    	}
    }
    
    for(int i = 0; i < commands.size(); i++){
    	if(commands[i] == ">"){
            //This symbol will mean we are writting to a file
            //We will store this in the descriptor table in the standard output
    		write= open(commands[i+1].c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(write,1);
            commands.erase(commands.begin()+i+1);
            commands.erase(commands.begin()+i);
    	}
    	if (commands[i] == "<"){
            //This symbol will mean we are reading to a file
            //We will store this in the descriptor table int the standard input
            read = open(commands[i+1].c_str(), O_RDONLY,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            dup2(read, 0);
            commands.erase(commands.begin()+i);
        }
    }
    char** args = vec_to_char_array (commands);// convert vec<string> into an array of char*
    execvp (args[0], args);
}

int main (){
    int stdin = dup(0);
    int stdout = dup(1);

    bool run = true;
    while (run){
        char curr_directory[PATH_MAX]; 
        cout << getcwd(curr_directory, sizeof(curr_directory)) << "$ "; 
        string inputline;
        getline (cin, inputline);   // get a line from standard input
        if (inputline == string("exit")){
            run = false;
            break;
        }

        vector<string> pipeCommands= split(inputline, "|");
        int num_of_pipes = pipeCommands.size();

      // Using the piazza post as a guideline
        for(int i = 0; i < num_of_pipes; i ++){
            int fd[2];
            pipe(fd);
            
            if(i != 0){
                //If command is not first:
		        //Rewire stdin to read end of pipe
                dup2(stdin, fd[1]);
            }
            pid_t pid = fork ();
            if (pid == 0){ 
                //child process
                if(i!= num_of_pipes - 1){
                    //rewrite stdin to read end of pipe
                    dup2(fd[0], stdin);
                }
                execute(pipeCommands[i]);
            }
            else{
                waitpid(pid,0,0);

            }
            dup2(fd[0], 0);
            close(fd[1]);
        }

        //for(int i = 0; i < children.size(); i ++){
         //   waitpid(children[i], 0 ,0);
        //}
        //rewire clone of stdin back to stdin
        dup2(stdin, 0);
        
    }
}


