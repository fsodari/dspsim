# Description of the delta cycle process

- evaluate all models in the update queue
- update signals. If a signal value changes, notify subscribers
- repeat until update queue is empty. Then advance to next time step.

# Signals, Ports, and Modules
- If a signal changes, it notifies a port
- A port will queue its event subscribers
- Modules will be queued for evaluation when a port in its sensitivity list changes

# Clocks and waiting
- waiting will add a time step to the time_step_priority_queue, and queues an event in the update queue
- The simulation advances to the next time step, subtracts the last step from all time steps in queue, then evaluates all models in update queue
- Clocks will wait on every half period

# Update queue
- void update_queue_push_back(Model *model);

# Event queue.
- Only update each module once per delta cycle if multiple port values changed.
- 

# Signal notifications.
```
// Module constructor
sensitivity_list << a.changed() << b.changed() << c.posedge();
void add_event(sensitivity_list, event_subscribers)
{
    event_subscribers.append(parent)
}

// Signal
void sync()
{
    if (q != d)
    {
        for (port : subscribers)
        {
            port.notify()
        }
    }
}

// Port
void notify()
{
    for (module : changed_event_subscribers)
    {
        update_queue_append(module);
    }
    for (module : posedge_event_subscribers)
    {
        update_queue_append(module);
    }
    ...
}
```