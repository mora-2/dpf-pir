# DPF-PIR

## Requirements

    - Boost
    - gRPC

## How to use

1. git clone repository

   ```
   git clone https://github.com/mora-2/dpf-pir.git
   cd dpf-pir
   ```
2. build server

   ```
   mkdir https/server/build/ && cd https/server/build/
   cmake ..
   make -j 32
   ```
3. build client

   ```
   mkdir https/client/build/ && cd https/client/build/
   cmake ..
   make -j 32
   ```
4. run server in two separate terminal

   ```
   ./server --id=0
   ```
   ```
   ./server --id=1
   ```
5. run client

   ```
   ./client --id=alice --q=0
   ```
