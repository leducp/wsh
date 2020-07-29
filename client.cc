
// Client side C/C++ program to demonstrate Socket programming 
#include "linenoise.h"
#include <iostream>

#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#include <fcntl.h>

#include <sys/epoll.h> 

#define PORT 8080 


int main(int argc, char const *argv[]) 
{ 
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 

    int epoll_fd = epoll_create1(0);

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = STDIN_FILENO;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event))
    {
        fprintf(stderr, "Failed to add file descriptor to epoll\n");
        close(epoll_fd);
        return 1;
    }

    event.events = EPOLLIN;
    event.data.fd = sock;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event))
    {
        fprintf(stderr, "Failed to add file descriptor to epoll\n");
        close(epoll_fd);
        return 1;
    }

    bool connected = true;

    enableRawMode();
    while (connected)
    {
        struct epoll_event events[5];
        int new_events = epoll_wait(epoll_fd, events, 5, -1);
        for (int n = 0; n < new_events; ++n) 
        {
            if (events[n].data.fd == STDIN_FILENO) 
            {
                char c;
                read(STDIN_FILENO, &c, 1);
                int rc = write(sock, &c, 1);
                if (rc < 0)
                {
                    connected = false;
                    break;
                }
                continue;
            }

            if (events[n].data.fd == sock) 
            {
                char buffer[1024];
                valread = read( sock , buffer, 1024); 
                if (valread > 0)
                {
                    write(STDOUT_FILENO, buffer, valread);
                }

                if (valread <= 0)
                {
                    connected = false;
                    break;
                }
                continue;
            }
        }
    }
    disableRawMode();
    printf("\n Disconnectd \n");
    return 0; 
} 

