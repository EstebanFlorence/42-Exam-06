#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_CLIENTS 1024

typedef struct s_client
{
	int				id;
	int				fd;
	char			*buf;
	// struct s_client	*next;
}	t_client;

t_client	clients[MAX_CLIENTS];
int			sockfd, connfd, maxfd;
int			ids;
fd_set		readfds; //, writedfs, activeds;

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	error(char* msg)
{
	write(STDERR_FILENO, msg, strlen(msg));
	write(STDERR_FILENO, "\n", 1);
	// close fds?
	exit(1);	
}

void	sendall(int senderfd, const char* msg)
{
	for (int i = 0; i < MAX_CLIENTS; ++i)	// maxfd
	{
		if (clients[i].fd != -1 && clients[i].fd != senderfd)
			if (send(clients[i].fd, msg, strlen(msg), 0) < 0)
				error("Fatal error");
	}
}

void	broadcast(int senderfd, int senderid, char *msg)
{
	// char buf[4200000];
	int		bufsize = snprintf(NULL, 0, "client %d: %s", senderid, msg) + 1;
	char*	buf = malloc(bufsize);
	if (!buf)
		error("broadcast buffer allocation failed");
	snprintf(buf, bufsize, "client %d: %s", senderid, msg);
	sendall(senderfd, buf);
	free(buf);
}

void	removeclient(int i)
{
	char	msg[64];
	sprintf(msg, "server: client %d just left\n", clients[i].id);
	sendall(clients[i].fd, msg);

	close(clients[i].fd);
	free(clients[i].buf);
	clients[i].fd = -1;
	clients[i].buf = NULL;
}

int	addclient(int sockfd)
{
	connfd = accept(sockfd, 0, 0);
	if (connfd < 0)
		error("Fatal error");
	
	if (connfd > maxfd)
		maxfd = connfd;
	
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i].fd != -1)
			continue;
		clients[i].fd = connfd;
		clients[i].id = ids++;
		clients[i].buf = NULL;

		char msg[64];
		sprintf(msg, "server: client %d just arrived\n", clients[i].id);
		sendall(clients[i].fd, msg);

		return i;
	}

	close(connfd);
	return -1;
}

void	handleclient(int i)
{
	char	buf[4096];
	int		bytes = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);

	if (bytes <= 0)
	{
		removeclient(i);
		return;
	}

	buf[bytes] = 0;
	clients[i].buf = str_join(clients[i].buf, buf);

	char*	msg;
	while (extract_message(&(clients[i].buf), &msg))
	{
		broadcast(clients[i].fd, clients[i].id, msg);
		free(msg);
	}
}

int main(int ac, char **av)
{
	if (ac != 2)
		error("Wrong number of arguments");

	int	port = atoi(av[1]);
	if (port < 0)
		error("Invalid port");

	struct sockaddr_in servaddr; //, cli; 

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd < 0)
		error("Fatal error");

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		error("Fatal error");

	if (listen(sockfd, 10) != 0)
		error("Fatal error");

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		clients[i].fd = -1;
	}

	while (42)
	{
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		maxfd = sockfd;

		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (clients[i].fd != -1)
			{
				FD_SET(clients[i].fd, &readfds);
				if (clients[i].fd > maxfd)
					maxfd = clients[i].fd;
			}
		}

		if (select(maxfd + 1, &readfds, 0, 0, 0) < 0)
			continue;
		
		if (FD_ISSET(sockfd, &readfds))
		{
			addclient(sockfd);
		}

		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &readfds))
				handleclient(i);
		}

	}

	return 0;
}