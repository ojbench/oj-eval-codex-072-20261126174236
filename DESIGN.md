# RV32I five-stage simulator

The simulator reads hexadecimal memory bytes with `@address` markers. It starts
at PC zero with zeroed registers and memory. The benchmark's `0x0ff00513`
termination instruction prints the low eight bits of a0 after older instructions
have retired.

The five stages are fetch, decode, execute, memory, and writeback. Four explicit
pipeline registers separate the stages. Decode stalls on dependencies against
execute and memory; writeback occurs before register reads in the same cycle.
Stalls retain the fetched instruction and insert a bubble into execute. Taken
branches and jumps resolve in execute and discard younger instructions. Stores
and loads execute in program order in the memory stage. Termination drains older
instructions before reporting a0.

All RV32I integer arithmetic uses unsigned 32-bit values for defined wraparound.
Signed comparisons and arithmetic right shifts explicitly implement two's
complement semantics. Loads and stores use little-endian byte accesses, including
unaligned accesses. Memory grows on writes and unread locations contain zero.

Runtime is linear in simulated cycles (a bounded number per retired instruction),
plus input size. Memory use is linear in the highest written address, with an
initial 1 MiB allocation and a 256 MiB address limit.

Build and test:

```sh
make
make test
```

The test-data path listed in README.md was absent in the supplied environment.
Local regression tests cover instruction semantics, dependencies, memory widths,
branch flushing, backward loops, jumps, and randomized arithmetic streams.
