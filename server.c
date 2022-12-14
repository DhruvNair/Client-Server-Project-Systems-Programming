#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>


int req_count=0;
int req_since_control = 0;
int handling_req=1;
int handshake_complete=0;
char second_server_ip_string[17], second_server_port_string[7];
int server_socket, second_server_socket;
struct sockaddr_in server_addr, second_server_addr;

void errorAndQuit(char *message){
    printf("[-]%s\n",message);
    exit(EXIT_FAILURE);
}
void logMessage(char *message){
    printf("[+]%s\n",message);
}

void *readFromSecondServer(void *args){
    while (1)
    {
        char message[255];
        if(!read(second_server_socket,message,255)){
            handshake_complete=0;
            close(second_server_socket);
            errorAndQuit("Connection with second server broken. Quitting");
        }
        if(!strcmp(message, "you")){
            req_since_control=0;
            handling_req=1;
            logMessage("Received control. Accepting requests now.");
        }else{
            printf("\n[!]Received unknown message from second server: %s\n",message);
        }
    }
}

void *writeToSecondServer(void *args){
    while (handshake_complete)
    {
        if(handling_req && req_count>=5 && req_since_control>0){
            printf("[+]Completed %d requests and transfered control.\n",req_since_control);
            req_since_control=0;
            handling_req=0;
            write(second_server_socket, "you", 4);
        }
    }
    errorAndQuit("Handshake broken. Quitting.");
}

void serviceClient(int client){
    while(1){
        char message[255];
        if(!read(client, message, 255))
            exit(0);
        printf("Got command: %s\n",message);
        if(!strcasecmp(message,"quit")){
            close(client);
            exit(0);
        }
        int pid = fork();
        if(pid < 0)
            errorAndQuit("Error when forking");
        else if (pid == 0){
            dup2(client, STDOUT_FILENO);
            system(message);
            sleep(1);
            write(client, "complete\n",10);
            exit(0);
        }
    }
}

int main() {
    logMessage("Setting up socket");
    socklen_t len=sizeof(server_addr);

    if((server_socket=socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("[-]Socket Error");
        errorAndQuit("Error when setting up socket");
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = 0;
    
    if(bind(server_socket,(struct sockaddr*)&server_addr,len)<0){
        perror("[-]Binding Error");
        errorAndQuit("Error when binding socket");
    }
    
    logMessage("Socket setup complete!");
    if (getsockname(server_socket, (struct sockaddr *)&server_addr, &len) <0)
        errorAndQuit("getsockname failed!");
    printf("\nSocket set up on port #%d\n\n", ntohs(server_addr.sin_port));
    
    printf("Enter IP address of the second server: ");
    fgets(second_server_ip_string, 17, stdin);
    second_server_ip_string[strcspn(second_server_ip_string,"\r\n")]=0;
    if(inet_pton(AF_INET, second_server_ip_string, &(second_server_addr.sin_addr))<0){
        errorAndQuit("The IP you entered could not be found. Try again");
    }

    printf("Enter port number: ");
    fgets(second_server_port_string, 7, stdin);
    second_server_port_string[strcspn(second_server_port_string,"\r\n")]=0;

    second_server_addr.sin_family = AF_INET;
    second_server_addr.sin_port = htons(atoi(second_server_port_string));

    logMessage("Trying to connect to the given server.");
    if((second_server_socket=socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("[-]Second Socket Error");
        errorAndQuit("Error when setting up second socket");
    }
    if(connect(second_server_socket,(struct sockaddr*)&second_server_addr,sizeof(second_server_addr))<0){
        logMessage("Could not connect to second server. Listening for any connections.");
        listen(server_socket,10);
        while(!handshake_complete){
            char message[255];
            second_server_socket = accept(server_socket,NULL,NULL);
            read(second_server_socket, message,255);
            if(!strcmp(message,"server2")){
                logMessage("Handshake with second server complete!");
                handshake_complete=1;
                handling_req=1;
                pthread_t read_thread,write_thread;
                pthread_create(&read_thread, NULL, readFromSecondServer, NULL);
                pthread_create(&write_thread, NULL, writeToSecondServer, NULL);
            }
        }
    }else{
        logMessage("Connected to the provided server. Waiting for a signal from that server.");
        write(second_server_socket,"server2",8);
        logMessage("Handshake with second server complete!");
        handshake_complete=1;
        handling_req=0;
        pthread_t read_thread,write_thread;
        pthread_create(&read_thread, NULL, readFromSecondServer, NULL);
        pthread_create(&write_thread, NULL, writeToSecondServer, NULL);
    }
    logMessage("Listening for client connections now");
    while(1){
        if(req_count>=5)
            listen(server_socket, 1);
        else
            listen(server_socket, 5);
        int client=accept(server_socket, NULL, NULL);
        if(handling_req){
            logMessage("Accepting request");
            write(client, "accepted", 9);
            int pid=fork();
            if(pid<0){
                errorAndQuit("Error when forking.");
            } else if(pid == 0){
                serviceClient(client);
            } else {
                close(client);
                req_count++;
                req_since_control++;
            }
        } else {
            logMessage("Got a request");
            printf("[+]Not accepting requests right now. Redirecting to %s:%s.\n",second_server_ip_string,second_server_port_string);
            char message[255];
            write(client, "redirect", 9);
            read(client, message, 255);
            if(!strcmp(message,"IP"))
                write(client, second_server_ip_string, 17);
            else{
                printf("\nReceived message: %s\n\n", message);
                printf("[!]Client didn't ask for IP.");
            }
            bzero(message, 255);
            read(client, message,255);
            if(!strcmp(message,"port"))
                write(client, second_server_port_string, 7);
            else{
                printf("\nReceived message: %s\n\n", message);
                printf("[!]Client didn't ask for port.");
            }
            close(client);
        }
    }
    return 0;
}