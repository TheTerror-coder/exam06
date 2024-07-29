#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>



typedef struct	s_client
{
	int		fd;
	int		id;
	int		cat_flag;
	struct s_client	*next;
}
				t_client;

typedef struct	s_vars
{
	int 				sockfd, connfd, len;
	struct sockaddr_in	servaddr, cli;
	t_client			clients;
	int					lastid;
	int					maxfd;
	fd_set				readfds;
}
				t_vars;

int		exitprocss();
int		init_vars();
int		init_serv(char **argv);
int		serv_loop();
int		receive_from_others();
int		remove_client(int id);
int		transmit(char *ptr, int id);
int		add_client();
int		goodbye_members(int id);
int		hello_others();
int		init_fds();
int 	extract_message(char **buf, char *line);
int		ft_error(const char *msg);


const int		LEN64 = 64;
const int		LEN1500K = 1500000;
t_vars			v;


int main(int argc, char **argv) {
	if (argc != 2)
		return (ft_error("Wrong number of arguments"), 1);
	if (!init_vars())
		return (1);
	if (!init_serv(argv))
		return (exitprocss());
	if (!serv_loop())
		return (exitprocss());
	exitprocss();
	return (0);
}

int		exitprocss()
{
	t_client	*it;
	t_client	*elm = NULL;

	it = v.clients.next;
	while (it)
	{
		elm = it;
		it = it->next;
		close(elm->fd);
		free(elm);
	}
	close(v.connfd);
	return (0);
}

int		init_vars()
{
	bzero(&v, sizeof(v));
	return (1);
}

int		init_serv(char **argv)
{
	int port;
	port = atoi(argv[1]);
	// socket create and verification 
	v.sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (v.sockfd == -1)
		return (ft_error("Fatal error"));

	bzero(&v.servaddr, sizeof(v.servaddr));
	// assign IP, PORT 
	v.servaddr.sin_family = AF_INET; 
	v.servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	v.servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(v.sockfd, (const struct sockaddr *)&v.servaddr, sizeof(v.servaddr))) != 0)
		return (ft_error("Fatal error"));
	if (listen(v.sockfd, 10) != 0)
		return (ft_error("Fatal error"));
	v.clients.fd = v.sockfd;
	v.clients.id = -1;
	v.clients.next = NULL;
	v.maxfd = v.sockfd;
	return (1);
}

int		serv_loop()
{
	int		ret_select = 0;
	while(1)
	{
		if (!init_fds())
			return (1);
		ret_select  = select(v.maxfd + 1, &v.readfds, NULL, NULL, NULL);
		if (ret_select == -1)
			return (ft_error("Fatal error"));
// ft_error("************debug***********");
		if (FD_ISSET(v.sockfd, &v.readfds))
		{
			v.connfd = accept(v.sockfd, NULL, NULL);
			if (v.connfd == -1)
				return (ft_error("Fatal error"));
			if (!add_client())
				return (0);
			if (!hello_others())
				return (0);
		}
		if (!receive_from_others())
			return (0);
	}
	return (1);
}

int		receive_from_others()
{
	char	c;
	int		count = 0, index = 0;
	int		ret_recv = 0;
	t_client	*it, *elm;
	char		received[LEN1500K];

	it = v.clients.next;
	while (it)
	{
		elm = it;
		it = it->next;
		index = 0;
		count = 0;
		ret_recv = LEN64;
		bzero(received, sizeof(received));
		while (ret_recv == LEN64)
		{
			c = 0;
			ret_recv = recv(elm->fd, &c, 1, MSG_DONTWAIT);
			if (ret_recv < 0 && !count)
				break;
			else if (!ret_recv)
			{
				if (count)
					transmit(received, elm->id);
				remove_client(elm->id);
				break;
			}
			else if (ret_recv > 0)
			{
				received[index] = c;
				index++;
				count++;
				ret_recv = LEN64;
				continue;
			}
			else
			{
				if (c)
					received[index] = c;
				transmit(received, elm->id);
				break;
			}
		}
	}
	return (1);
	
}

int		remove_client(int id)
{
	t_client	*it = NULL;
	t_client	*elm = NULL;
	t_client	*prec = NULL;

	it = &v.clients;
	while (it->next)
	{
		prec = it;
		elm = it->next;
		if (elm->id == id)
		{
			goodbye_members(id);
			prec->next = elm->next;
			close(elm->fd);
			free(elm);
			return (1);
		}
		it = it->next;
	}
	return (1);
}

int		transmit(char *ptr, int id)
{
	t_client	*it;
	char		buf[LEN64];
	char		line[LEN1500K];

	bzero(buf, sizeof(buf));
	sprintf(buf, "client %d: ", id);
	while (ptr && ptr[0])
	{
		it = v.clients.next;
		bzero(line, sizeof(line));
		extract_message(&ptr, line);
		while (line[0] && it)
		{
			if (it->id != id)
			{
				if (!it->cat_flag)
					send(it->fd, buf, strlen(buf), MSG_DONTWAIT);
				else
					it->cat_flag = 0;
				if (line[strlen(line) - 1] != '\n')
					it->cat_flag = 1;
				send(it->fd, line, strlen(line), MSG_DONTWAIT);
			}
			it = it->next;
		}
	}
	return (1);
}

int		add_client()
{
	t_client	*it;

	it = &v.clients;
	while (it->next)
		it = it->next;
	it->next = calloc(1, sizeof(t_client));
	if (!it->next)
		return (0);
	it->next->fd = v.connfd;
	it->next->id = it->id + 1;
	it->next->next = NULL;
	v.lastid = it->next->id;
	if (v.connfd > v.maxfd)
		v.maxfd = v.connfd;
	return (1);
}

int		goodbye_members(int id)
{
	t_client	*it;
	char		buf[LEN64];

	bzero(buf, sizeof(buf));
	sprintf(buf, "server: client %d just left\n", id);
	it = v.clients.next;
	while (it)
	{
		if (it->id != id)
		{
			send(it->fd, buf, strlen(buf), MSG_DONTWAIT);
			it->cat_flag = 0;
		}
		it = it->next;
	}
	return (1);
}

int		hello_others()
{
	t_client	*it;
	char		buf[LEN64];

	bzero(buf, sizeof(buf));
	sprintf(buf, "server: client %d just arrived\n", v.lastid);
	it = v.clients.next;
	while (it)
	{
		if (it->id != v.lastid)
		{
			send(it->fd, buf, strlen(buf), MSG_DONTWAIT);
			it->cat_flag = 0;
		}
		it = it->next;
	}
	return (1);
}

int		init_fds()
{
	t_client	*it;

	it = v.clients.next;
	FD_ZERO(&v.readfds);
	FD_SET(v.sockfd, &v.readfds);
	while (it)
	{
		FD_SET(it->fd, &v.readfds);
		it = it->next;
	}
	return (1);
}
int extract_message(char **buf, char *line)
{
	int	i;

	i = 0;
	while ((*buf)[i])
	{
		line[i] = (*buf)[i];
		if ((*buf)[i] == '\n')
		{
			(*buf) = (*buf) + i + 1;
			return (1);
		}
		i++;
	}
	(*buf) = NULL;
	return (1);
}

int		ft_error(const char *msg)
{
	write(2, msg, strlen(msg));
	write(2, "\n", 1);
	return (0);
}