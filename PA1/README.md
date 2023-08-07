# Compilation
1. Make sure you're under `PA1` directory
2. Run `make clean && make all` in terminal. After the compilation, you should see two executables under `build` directory

# Run
## Server
Start server by `./build/server <port>`. If you don't supply the port number, server will listen on default port specified by macro `DEFAULT_SERVER_PORT` defined `src/const.h`.

## Client
Run a test case by `./build/client <test_case_no> <port>`. If you don't supply the port number, client will make request to default server port specified by macro `DEFAULT_SERVER_PORT`.

The five test cases are:
0. Normal case, all five packets successfully sent
1. Out-of-Order Packets
2. Length field mismatch
3. Incorrect end of packet id
4. Duplicate packets
