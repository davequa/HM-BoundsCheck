# HM-BoundsCheck
HM-BoundsCheck is a bounds checking framework based on the AddressSanitizer and Light-weight Bounds Checking tools. It incorporates the detection of spatial memory errors in heap memory through the usage of shadow memory (based on the AddressSanitizer implementation) and the optimal checking method using a 'fast' and 'slow' path (based on the Light-weight Bounds Checking implementation).

Furthermore, HM-BoundsCheck contains a configuration which allows for the use of alternative to shadow memory. This framework, DHash, contains the same functionalities as HM-BoundsCheck, but uses a hash table-based implementation to record the per-byte/per-object addressability of all memory allocations.

The current implementation still requires implementations for stack and global memory, and must be generalised to easily work on all types of platforms. It has been tested on MacOS and Linux (Ubuntu), but has not been confirmed to work on Windows.
