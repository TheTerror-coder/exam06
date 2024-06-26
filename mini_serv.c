#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct	s_client
{
	int		id;
	int		fd;
	struct s_client	*next;
}				t_client;

typedef struct	s_vars
{
	int			maxfd;
	int			lastid;
	int 		sockfd, connfd, len;
	struct sockaddr_in servaddr, cli; 
	t_client	clients;
	char	*port_str;
	fd_set		readfds;
	char		msg[1500000];
}				t_vars;


int	exitprocss(void);
int		serv_loop(void);
int		recv_from_clients(void);
int		inform_members(void);
int		add_client(void);
int		rmv_client(int fd);
int		init_tvars(void);
int		init_fdsets(void);
int		ft_error(const char *str);
int		init_serv(void);

t_vars	v;
int		LEN64 = 64;
int		LEN1050 = 1050;
int		LEN4096 = 4096;


int extract_message(char **buf, char **msg)
{
	int	i;

	i = 0;
	while (buf && (*buf)[i])
	{
		(*msg)[i] = (*buf)[i];
		if ((*buf)[i] == '\n')
		{
			(*msg)[i + 1] = 0;
			*buf = *buf + i + 1;
			return (1);
		}
		i++;
	}
	(*msg)[i + 1] = 0;
	*buf = NULL;
	return (1);
}

int main(int argc, char **argv) {
	if (argc < 2)
		return (ft_error("Wrong number of arguments"), 1);
	if (!init_tvars())
		return (exitprocss());
	v.port_str = argv[1];
	if (!init_serv())
		return (exitprocss());
	if (!serv_loop())
		return (exitprocss());
	return (exitprocss());
}

int	exitprocss(void)
{
	t_client	*it;
	t_client	*tmp = NULL;

	it = v.clients.next;
	while (it)
	{
		tmp = it;
		it = it->next;
		close(tmp->fd);
		free(tmp);
	}
	close(v.clients.fd);
	exit(1);
	return (1);
}

int		serv_loop(void)
{
	int		ret_sel = 0;

	while (1)
	{
		if (!init_fdsets())
			return (0);
		ret_sel = select(v.maxfd + 1, &v.readfds, NULL, NULL, NULL);
		if (ret_sel < 0)
			return (ft_error("Fatal error"));
		else if (ret_sel > 0 && FD_ISSET(v.sockfd, &v.readfds))
		{
			v.connfd = accept(v.sockfd, NULL, NULL);
			if (v.connfd < 0)
				return (ft_error("Fatal error"));
			if (!add_client())
				return (0);
			if (!inform_members())
				return (0);
		}
		if (!recv_from_clients())
			return (0);
	}
	return (1);
}

int		transmit(char *buf, int id)
{
	t_client	*it;
	char		str[LEN4096 + 1];
	char		*line = str;
	char		prefix[LEN64 + 1];

	bzero(prefix, sizeof(prefix));
	sprintf(prefix, "client %d: ", id);
	while (buf && buf[0])
	{
		extract_message(&buf, &line);
		it = v.clients.next;
		while (it)
		{
			if (it->id != id)
			{
				send(it->fd, prefix, strlen(prefix), MSG_DONTWAIT);
				send(it->fd, line, strlen(line), MSG_DONTWAIT);
			}
			it = it->next;
		}
	}
	return (1);
}

int		recv_from_clients(void)
{
	int			ret_recv = 0;
	int			count = 0;
	t_client	*it;
	char		buf[LEN1050 + 1];

	it = v.clients.next;
	while (it)
	{
		if (FD_ISSET(it->fd, &v.readfds))
		{
			count = 0;
			ret_recv = LEN1050;
			bzero(v.msg, sizeof(v.msg));
			while (ret_recv == LEN1050)
			{
				bzero(buf, sizeof(buf));
				ret_recv = recv(it->fd, buf, LEN1050, MSG_DONTWAIT);
				if (ret_recv < 0 && !count)
					return (1);
				else if (!ret_recv)
				{
					rmv_client(it->fd);
					return (1);
				}
				else if (ret_recv == LEN1050)
				{
					strcat(v.msg, buf);
					count++;
				}
				else
				{
					strcat(v.msg, buf);
					transmit(v.msg, it->id);
					ret_recv = -1;
				}
			}
		}
		it = it->next;
	}
	return (1);
}

int		inform_members(void)
{
	t_client	*it;
	char		buf[LEN64 + 1];

	bzero(buf, sizeof(buf));
	sprintf(buf, "server: client %d just arrived\n", v.lastid);
	it = v.clients.next;
	while (it)
	{
		if (it->id != v.lastid)
			send(it->fd, buf, strlen(buf), MSG_DONTWAIT);
		it = it->next;
	}
	return (1);
}

int		goodbye_members(int id)
{
	t_client	*it;
	char		buf[64];

	bzero(buf, sizeof(buf));
	sprintf(buf, "server: client %d just left\n", id);
	it = v.clients.next;
	while (it)
	{
		if (it->id != id)
			send(it->fd, buf, strlen(buf), MSG_DONTWAIT);
		it = it->next;
	}
	return (1);
}
int		add_client(void)
{
	t_client	*it;

	it = &v.clients;
	while (it->next)
		it = it->next;
	it->next = calloc(1, sizeof(t_client));
	if (!it->next)
		return (ft_error("Fatal error"));
	it->next->fd = v.connfd;
	it->next->id = it->id + 1;
	it->next->next = NULL;
	v.lastid = it->next->id;
	if (v.connfd > v.maxfd)
		v.maxfd = v.connfd;
	return (1);
}
int		rmv_client(int fd)
{
	t_client	*it;
	t_client	*prec = NULL;
	t_client	*elm = NULL;

	it = &v.clients;
	while (it->next)
	{
		prec = it;
		elm = it->next;
		if (elm->fd == fd)
		{
			goodbye_members(elm->id);
			prec->next = elm->next;
			close(elm->fd);
			free(elm);
			return (1);
		}
		it = it->next;
	}
	return (1);
}

int		init_fdsets(void)
{
	t_client	*it;

	FD_ZERO(&v.readfds);
	FD_SET(v.sockfd, &v.readfds);
	it = v.clients.next;
	while(it)
	{
		FD_SET(it->fd, &v.readfds);
		it = it->next;
	}
	return (1);
}

int		init_tvars(void)
{
	bzero(&v, sizeof(v));
	return (1);
}

int		init_serv(void)
{
	int		port = 0;

	port = atoi(v.port_str);
	// socket create and verification 
	v.sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (v.sockfd == -1)
		return (ft_error("Fatal error"));

	// assign IP, PORT 
	v.servaddr.sin_family = AF_INET; 
	v.servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	v.servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(v.sockfd, (const struct sockaddr *)&v.servaddr, sizeof(v.servaddr))) != 0)
		return (ft_error("Fatal error"));
	if (listen(v.sockfd, 10) != 0) 
		return (ft_error("Fatal error"));
	v.maxfd = v.sockfd;
	v.clients.fd = v.sockfd;
	v.clients.id = -1;
	v.clients.next = NULL;
	return (1);
}

int		ft_error(const char *str)
{
	write(2, str, strlen(str));
	write(2, "\n", 1);
	return (0);
}