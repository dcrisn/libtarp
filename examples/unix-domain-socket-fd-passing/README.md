When calling the server and client with argument = 'pidfd' you will have to run
as `sudo`, otherwise the call to `syscall(SYS_pid..)` will fail.

So, from the build/examples directory where the server and client binaries get
compiled, call:
```
sudo LD_LIBRARY_PATH=$(realpath ../../build/lib) ./uds-fdpass-server pidfd
sudo LD_LIBRARY_PATH=$(realpath ../../build/lib) ./uds-fdpass-client pidfd
```
