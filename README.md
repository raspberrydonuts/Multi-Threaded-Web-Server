Multi-Threaded Web Server using POSIX threads in C

Creates dispatcher and worker threads to handle HTML, GIF, JPEG, TXT files of any arbitrary size. Uses POSIX socket programming to create a client/server network that receives requests and sends responses.

How to compile:
run `make`
run `make clean` to remove output files

How to run program:
First compile by running `make`
Then run `./web_server port_num testing num_dispatchers num_workers 0 queue_length 0`
Example: `./web_server 9000 testing 100 100 0 100 0`

port_num must be between 1025 and 65535
num_dispatchers, num_workers, and queue_length must be greater than 0 and less than 101

On another terminal, you can run something like
`wget http://127.0.0.1:9000/image/jpg/29.jpg`

To run several concurrent requests, go into the project root directory (where server.c is) and run:
`cat testing/urls | xargs -n 1 -P 8 wget`
Note: This concurrent requests command will only work if you run with port 9000

The output file `web_server_log` will contain a log of requests
