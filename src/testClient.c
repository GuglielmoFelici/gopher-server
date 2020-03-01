/*
	C ECHO client example using sockets
*/
#include <arpa/inet.h>  //inet_addr
#include <stdio.h>      //printf
#include <stdlib.h>
#include <string.h>      //strlen
#include <sys/socket.h>  //socket
#include <unistd.h>

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server;
    char message[1000], server_reply[2000];

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[1]));

    puts("Connected\n");

    //keep communicating with server
    while (1) {
        server_reply[0] = '\0';
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            printf(" Could not create socket ");
            return 1;
        }
        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
            perror("Connect failed.Error ");
            return 1;
        }

        // Input message
        printf("Enter message: ");
        scanf("%s", message);
        //Send request
        if (send(sock, message, strlen(message), 0) <= 0) {
            puts("Send failed ");
            return 1;
        }
        if (send(sock, "\r\n", 2, 0) <= 0) {
            puts("Send failed ");
            return 1;
        }

        //Receive a reply from the server
        int bytesRead = 0, totalBytesRead = 0;
        while ((bytesRead = recv(sock, server_reply + totalBytesRead, 2000, 0)) > 0)
            totalBytesRead += bytesRead;
        server_reply[totalBytesRead] = '\0';
        puts("Server reply: ");
        puts(server_reply);
        fflush(stdout);
        close(sock);
    }
    close(sock);
    return 0;
}