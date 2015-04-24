#define _GNU_SOURCE // defining compiler
#include <stdio.h> // including library files
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h> // setting up socket connection
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stddef.h>
#include <dirent.h>
#include <netinet/in.h>
#include <netinet/tcp.h>



 
#define REQBUFLEN 512
#define LISTENBACKLOG 16
#define TRUE 1
#define WEBSERVERPORT 80 // defining port number to webserver
#define DATABUFLEN 512
#define PROXYPORT 8888 // configuring port number


char web_server[REQBUFLEN]={0};
char server_name[REQBUFLEN]={0};
char *cache_dir="./cache"; //setting up cache directory
char *file_prefix="cache_";
char *blockedsites="blocksites.txt"; // setting up file to block listed sites
char *blockedwords="blockwords.txt"; // setting up file to block listed word from the www.hyperhero.com/en/insults.htm


//handle error by printing the error message and exit
void
error (char *err_str)
{
  perror (err_str);
  exit (0);
}



// create new random file for storing cache info of one URL
FILE * new_cache_file(char *hostname ,char * url)
{
	char txtfile[512];
	
	//generate temporary filename
	char *file  = tempnam(cache_dir, file_prefix);
	FILE *fp ;
	strcpy(txtfile,file);
	//create file for host and url storage - with .txt extension
	strcpy(txtfile + strlen(txtfile), ".txt");
	fp = fopen(txtfile, "w");
	fprintf(fp, "%s\r\n%s", hostname, url);
	fclose(fp);
	//now create file for cache content and return it
	fp = fopen(file, "w");
	free(file);
	
	return fp;
}

// for given host and URL , search all cache files, and return matching one.
char * find_cache_file(char *hostname, char *url)
{
  DIR *dp;
  struct dirent *ep;
  FILE *fp ;
  char *file_name=NULL, *extention;
  char readhostname[512], readurl[512], fname[512];

  //open cache_dir to look for liles
  dp = opendir (cache_dir);
  if (dp != NULL)
    {
	  //while there are more files
      while ((ep = readdir (dp)))
      {
		 //look only for .txt files
		 if (strstr(ep->d_name, ".txt"))
		 {
			 readhostname[0] = readurl[0] = '\0';
		     strcpy(fname, cache_dir);
		     strcat(fname, "/");
		     strcat(fname, ep->d_name);
		     //open cache txt file
		     fp = fopen(fname, "r");
		    
		     if (fp)
		     {
			   //parse from file host and url	  
		       fscanf(fp, "%s\r\n%s", readhostname, readurl);
		       fclose(fp);
		     }  
		     //compare if match with ones we are looking for 
		     if (!strcmp(readhostname, hostname)  && !strcmp(readurl, url))
		       {
				  //we have found exact cache file, remove .txt extension
				  file_name = strdup(fname);
				  extention = strstr(file_name,".txt");
				  if (extention)
				    *extention = '\0';
				  // if match - break and return file name
				  break;   
			   }
		 }
	     
	  }   
      (void) closedir (dp);
      return file_name;
    }
  else
    return NULL;
	
}

//get etag from cache file
void get_cache_tags (char *cache_file , char *etag)
{
// in cache file, locate etag:
//	Etag: “4135cda4″
//    const char *last_modified_keyword="Last-Modified:";
    const char *etag_keyword="Etag:";
	char line[512], *ptr;
	FILE *fp;

	etag[0] = '\0';
	
	//open cache file
	fp = fopen(cache_file, "r");
	while(!feof(fp)) 
	 // read line by line cache file
     if (fgets(line,512,fp)) 
     {
	   if (line[strlen(line) - 1] == '\n' ) 
				line[strlen(line) - 1] = '\0';
	   //check if etag is present
	   if (!strlen(	etag) && (ptr=strstr(line, etag_keyword)))
	   {
		   //etag found - return it to caller
		   strcpy(etag, ptr+strlen(etag_keyword) );
	   }
	   
	   if (strlen(etag))
	       break;
    } //end of if
    
   fclose(fp);
   
  return;
}

// convert hostname to IP address
int hostname_to_ip(char * hostname , char* ip)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
    
    // try to resolve hostname     
    if ( (he = gethostbyname( hostname ) ) == NULL) 
    {
        // fail to get the host info
        return 1;
    }
 
    //get address list
    addr_list = (struct in_addr **) he->h_addr_list;
     
    //loop address list to get one 
    for(i = 0; addr_list[i] != NULL; i++) 
    {
        //return the first IP address found
        strcpy(ip , inet_ntoa(*addr_list[i]) );
        return 0;
    }
     
    return 1;
}

