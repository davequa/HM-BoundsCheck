#Thread-Safe Libraries
These libraries are the standard thread-safe libraries built to accommodate for (almost) all possible configurations. The libraries actually used in the instrumentation-skeleton are inlined and optimised to work for the SPEC2006 benchmarking system, where all traces of thread-safety measures (mutexes, etc) have been removed. The versions here are not inlined, resulting in a very large runtime overhead. Inlining these libraries is still on the to-do list (future work).

Overhead (approximately) doubles (on benchmarks in SPEC2006) when not inlining code, making these libraries unusable for any actual production work. Only testing work is done with these, and until they are made to work with code inlining, they should not be used for other purposes.
