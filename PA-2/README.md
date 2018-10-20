# Simple web server program
* handles GET and POST requests
* implements pipelining with a 10 second timeout

## How to run
make \
./webserver \<port\> \<document root\>

The webserver will run continuously on localhost until it is given ctrl-c, or the process is killed. \
Use `telnet` (or `nc` on Mac) to send requests to server. \
GET requests can also be done by going to a browser and going to `127.0.0.1:<port>/path/to/file`. \
For example, if you wanted to see the default webpage and you are running the server on port 8888, type `127.0.0.1:8888/` or `127.0.0.1:8888/index.html` into your favorite web browser. \
`www` is a sample document root used to test the program, I did not create it.
