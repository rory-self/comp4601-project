## Building
```bash
cd software
# Cmake must be installed, use g++ otherwise
cmake -B build
make -C build
```

## Running
```bash
cd software

# For timing operations with dummy test data
./build/arithmetic_coder --timing

# To read a file using a local path
./build/arithmetic_coder --timing --file <file_path>
```

Output data will be written to the working directory to a file named "output". This can be renamed to the appropriate file.