//check if host appears in block list
int is_host_blocked( char  * hostname)
{
	char fline[512];
	FILE *fp;
	fp = fopen(blockedsites, "r");
	while(!feof(fp)) {
	//read every line of block file
    if (fgets(fline,512,fp)) {
		//check if hostname matches with read line
        if (strstr(fline, hostname))
          {
			   fclose(fp);
			   return 1;
		  }
    }
   }
   fclose(fp);
   //no match found
   return 0;
}

 
     
// handle proxy connection for multiple requests
void *
handle_proxy (void *sockptr)
{
  int mysocket = 0;
  struct sockaddr_in host_addr = {0};
  int serversock1 = 0, n = 0 , serversock = 0;
  char buffer[DATABUFLEN], req1[REQBUFLEN], req2[REQBUFLEN], req3[REQBUFLEN];
  char cond_buffer[DATABUFLEN];
  FILE *fp; 
 

  mysocket = (int)(intptr_t)sockptr;
 
  bzero ((char *) buffer, DATABUFLEN);
  //read request from browser
  recv (mysocket, buffer, DATABUFLEN, 0);

  //parse request - check request and protocol
  sscanf (buffer, "%s %s %s", req1, req2, req3);
  if ((strncmp (req1, "GET", 3) == 0))
      {
      char *host_name = req2;
      char *url=req2;
      char web_ip[REQBUFLEN]={0};
	  char server_name[REQBUFLEN]={0};
	  char *cache_name;
	  //char cache_timestamp[DATABUFLEN]={0};
	  char cache_etag[DATABUFLEN]={0};
	  
      //try to get the hostname of the server to request    
      if (host_name[0] == '/') 
           host_name = &host_name[1];
      //can we resolve the hostname     
	  if (0 != hostname_to_ip(host_name, web_ip))
	  {	
		  if (strlen(web_server) && strlen(server_name))
		    {
				host_name = server_name;
				strcpy(web_ip, web_server);
			}
		  else 
		  {	
		        printf ("Error during resolving server %s", host_name);
			    return NULL;
		   }	 
	  }
	  else
	  {
		 strcpy(web_server, web_ip);
         strcpy(server_name, host_name);
         url = "/";
      }   
      //will connect to hardcoded web server address and port
      host_addr.sin_port = htons (WEBSERVERPORT);
      host_addr.sin_family = AF_INET;
      host_addr.sin_addr.s_addr = inet_addr(web_ip);
      
      
      //check if host is blocked
      if (is_host_blocked(host_name))
       {  
		     printf ("\r\nHost %s is BLOCKED, exit!\r\n", host_name);
		     sprintf(buffer, "404 Host is blocked!");
		     send (mysocket, buffer, strlen(buffer), 0);
			 close (mysocket);
			 return NULL;
	   }
      
	  printf("\r\nopening site: %s , url: %s", web_ip, url);
	  //open socket and connect to web server
      serversock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
      serversock1 = connect (serversock, (struct sockaddr *) &host_addr,
							sizeof (struct sockaddr));
	  //check if connected, and exit from thread in case of error
      if (serversock1 < 0)
      {
			printf ("Error during connecting to remote server");
			sprintf(buffer, "%s %s %s\r\nhost: %s\r\n\r\n", "GET", url, "HTTP/1.1", host_name);
			return NULL;
      }
      cond_buffer[0]='\0';
      //find if host/url appear in cache
      cache_name = find_cache_file(host_name, url);
      if (cache_name)
      {
        printf ("\r\n found cache file: %s", cache_name); 
        //get etag from cache
        get_cache_tags(cache_name, cache_etag);
        
        //fill in conditional http request
        if (strlen(cache_etag))
          sprintf(cond_buffer, "\r\nIf-None-Match: %s", cache_etag);
      }

       
	  //send request to web server
	  sprintf(buffer, "%s %s %s\r\nhost: %s%s\r\n\r\n", "GET", url, "HTTP/1.1", host_name, cond_buffer);
      n = send (serversock, buffer, strlen (buffer), 0);
      if (n <= 0)
      {
			printf ("Error in writing to socket");
			return NULL;
	  }
     
      bzero ((char *) buffer, DATABUFLEN);
      n = recv (serversock, buffer, DATABUFLEN, 0);
      
      //check responce , if cache should be used
      if (cache_name && strstr(buffer, " 304 "))
      {
		//not modified use cache content  
		printf("\r\n Using cache content...");
		
		//read cache file
	    fp = fopen(cache_name, "r");
	    while(!feof(fp)) 
         if ((n = fread(buffer, 1, DATABUFLEN, fp)))
         {
			 //filter bad words, and send to browser
	         hide_bad_words(buffer, n);
		     send (mysocket, buffer, n, MSG_DONTWAIT);
		     bzero(buffer, DATABUFLEN);
         } //end of if
    
        fclose(fp);
		  
	  }
	  else //modified, feed content from server
	  {
        printf("\r\n reading from web server...");
        //allocate cache file 
		fp =  new_cache_file(host_name,url);  
		hide_bad_words(buffer, n);
		send (mysocket, buffer, n, MSG_DONTWAIT	);
		fwrite(buffer, n, 1, fp);
	    do
	    {
	      bzero ((char *) buffer, DATABUFLEN);
	      //try to receive from web server
	      n = recv (serversock, buffer, DATABUFLEN, 0);
	      
	      // and pass it back to browser
	      if (n>0)
	      {
			hide_bad_words(buffer, n);
		    send (mysocket, buffer, n, MSG_DONTWAIT	);
		    fwrite(buffer, n, 1, fp);
		  }  
		    
	    }
	    while (n > 0);
	    
	    fclose(fp);
	  }
    }
  else
    { 
	  //unsupported request 
      send (mysocket, "400 : BAD REQUEST\nOnly Get HTTP requests.", 19, 0);
    }
  //close sockets to server and browser, and exit from thread
  printf("\r\n Request done."); fflush(stdout);
  close (serversock);
  
  close (mysocket);
  return NULL;
}
//hide bad words  
int hide_bad_words( char  * buffer, int size)
{
	char word[512], *match;
	FILE *fp;
	int i;
	fp = fopen(blockedwords, "r");

	//read every blocked word
	while(!feof(fp)) {
    if (fgets(word,512,fp)) 
    {
	   match = buffer;
	   //remove trailing newline
	   if (word[strlen(word) - 1] == '\n' ) 
				word[strlen(word) - 1] = '\0';
				
	   //search case insensitive into buffer padded	
       while ((match = strcasestr(match, word)))
         {
			 //check we didn't match part of word
			 if (!isalpha(match[-1]) && !isalpha(match[strlen(word)]))
			 { 
			   //we matched a word -> replace it with "-"s
		       for (i=0; i<strlen(word); i++,match++)
		          *match = '-';
		     } else
		        match += strlen(word);
		 }
    }
   }
   fclose(fp);
   return 0;
}

