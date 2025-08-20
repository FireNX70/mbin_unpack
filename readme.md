Compile with "g++ -std=c++23 unpack.cpp -o mbin_unpack.elf". It needs C++23
because I'm too lazy to write my own byteswap.

The CRC directory contains some tests you could also build and run.

Does what it says on the tin. Call it with "mbin_unpack.elf \[path\]". \[path\]
may be any number of paths.
