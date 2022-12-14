# Client-Server-Project-Systems-Programming
Implementation of a self load balancing remote shell connection using threads and sockets for the Advanced Systems Programming course at the University of Windsor.

The server process/s and the client process will run on two different machines and the communication between the two processes is achieved using sockets.

Requirement specifications: [requirement.pdf](https://github.com/DhruvNair/Client-Server-Project-Systems-Programming/files/10230486/Project.pdf)

Note: dup2() was used for redirecting the output instead of taking in the input from the socket because then the quit command would not be accessible. 
