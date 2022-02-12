#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define CTRLPORT 8000
#define DATAPORT 8001
#define QUEUE 50

int create_and_bind_socket(int port, struct sockaddr_in *address)
{
    int sock, num_socks, opt=1;

    //Create TCP socket and set appropriate options
    if ((sock = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) 
	{
		perror("Set socket options failed");
		exit(EXIT_FAILURE);
    }

    address->sin_family      = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port        = htons(port);

    //Bind socket to the port and start to listen
    if (bind(sock, (struct sockaddr *) address, sizeof(*address)) < 0) 
	{
		perror("Socket bind failed");
		exit(EXIT_FAILURE);
    }
    if (listen(sock, 5) < 0)
	{
		perror("Listen socked failed");
		exit(EXIT_FAILURE);
    }

    return sock;
}



/*
 *
 *	MAIN PROGRAM
 *
 */


int main(int argc, char *argv[])
{
    int control_socket, data_socket, client_control_socket[QUEUE], client_data_socket[QUEUE];
    int control_addrlen, data_addrlen;
    int desc, num_socks, sock, event, msglen;
    int tmp, brk, id, key;
    char message[128], str[20];
    struct idkeystruct {
		int id;
		int key;
		int ctrl_sock;
		int data_sock;
    } idkey[QUEUE];
    struct sockaddr_in control_address, data_address;
    char buffer[128];
    fd_set socket_set;
    FILE *log;
    const char* logfile="./testserver.log";
    const char* errmsg="Wrong key or ID";
    srand(time(NULL));

    //Zeroing variables
    for (int i = 0; i < QUEUE; i++) 
	{
		client_control_socket[i]	= 0;
		client_data_socket[i]		= 0;
		idkey[i].id					= 0;
		idkey[i].key				= 0;
		idkey[i].ctrl_sock			= 0;
		idkey[i].data_sock			= 0;
    }

    //Create and initialize sockets
    control_socket  = create_and_bind_socket(CTRLPORT, &control_address);
    data_socket     = create_and_bind_socket(DATAPORT, &data_address);
    control_addrlen = sizeof(control_address);
    data_addrlen    = sizeof(data_address);


	//Main loop
    while (1)
    {
		//We will use select() to test socket descriptors
		FD_ZERO(&socket_set);
		FD_SET(control_socket, &socket_set);
		FD_SET(data_socket, &socket_set);
		num_socks = data_socket;

		for (int i = 0; i < QUEUE; i++)
		{
		//Set decriptors that are in use:
			desc = client_control_socket[i];
			if (desc > 0)
				FD_SET(desc, &socket_set);
			if (desc > num_socks) 
				num_socks = desc;

			desc = client_data_socket[i];
			if (desc > 0)
				FD_SET(desc, &socket_set);
			if (desc > num_socks) 
				num_socks = desc;
		}

		event = select(num_socks+1, &socket_set, NULL, NULL, NULL);

		if ((event < 0) && (errno != EINTR)) 
			printf("Select failure");

		//Establish new connections to 8000 port and save socket for  future use
		if (FD_ISSET(control_socket, &socket_set)) 
		{
			if ((sock = accept(control_socket, (struct sockaddr *)&control_address, (socklen_t*)&control_addrlen)) < 0) 
			{
				perror("New connection to control port failed");
				exit(EXIT_FAILURE);
			}

			for  (int i = 0; i < QUEUE; i++) 
			{
				if (client_control_socket[i] == 0) 
				{
					client_control_socket[i] = sock;
					break;
				}
			}
		}
		
		//Do the same for  the port 8001		
		if (FD_ISSET(data_socket, &socket_set)) 
		{
			if ((sock = accept(data_socket, (struct sockaddr *)&data_address, (socklen_t*)&data_addrlen)) < 0) 
			{
				perror("New connection to data port failed");
				exit(EXIT_FAILURE);
			}
			for (int i = 0; i < QUEUE; i++) 
			{
				if (client_data_socket[i] == 0) 
				{
					client_data_socket[i] = sock;
					break;
				}
			}
		}
	

		//Get ID, check it and send random key to client
		for (int i = 0; i < QUEUE; i++) 
		{
			sock = client_control_socket[i];
			if (FD_ISSET(sock, &socket_set)) 
			{
				if ((msglen = read(sock, buffer, 127)) == 0) 
				{
					close(sock);
					client_control_socket[i] = 0;
				} 
				else 
				{
					buffer[msglen] = '\0';
					if (sscanf(buffer, "%d", &tmp) <= 0) 
					{
						perror("Wrong ID");
						exit(EXIT_FAILURE);
					} 
					else 
					{
						printf("ID = %d\n", tmp);
						//Check ID that we got and terminate if  it's erroneous
						//(for  simplicity, of course will not have such behaviour in production code)
						for (int j = 0; j < QUEUE; j++) 
						{
							if (idkey[j].id == tmp) 
							{
								perror("ID is zero or already in use");
								exit(EXIT_FAILURE);
							}
						}

						//Save ID, key, and control port socket in idkey
						brk = 0;
						for (int j = 0; j < QUEUE; j++) 
						{
							if (idkey[j].id == 0) 
							{
								idkey[j].id = tmp;
								idkey[j].ctrl_sock = sock;
								key = rand();
								idkey[j].key = key;
								brk = 1;
								sprintf(str, "%d", key);
								if (send(sock, str, strlen(str), 0) != strlen(str)) 
									perror("Send error");
								break;
							}
						}
						//idkey has no free space
						if (brk == 0) 
						{
							perror("Too many connections");
							exit(EXIT_FAILURE);
						}
					}
				}
			}
		}

		//Let's start to process 8001 port input now
		for (int i = 0; i < QUEUE; i++) 
		{
			sock = client_data_socket[i];
			if (FD_ISSET(sock, &socket_set)) 
			{
				//if message length is zero, close connection to 8001 port and clear appropriate idkey
				if ((msglen = read(sock, buffer, 127)) == 0 ) 
				{
					close(sock);
					client_data_socket[i] = 0;
					for ( int j = 0; j < QUEUE; j++ ) 
					{
						if (idkey[j].data_sock == sock) 
						{
							idkey[j].id			= 0;
							idkey[j].key		= 0;
							idkey[j].ctrl_sock	= 0;
							idkey[j].data_sock	= 0;
							break;
						}
					}
				} 
				else 
				{
					//Or parse it in either case
					buffer[msglen] = '\0';
					int n = sscanf(buffer, "%d %d %[^\n]", &id, &key, message);
					if (n < 3) 
					{
						perror("Wrong message format");
						exit(EXIT_FAILURE);
					}

					//Check ID/key pair
					brk = 0;
					for (int j = 0; j < QUEUE; j++) 
					{
						if (id == idkey[j].id) 
						{
							brk == 1;
							if (key == idkey[j].key) 
							{
								//Pair matches - we are saving data socket and writing message to the log
								if ((log = fopen(logfile, "a")) == NULL) 
								{
									perror("Could not open log file");
									exit(EXIT_FAILURE);
								}
								fputs(message,log);
								fclose(log);
								idkey[j].data_sock = sock;
								break;
							} 
							else 
							{
								//Pair doesn't match - send error message to client
								if (send(sock, errmsg, strlen(errmsg), 0) != strlen(errmsg)) 
									perror("Send error");
								break;
							}
						}
					}
					//We didn't find ID that we got. Send back the same error message to make it indistinguishable from wrong key
				}
			}
		}
    }
}
