// Verilator's runtime calls sc_time_stamp() for the simulation time of a model that has no time callback.
// With GCC and Clang the Verilator headers declare it weak, so it may be missing or defined more than once.
// MSVC has no weak symbols: a program that links a verilated model must define it, and exactly once.
double sc_time_stamp()
{
    return 0;
}
