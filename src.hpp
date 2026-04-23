#pragma once

#include "Task.hpp"
#include <vector>

class TaskNode {
    friend class TimingWheel;
    friend class Timer;
public:
    TaskNode() : task(nullptr), next(nullptr), prev(nullptr), time(0) {}
    TaskNode(Task* t, int tm) : task(t), next(nullptr), prev(nullptr), time(tm) {}
    
private:
    Task* task;
    TaskNode* next, *prev;
    int time;
};

class TimingWheel {
    friend class Timer;
public:
    TimingWheel(size_t size, size_t interval) : size(size), interval(interval), current_slot(0) {
        slots = new TaskNode*[size];
        for (size_t i = 0; i < size; ++i) {
            slots[i] = nullptr;
        }
    }
    
    ~TimingWheel() {
        for (size_t i = 0; i < size; ++i) {
            if (slots[i]) {
                TaskNode* start = slots[i];
                TaskNode* cur = start;
                do {
                    TaskNode* next = cur->next;
                    delete cur;
                    cur = next;
                } while (cur != start);
            }
        }
        delete[] slots;
    }
    
    void addTaskNode(TaskNode* node, size_t slot) {
        if (slots[slot] == nullptr) {
            slots[slot] = node;
            node->next = node;
            node->prev = node;
        } else {
            TaskNode* head = slots[slot];
            node->next = head;
            node->prev = head->prev;
            head->prev->next = node;
            head->prev = node;
        }
    }
    
    void removeTaskNode(TaskNode* node, size_t slot) {
        if (node->next == node) {
            slots[slot] = nullptr;
        } else {
            if (slots[slot] == node) {
                slots[slot] = node->next;
            }
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }
        node->next = nullptr;
        node->prev = nullptr;
    }
    
private:
    const size_t size, interval;
    size_t current_slot;
    TaskNode** slots;
};

class Timer {
public:    
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    Timer() : seconds_wheel(60, 1), minutes_wheel(60, 60), hours_wheel(24, 3600) {
        current_time = 0;
    }

    ~Timer() {}

    TaskNode* addTask(Task* task) {
        size_t delay = task->getFirstInterval();
        TaskNode* node = new TaskNode(task, 0);
        addTaskToWheel(node, delay);
        return node;
    }

    void cancelTask(TaskNode *p) {
        if (p == nullptr) return;
        
        // Find which wheel and slot the task is in
        for (size_t i = 0; i < 60; ++i) {
            if (seconds_wheel.slots[i]) {
                TaskNode* cur = seconds_wheel.slots[i];
                TaskNode* start = cur;
                do {
                    if (cur == p) {
                        seconds_wheel.removeTaskNode(p, i);
                        delete p;
                        return;
                    }
                    cur = cur->next;
                } while (cur != start);
            }
        }
        
        for (size_t i = 0; i < 60; ++i) {
            if (minutes_wheel.slots[i]) {
                TaskNode* cur = minutes_wheel.slots[i];
                TaskNode* start = cur;
                do {
                    if (cur == p) {
                        minutes_wheel.removeTaskNode(p, i);
                        delete p;
                        return;
                    }
                    cur = cur->next;
                } while (cur != start);
            }
        }
        
        for (size_t i = 0; i < 24; ++i) {
            if (hours_wheel.slots[i]) {
                TaskNode* cur = hours_wheel.slots[i];
                TaskNode* start = cur;
                do {
                    if (cur == p) {
                        hours_wheel.removeTaskNode(p, i);
                        delete p;
                        return;
                    }
                    cur = cur->next;
                } while (cur != start);
            }
        }
    }

    std::vector<Task*> tick() {
        std::vector<Task*> result;
        current_time++;
        
        size_t old_slot = seconds_wheel.current_slot;
        seconds_wheel.current_slot = (seconds_wheel.current_slot + 1) % 60;
        
        // Check if we need to cascade
        if (seconds_wheel.current_slot == 0) {
            cascadeMinutes();
        }
        
        // Process tasks in the current slot
        size_t slot = seconds_wheel.current_slot;
        if (seconds_wheel.slots[slot]) {
            TaskNode* cur = seconds_wheel.slots[slot];
            TaskNode* start = cur;
            std::vector<TaskNode*> to_process;
            
            do {
                TaskNode* next = cur->next;
                if (cur->time == 0) {
                    to_process.push_back(cur);
                }
                cur = next;
            } while (cur != start);
            
            for (TaskNode* node : to_process) {
                seconds_wheel.removeTaskNode(node, slot);
                result.push_back(node->task);
                
                // Re-add periodic task
                size_t period = node->task->getPeriod();
                if (period > 0) {
                    addTaskToWheel(node, period);
                } else {
                    delete node;
                }
            }
        }
        
        return result;
    }

private:
    TimingWheel seconds_wheel;
    TimingWheel minutes_wheel;
    TimingWheel hours_wheel;
    size_t current_time;
    
    void addTaskToWheel(TaskNode* node, size_t delay) {
        if (delay < 60) {
            size_t slot = (seconds_wheel.current_slot + delay) % 60;
            node->time = (seconds_wheel.current_slot + delay) / 60;
            seconds_wheel.addTaskNode(node, slot);
        } else if (delay < 3600) {
            size_t minutes = delay / 60;
            size_t seconds = delay % 60;
            size_t slot = (minutes_wheel.current_slot + minutes) % 60;
            node->time = seconds;
            minutes_wheel.addTaskNode(node, slot);
        } else {
            size_t hours = delay / 3600;
            size_t remaining = delay % 3600;
            size_t slot = (hours_wheel.current_slot + hours) % 24;
            node->time = remaining;
            hours_wheel.addTaskNode(node, slot);
        }
    }
    
    void cascadeMinutes() {
        size_t old_slot = minutes_wheel.current_slot;
        minutes_wheel.current_slot = (minutes_wheel.current_slot + 1) % 60;
        
        if (minutes_wheel.current_slot == 0) {
            cascadeHours();
        }
        
        size_t slot = minutes_wheel.current_slot;
        if (minutes_wheel.slots[slot]) {
            TaskNode* cur = minutes_wheel.slots[slot];
            TaskNode* start = cur;
            std::vector<TaskNode*> to_move;
            
            do {
                TaskNode* next = cur->next;
                to_move.push_back(cur);
                cur = next;
            } while (cur != start);
            
            for (TaskNode* node : to_move) {
                minutes_wheel.removeTaskNode(node, slot);
                size_t delay = node->time;
                addTaskToWheel(node, delay);
            }
        }
    }
    
    void cascadeHours() {
        hours_wheel.current_slot = (hours_wheel.current_slot + 1) % 24;
        
        size_t slot = hours_wheel.current_slot;
        if (hours_wheel.slots[slot]) {
            TaskNode* cur = hours_wheel.slots[slot];
            TaskNode* start = cur;
            std::vector<TaskNode*> to_move;
            
            do {
                TaskNode* next = cur->next;
                to_move.push_back(cur);
                cur = next;
            } while (cur != start);
            
            for (TaskNode* node : to_move) {
                hours_wheel.removeTaskNode(node, slot);
                size_t delay = node->time;
                addTaskToWheel(node, delay);
            }
        }
    }
};
