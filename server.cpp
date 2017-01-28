#include <cstdio>
#include <csignal>
#include <vector>
#include <map>
#include <new>
#include <sstream>
#include <algorithm>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <net/net.h>
#include <net/poll.h>
#include <sys/thread.h>
#include <sys/lv2errno.h>

#include "const.h"
#include "server.h"
#include "client.h"
#include "command.h"
#include "common.h"

using namespace std;

#ifdef _USE_FASTPOLL_
int fast_poll(pollfd* fds, nfds_t nfds, int timeout)
{
	return netPoll(fds, nfds, timeout);
}
#endif

void server_start(void* arg)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGXCPU, SIG_IGN);
	signal(SIGSYS, SIG_IGN);

	app_status* status = (app_status*)arg;

	// Create server variables.
	vector<pollfd> pollfds;
	map<int, Client*> clients;
	map<int, Client*> clients_data;
	map<string, cmdfunc> commands;
	bool aio_toggle = false;

	// Register server commands.
	register_cmds(&commands);

	// Create server socket.
	int server = socket(PF_INET, SOCK_STREAM, 0);

	sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(21);
	myaddr.sin_addr.s_addr = INADDR_ANY;

	// Bind port 21 to server socket.
	if(bind(server, (sockaddr*)&myaddr, sizeof myaddr) != 0)
	{
		// Could not bind port to socket.
		status->is_running = 0;
		close(server);
		sysThreadExit(1);
	}

	// Start listening for connections.
	listen(server, LISTEN_BACKLOG);

	// Create and add server pollfd.
	pollfd server_pollfd;
	server_pollfd.fd = FD(server);
	server_pollfd.events = POLLIN;

	pollfds.push_back(server_pollfd);

	// Server loop.
	while(status->is_running)
	{
		int p = poll(&pollfds[0], (nfds_t)pollfds.size(), 500);

		if(p == 0)
		{
			// nothing happened
			continue;
		}

		if(p == -1)
		{
			// poll error
			status->is_running = 0;
			break;
		}

		// new event detected!
		// iterate through connected sockets
		for(vector<pollfd>::iterator pfd_it = pollfds.begin(); pfd_it != pollfds.end(); pfd_it++)
		{
			if(p == 0)
			{
				break;
			}

			pollfd pfd = *pfd_it;

			if(pfd.revents != 0)
			{
				p--;

				// handle socket events, depending on socket type
				// server
				if(OFD(pfd.fd) == server)
				{
					// incoming connection
					int client_new = accept(server, NULL, NULL);

					if(client_new == -1)
					{
						continue;
					}

					// create new internal client
					Client* client = new (nothrow) Client(client_new, &pollfds, &clients, &clients_data);

					// check if allocated successfully
					if(!client)
					{
						close(client_new);
						continue;
					}

					if(!client->buffer || !client->buffer_data)
					{
						delete client;
						continue;
					}

					// set default variables
					client->cvar_use_aio = aio_toggle;

					// assign socket to internal client
					clients.insert(make_pair(client_new, client));

					// create and add pollfd
					pollfd client_pollfd;
					client_pollfd.fd = FD(client_new);
					client_pollfd.events = (POLLIN|POLLRDNORM);

					pollfds.push_back(client_pollfd);

					// hello!
					client->send_multicode(220, WELCOME_MSG);

					client->send_string(" Supported commands:");

					for(map<string, cmdfunc>::iterator cmds_it = commands.begin(); cmds_it != commands.end(); cmds_it++)
					{
						client->send_string("  " + cmds_it->first);
					}

					client->send_code(220, "Ready.");
					continue;
				}
				else
				{
					// check if socket is data
					map<int, Client*>::iterator cdata_it = clients_data.find(OFD(pfd.fd));

					if(cdata_it != clients_data.end())
					{
						// get client
						Client* client = cdata_it->second;

						// check for disconnection
						if(pfd.revents & (POLLNVAL|POLLHUP|POLLERR))
						{
							client->data_end();
							continue;
						}

						// execute data handler
						if(pfd.revents & (POLLOUT|POLLWRNORM|POLLIN|POLLRDNORM))
						{
							client->handle_data();
							continue;
						}

						continue;
					}

					// check if socket is a client
					map<int, Client*>::iterator client_it = clients.find(OFD(pfd.fd));

					if(client_it != clients.end())
					{
						// get client
						Client* client = client_it->second;

						// check disconnect event
						if(pfd.revents & (POLLNVAL|POLLHUP|POLLERR))
						{
							delete client;
							pollfds.erase(pfd_it);
							clients.erase(client_it);
							continue;
						}

						// check receiving event
						if(pfd.revents & (POLLIN|POLLRDNORM))
						{
							ssize_t bytes = recv(client->socket_ctrl, client->buffer, CMD_BUFFER - 1, 0);

							// check if recv was valid
							if(bytes <= 0)
							{
								// socket was dropped
								delete client;
								pollfds.erase(pfd_it);
								clients.erase(client_it);
								continue;
							}

							client->buffer[bytes] = '\0';

							// handle commands at a basic level
							string data(client->buffer);
							data.resize(bytes - 2);

							// pretend we didn't see a blank line
							if(data.empty())
							{
								continue;
							}

							stringstream in(data);

							string cmd, params, param;
							in >> cmd;

							while(in >> param)
							{
								if(!params.empty())
								{
									params += ' ';
								}

								params += param;
							}

							// capitalize command string internally
							transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

							// internal commands
							if(cmd == "QUIT")
							{
								client->send_code(221, "Bye");
								
								delete client;
								pollfds.erase(pfd_it);
								clients.erase(client_it);
								continue;
							}

							if(cmd == "APP_EXIT")
							{
								status->is_running = 0;
								break;
							}

							if(cmd == "AIO")
							{
								aio_toggle = !aio_toggle;
								string aio_status(aio_toggle ? "true" : "false");

								client->send_code(200, "Async IO toggled for new connections: " + aio_status);
								continue;
							}

							// handle client command
							client->handle_command(&commands, cmd, params);
							continue;
						}

						continue;
					}
				}
			}
		}
	}

	// Close sockets.
	for(map<int, Client*>::iterator client_it = clients.begin(); client_it != clients.end(); client_it++)
	{
		Client* client = client_it->second;
		delete client;
	}

	status->is_running = 0;
	close(server);
	sysThreadExit(0);
}