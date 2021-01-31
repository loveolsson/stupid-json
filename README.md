# StupidJSON
Incomplete, not tested, not maintained, just a test, don't use!

JSON parser that aims to do extremely fast document parsing, minimal allocations, and delay all value parsing until the value is read by the user.

During parsing, it builds a linked list of JSON nodes referencing the source buffer.

Nodes are allocated in blocks of 64 by the arena allocator, and is only released once the allocator is destructed.

Parsing a document with less than 64 nodes (element + keys) only does one allocation.
