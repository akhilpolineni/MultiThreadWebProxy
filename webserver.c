/*
 * server.c
 *
 */


#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFSIZE 512
#define LISTENBACKLOG 16

//server port to listen to
#define SERVERPORT 11000
#define SERVERADDR "http://127.0.0.1"

void
error (char *err_str)
{
  //explain error occured and exit
  perror (err_str);
  exit (0);
}


int main() {
   int serversocket=0, new_socket=0;
   socklen_t addrlen;
   char buffer[BUFSIZE]="\0", req[BUFSIZE]="\0", reply[BUFSIZE]="\0", fname[BUFSIZE]="\0", temp[BUFSIZE]="\0";
   struct sockaddr_in address={0};
   int reuse = 1;
   int file=-1;
   int length=0;
int feed,i;

   //open new tcp socket for server
   if ((serversocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
      error("Error during opening socket.");
   }

   //bind to any address, and specified server port
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(SERVERPORT);

   //reuse address in case of address conflict
   setsockopt(serversocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

   //bind server to the port
   if (bind(serversocket, (struct sockaddr *) &address, sizeof(address)) < 0)
      error ("Error during opening a socket.");

   //server will listen for incomming connections
   if (listen(serversocket, LISTENBACKLOG) < 0) {
         error("Error during listen.");
      }

   // main server loop
   while (1) {

      // block waiting for new connections, once new client comes, continue with new_socket to communicate
      if ((new_socket = accept(serversocket, (struct sockaddr *) &address, &addrlen)) < 0) {
         error("Error during accept.");
      }

      //receive request
     // recv(new_socket, buffer, BUFSIZE, 0);

	feed = read(new_socket,buffer,BUFSIZE);   //read the request
	if(feed > 0 && feed < BUFSIZE)	 //is it successful
				buffer[feed]=0;		
			else buffer[0]=0;
	for( i=0;i<feed;i++)				
			if(buffer[i] == '\r' || buffer[i] == '\n')
				buffer[i]='*';
	for( i=4;i<BUFSIZE;i++) { // remove additional chars
			if(buffer[i] == ' ') {
				buffer[i] = 0;
				break;
			}
		}
    /*  //parse request and file requested
      sscanf (buffer, "%s %s", req, fname);

      // if in requested file is matched server url, remove it to get only file name
      if (!strncmp(SERVERADDR, fname, strlen(SERVERADDR)))
      {
		   strcpy(temp, &(fname[strlen(SERVERADDR)]));
		   strcpy(fname, temp);
      }


      printf("\nReceived request for file %s\n", fname);
*/
      //check we received GET request, and try to open file
      if((file = open(&buffer[5], O_RDONLY)) == -1)
      {
		//write error 404 to user
		sprintf(reply, "HTTP/1.1 404 Not Found\nContent-Length: 100\nConnection: close\nContent-Type: \
		text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n404 - Not Found</body></html>\n");
		write(new_socket, reply, strlen(reply));
		printf (" can't find file ");

      }
      else
      {
		//get length of file, and prepare it for reading at its start
		length = (long)lseek(file, (off_t)0, SEEK_END);
		lseek(file, (off_t)0, SEEK_SET);

		//write responce header
		if (strstr(fname, ".jp") == NULL )
		   sprintf(reply,"HTTP/1.1 200 OK\nContent-Length: %ld\nConnection: close\nContent-Type: text/html\n\n", length);
		 else
		   sprintf(reply,"HTTP/1.1 200 OK\nContent-Length: %ld\nConnection: close\nContent-Type: image/jpeg\n\n", length);

		write(new_socket, reply, strlen(reply));

		//read content of file and write it back to web client
		while( (length =  read(file, buffer, BUFSIZE)) > 0 )
		  write(new_socket, buffer, length);

		//file read till end, close it
		close(file);
		printf (" file read ok. ");

	   }
	   close(new_socket);
   }
   close(serversocket);
   return 0;
}

