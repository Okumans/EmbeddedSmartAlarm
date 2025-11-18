#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <Arduino.h>

#include <functional>

// Simple task scheduler that prioritizes time-critical tasks
// High priority tasks run every loop, low priority tasks run at intervals
class TaskScheduler {
 public:
  enum Priority {
    PRIORITY_CRITICAL = 0,  // Run every loop (e.g., audio decoding)
    PRIORITY_HIGH = 1,      // Run every few ms (e.g., MQTT)
    PRIORITY_MEDIUM = 10,   // Run every 10ms
    PRIORITY_LOW = 100      // Run every 100ms
  };

 private:
  struct Task {
    std::function<void()> callback;
    Priority priority;
    unsigned long lastRun;
    unsigned long interval;  // milliseconds between runs
    bool enabled;
    const char* name;

    Task()
        : callback(nullptr),
          priority(PRIORITY_CRITICAL),
          lastRun(0),
          interval(0),
          enabled(true),
          name("") {}

    Task(std::function<void()> cb, Priority prio, const char* taskName)
        : callback(cb),
          priority(prio),
          lastRun(0),
          enabled(true),
          name(taskName) {
      // Set interval based on priority
      switch (prio) {
        case PRIORITY_CRITICAL:
          interval = 0;
          break;  // Every loop
        case PRIORITY_HIGH:
          interval = 5;
          break;  // Every 5ms
        case PRIORITY_MEDIUM:
          interval = 10;
          break;  // Every 10ms
        case PRIORITY_LOW:
          interval = 100;
          break;  // Every 100ms
      }
    }
  };

  static const int MAX_TASKS = 16;
  Task tasks[MAX_TASKS];
  int taskCount;

 public:
  TaskScheduler() : taskCount(0) {}

  // Add a task with specified priority
  bool addTask(std::function<void()> callback, Priority priority,
               const char* name) {
    if (taskCount >= MAX_TASKS) {
      Serial.println("[Scheduler] ERROR: Maximum tasks reached!");
      return false;
    }

    tasks[taskCount] = Task(callback, priority, name);
    taskCount++;

    Serial.printf(
        "[Scheduler] Task '%s' added (priority: %d, interval: %lums)\n", name,
        priority, tasks[taskCount - 1].interval);
    return true;
  }

  // Run all tasks based on their priority and timing
  void run() {
    unsigned long now = millis();

    // Process tasks in order (add critical tasks first for priority)
    for (int i = 0; i < taskCount; i++) {
      Task& task = tasks[i];

      if (!task.enabled || !task.callback) {
        continue;
      }

      // Check if it's time to run this task
      if (task.priority == PRIORITY_CRITICAL ||
          (now - task.lastRun >= task.interval)) {
        task.callback();
        task.lastRun = now;
      }
    }
  }

  // Enable/disable a task by name
  void enableTask(const char* name, bool enable) {
    for (int i = 0; i < taskCount; i++) {
      if (strcmp(tasks[i].name, name) == 0) {
        tasks[i].enabled = enable;
        Serial.printf("[Scheduler] Task '%s' %s\n", name,
                      enable ? "enabled" : "disabled");
        return;
      }
    }
  }

  // Print task status
  void printStatus() {
    Serial.println("\n[Scheduler] Task Status:");
    Serial.println("----------------------------------------");
    for (int i = 0; i < taskCount; i++) {
      Task& task = tasks[i];
      Serial.printf("  [%d] %s: %s (interval: %lums)\n", i, task.name,
                    task.enabled ? "enabled" : "disabled", task.interval);
    }
    Serial.println("----------------------------------------\n");
  }
};

#endif  // TASK_SCHEDULER_H
