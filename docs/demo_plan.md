# COMP4601 Arithmetic Coding Demo Plan

## Repository walkthrough
A brief walkthrough of the current repository will be given, showing off the following components:

1. The pure-software implementation developed from scratch. This resides in ./software/ and can be built on any system with gcc or cmake.

2. The replication HLS solution and host code, located in `/hardware/replication_full`.

3. The interleaved HLS solution and host code, located in `/hardware/interleaved`.

4. Our 'mcoder' HLS solution and host code, located currently in branch m_coderbb under `/brain_storming/mcoder`.
    - To be organised into main branch soon.

5. The working tANS solution, compiled for both a single 1x compute unit factor and 2x compute unit factor.
    - Results not shown in presentation due to fundamental algorithm differences.

## Running software and HLS solutions on the Kria
1. The naive c++ software will be run using dummy data to prove efficacy.
    - This is done with a round-trip decoding job that can be inspected for integrity.
    - Slow, but used as an ongoing reference point with 'clean' idiomatic c++ code.

2. Each HLS solution will be run.
    - Replication build using pre-built binaries on Rory's board.
    - Interleaved build using pre-built binaries on Rory's board.
    - 'mcoder' solutionbuild using Bryan's board.
    - tANS solution on Rory's board if time permits
