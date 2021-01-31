# StupidJSON
Incomplete, not tested, not compliant, not maintained, just a test, don't use!

JSON parser that aims to do extremely fast document parsing, minimal allocations, and delay all value parsing until the value is read by the user.

During parsing, it builds a linked list of JSON nodes referencing the source buffer.

Nodes are allocated in incrementally growing blocks by the arena allocator, and is only released once the allocator is destructed.