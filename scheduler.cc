#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

struct Task {
    int pid;
    int arrival;
    int burst;
    int remaining;
    int inputOrder;
    int start = -1;
    int completion = -1;
};

//Prints the command line message.
static void printUsage(const char *program) { 
    cerr << "Usage: " << program << " input_file [FCFS|RR|SJF] [time_quantum]\n";
}

//Prints the final results in a simple text table.
//PID | Arrival Time | Start Time | End Time | Running Time | Waiting Time
static void printResults(const vector<Task> &tasks, const vector<int> &completionOrder, const string &algorithm, double totalWaiting) {
    cout << left << setw(8)  << "PID" << setw(15) << "Arrival Time" << setw(13) << "Start Time" << setw(11) << "End Time" << setw(14) << "Running Time" << setw(14) << "Waiting Time" << '\n';
    cout << string(75, '-') << '\n';

    //Print processes in the order in which they completed.
    for (int index : completionOrder) {
        const Task &task = tasks[index];

        //Waiting time
        int waiting = task.completion - task.arrival - task.burst;
        cout << left << setw(8)  << task.pid << setw(15) << task.arrival << setw(13) << task.start << setw(11) << task.completion << setw(14) << task.burst << setw(14) << waiting << '\n';
    }

    //Calculate and print the average waiting time.
    double averageWaiting = totalWaiting / static_cast<double>(tasks.size());
    cout << "\nAverage Waiting Time: " << fixed << setprecision(2) << averageWaiting << " ms\n";
}

