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

void errorAndQuit(char *message){
    printf("[-]%s\n",message);
    exit(EXIT_FAILURE);
}
void logMessage(char *message){
    printf("[+]%s\n",message);
}

int serverFD;
int ready_for_input=1;

void *readFromServer(void *args){
    while (1)
    {
        char message[255]={0};
        int messageLen;
        if(messageLen = read(serverFD, message, 255) ==0){
            errorAndQuit("Lost connection to server. Quitting.");
        }
        if(!strcmp(message,"complete\n"))
            ready_for_input=1;
        else
            printf("Server>\n %s",message);
    }
}

void *writeToServer(void *args){
    while (1)
    {
        if(ready_for_input){
            printf("Client> ");
            char command[255], message[255];
            fgets(command,255,stdin);
            command[strcspn(command,"\r\n")]=0;
            write(serverFD, command, strlen(command)+1);
            ready_for_input=0;
        }
    }
}

void enterShell(int server){
    serverFD=server;
    pthread_t read_thread,write_thread;
    pthread_create(&read_thread, NULL, readFromServer, NULL);
    pthread_create(&write_thread, NULL, writeToServer, NULL);
    while(1){}
}

int main(int argc, char *argv[]){
    if( argc != 3 ) {
        printf("Usage %s <IP> <port>",argv[0]);
        exit(0);
    }
    int server, second_server;
    struct sockaddr_in server_addr, second_server_addr;
    char *server_ip_string=argv[1], *server_port_string=argv[2], second_server_ip_string[17], second_server_port_string[7];
    if(inet_pton(AF_INET, server_ip_string, &(server_addr.sin_addr))<0){
        errorAndQuit("The IP you entered could not be found. Try again");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(server_port_string));

    if((server=socket(AF_INET, SOCK_STREAM, 0))<0){
        perror("[-]Socket Error");
        errorAndQuit("Error when setting up socket");
    }
    printf("[+]Connecting to server at %s:%s\n", server_ip_string, server_port_string);
    if(connect(server, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        perror("[-]Connect Error");
        errorAndQuit("Error when connecting to server");
    }
    char message[255];
    if(!read(server,message,255)){
        close(server);
        errorAndQuit("Connection with server broken. Quitting");
    }
    if(!strcmp(message, "accepted")){
        printf("[+]%s:%s accepted connection.\n",server_ip_string,server_port_string);
        enterShell(server);
    }else if (!strcmp(message, "redirect")){
        if((second_server=socket(AF_INET, SOCK_STREAM, 0))<0){
            perror("[-]Socket Error");
            errorAndQuit("Error when setting up second socket");
        }
        write(server, "IP", 3);
        if(!read(server, second_server_ip_string, 17)){
            close(server);
            errorAndQuit("Connection with server broken. Quitting");
        }
        if(inet_pton(AF_INET, second_server_ip_string, &(second_server_addr.sin_addr))<0){
            errorAndQuit("Got an invalid IP redirect from server");
        }
        write(server, "port", 5);
        if(!read(server, second_server_port_string, 7)){
            close(server);
            errorAndQuit("Connection with server broken. Quitting");
        }
        second_server_addr.sin_family = AF_INET;
        second_server_addr.sin_port = htons(atoi(second_server_port_string));
        logMessage("Redirected to a new server");
        printf("[+]Connecting to %s:%s\n",second_server_ip_string,second_server_port_string);
        close(server);
        if(connect(second_server, (struct sockaddr*)&second_server_addr, sizeof(second_server_addr))<0){
            perror("Connect Error:");
            errorAndQuit("Error when connecting to server");
        }
        bzero(message, 255);
        if(!read(second_server,message,255)){
            close(second_server);
            errorAndQuit("Connection with server broken. Quitting");
        }
        if(!strcmp(message, "accepted")){
            printf("[+]%s:%s accepted connection.\n",second_server_ip_string,second_server_port_string);
            enterShell(second_server);
        }else{
            printf("[!] Server sent an unexpected response: %s",message);
            errorAndQuit("Could not get accepted into any server. Quitting");
        }
    }
    return 0;
}