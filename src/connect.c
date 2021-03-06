/* This part of the program creates the inital set 
 * of connections in the alloted time, or with a
 * server setup, and stores it in player structs
 * and in an FD_SET which gets returned.
 */

#include "server.h"

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//this gets passed a Server Info struct that is in the main program 
//for general information, and passes back an fd_set for tnhe list
//of all connected parties, hopefully to make later selects easier.
int start_server(Server_Info *serverinfo, fd_set *current_users)
{
	fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int biggest_fd;   // largest file descriptor number 

	int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, ext_port, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    biggest_fd = listener; // so far, it's this one

	int select_result = 0;
	struct timeval tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	
	for(;;) {
        read_fds = master; // copy it
		printf("the time starting left is %i seconds.\n", (int)tv.tv_sec );
		select_result = select(biggest_fd+1, &read_fds, NULL, NULL, &tv);
        if (select_result == -1) {
            perror("select");
            exit(4);
        }
		else if(select_result)
		{
			printf("the time left is %i seconds.\n", (int)tv.tv_sec );
        	// run through the existing connections looking for data to read
	        for(i = 0; i <= biggest_fd; i++) {
	            if (FD_ISSET(i, &read_fds)) { // we got one!!
	                if (i == listener) {
	                    // handle new connections
	                    addrlen = sizeof remoteaddr;
	                    newfd = accept(listener,
	                        (struct sockaddr *)&remoteaddr,
	                        &addrlen);
						
	                    if (newfd == -1) {
	                        perror("accept");
	                    } else {
	                        FD_SET(newfd, &master); // add to master set
	                        if (newfd > biggest_fd) {    // keep track of the max
	                            biggest_fd = newfd;
	                        }
	                        printf("selectserver: new connection from %s on "
	                            "socket %d\n",
	                            inet_ntop(remoteaddr.ss_family,
	                                get_in_addr((struct sockaddr*)&remoteaddr),
	                                remoteIP, INET6_ADDRSTRLEN),
	                            newfd);
	                    }
	                } else {
	                    // handle data from a client
	                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
	                        // got error or connection closed by client
	                        if (nbytes == 0) {
	                            // connection closed
	                            printf("selectserver: socket %d hung up\n", i);
	                        } else {
	                            perror("recv");
	                        }
	                        close(i); // bye!
	                        FD_CLR(i, &master); // remove from master set
	                	} 
	                } // END handle data from client
	            } // END got new incoming connection
	        } // END looping through file descriptors
		} // END else if for if this is a thing
		else
		{
			break;
		}
    } // END for(;;)--and you thought it would never end!

	printf("Done looking for users (30 seconds elapsed)\n");

	(*current_users) = master;
	return 0;
}