int main(int argc, char *argv[]) {

    //Checks command-line arguments
    if (argc != 3 && argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    //Convert the algorithm name to uppercase
    string algorithm = argv[2];
    transform(algorithm.begin(), algorithm.end(), algorithm.begin(),[](unsigned char c) {return static_cast<char>(toupper(c));});

    //Error check
    if (algorithm != "FCFS" && algorithm != "RR" && algorithm != "SJF") {
        cerr << "Error: algorithm must be FCFS, RR, or SJF.\n";
        printUsage(argv[0]);
        return 1;
    }

    //Reads the Round Robin time quantum
    int quantum = 0;

    //Error check
    if (algorithm == "RR") {
        if (argc != 4) {
            cerr << "Error: RR requires a time quantum.\n";
            printUsage(argv[0]);
            return 1;
        }

        try {
            size_t parsed = 0;
            quantum = stoi(argv[3], &parsed);
            if (parsed != string(argv[3]).size() || quantum <= 0) {
                throw invalid_argument("invalid quantum");
            }
        } catch (const exception &) {
            cerr << "Error: time quantum must be a positive integer.\n";
            return 1;
        }

    } else if (argc != 3) {
        cerr << "Error: time quantum is only valid for RR.\n";
        printUsage(argv[0]);
        return 1;
    }

    //Opens and read the input file
    ifstream input(argv[1]);

    //Error check
    if (!input) {
        cerr << "Error: could not open input file '" << argv[1] << "'.\n";
        return 1;
    }

    vector<Task> tasks;

    int pid;
    int arrival;
    int burst;

    while (input >> pid >> arrival >> burst) {
        //Error check
        if (arrival < 0 || burst <= 0) {
            cerr << "Error: arrival times must be nonnegative and burst times positive.\n";
            return 1;
        }

        //Every process must have a unique PID.
        if (any_of(tasks.begin(), tasks.end(),[pid](const Task &task) {return task.pid == pid;})) {
            cerr << "Error: duplicate process ID " << pid << ".\n";
            return 1;
        }

        tasks.push_back({pid, arrival, burst, burst, static_cast<int>(tasks.size()), -1, -1});
    }

    //Error checks
    if (!input.eof()) {
        cerr << "Error: input must contain integer records: pid arrival_time burst_time.\n";
        return 1;
    }

    if (tasks.empty()) {
        cerr << "Error: input file contains no tasks.\n";
        return 1;
    }

    //Sort tasks by arrival time
    stable_sort(tasks.begin(), tasks.end(), [](const Task &a, const Task &b) {
        if (a.arrival != b.arrival) {
            return a.arrival < b.arrival;
        }
        return a.inputOrder < b.inputOrder;
    });

    //Set up the simulation
    cout << algorithm;
    if (algorithm == "RR") {
        cout << " (quantum " << quantum << " ms)";
    }
    cout << ":\n";
    int time = 0;
    size_t next = 0;
    int completed = 0;
    double totalWaiting = 0.0;

    //Stores processes in the order they finish.
    vector<int> completionOrder;

    //FCFS and SJF simulation
    if (algorithm == "FCFS" || algorithm == "SJF") {
        vector<int> ready;

        while (completed < static_cast<int>(tasks.size())) {

            //Add every process that has arrived by the current time to the ready queue
            while (next < tasks.size() && tasks[next].arrival <= time) {
                ready.push_back(static_cast<int>(next));
                ++next;
            }

            //If nothing is ready, jump directly to the next arrival
            if (ready.empty()) {
                cout << "Time " << time << ": idle\n";
                time = tasks[next].arrival;
                continue;
            }

            size_t choice = 0;

            if (algorithm == "SJF") {
                for (size_t i = 1; i < ready.size(); ++i) {
                    const Task &candidate = tasks[ready[i]];
                    const Task &current = tasks[ready[choice]];

                    if (candidate.burst < current.burst ||
                        (candidate.burst == current.burst &&
                         candidate.inputOrder < current.inputOrder)) {
                        choice = i;
                    }
                }
            }

            //Remove the selected process from the ready queue.
            int index = ready[choice];
            ready.erase(ready.begin() + static_cast<ptrdiff_t>(choice));
            Task &task = tasks[index];

            //Time calculations
            task.start = time;
            int waiting = task.start - task.arrival;
            //cout << "Time " << time << "-" << time + task.burst << ": P" << task.pid << " running\n";

            // Run the process for its entire burst.
            time += task.burst;
            task.remaining = 0;
            task.completion = time;
            totalWaiting += waiting;
            ++completed;

            //Record completion order
            completionOrder.push_back(index);
            //cout << "Time " << time << ": P" << task.pid << " completed\n";
        }
    }

    //Round Robin simulation
    else {
        queue<int> ready;
        while (completed < static_cast<int>(tasks.size())) {

            //Add newly arrived processes to the queue.
            while (next < tasks.size() && tasks[next].arrival <= time) {
                ready.push(static_cast<int>(next));
                ++next;
            }

            //If the queue is empty, CPU is idle until the next arrival.
            if (ready.empty()) {
                //cout << "Time " << time << ": idle\n";
                time = tasks[next].arrival;
                continue;
            }

            //Select the process at the front of the queue.
            int index = ready.front();
            ready.pop();

            Task &task = tasks[index];

            //Record only the first time the process runs.
            if (task.start == -1) {task.start = time;}

            //Run for either the entire remaining burst or one quantum, whichever is smaller
            int runFor = min(quantum, task.remaining);
            //cout << "Time " << time << "-" << time + runFor << ": P" << task.pid << " running\n";
            time += runFor;
            task.remaining -= runFor;

            //Add processes that arrived during this time slice
            while (next < tasks.size() && tasks[next].arrival <= time) {
                ready.push(static_cast<int>(next));
                ++next;
            }

            if (task.remaining == 0) {
                //Process has finished
                task.completion = time;
                int waiting = task.completion - task.arrival - task.burst;
                totalWaiting += waiting;
                ++completed;
                completionOrder.push_back(index);
                //cout << "Time " << time << ": P" << task.pid << " completed\n";
            } else {
                //Process did not finish, so put it at the back
                ready.push(index);
                //cout << "Time " << time << ": P" << task.pid << " preempted; " << task.remaining << " ms remaining\n";
            }
        }
    }

    //Print the final results table
    printResults(tasks, completionOrder, algorithm, totalWaiting);
    return 0;
}