int
main (int argc, char *argv[])
{
  struct sockaddr_in acc_addr, bind_addr;
  int sockfd = -1, newsockfd;
  pthread_t thread_id = 0;
  int socklen = 0;
  int reuse = 1, flag = 1;


  bzero ((char *) &bind_addr, sizeof (bind_addr));
  bzero ((char *) &acc_addr, sizeof (acc_addr));
  
  //proxy for all interfaces, specified port
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons (PROXYPORT);
  bind_addr.sin_addr.s_addr = INADDR_ANY;

  //open socket for proxy
  sockfd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0)
    error ("Error during socket init");
    
  //reuse address in case of address conflict   
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

  // bind proxy to specified address and port
  if (bind (sockfd, (struct sockaddr *) &bind_addr, sizeof (bind_addr)) < 0)
    error ("Error during binding socket");

  //listen for incomming connections
  listen (sockfd, LISTENBACKLOG);


  while (TRUE)
    {

      //accept new sessions, proxy blocks here until new connection gets accepted
      socklen = sizeof (acc_addr);
      newsockfd = 	accept (sockfd, (struct sockaddr *) &acc_addr, (socklen_t *) & socklen);

      if (newsockfd < 0)
		error ("Error to accepting connection");
		
	   setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));	

	  //spawn proxy as new thread
      if (pthread_create (&thread_id, NULL, handle_proxy, (void *)(intptr_t) newsockfd)
			< 0)
	error ("Error during pthread creation");
    }

  return 0;
}
