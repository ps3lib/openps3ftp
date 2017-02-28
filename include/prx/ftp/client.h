#pragma once

#include "common.h"
#include "server.h"

struct ClientVariable
{
	char name[32];
	void* ptr;
};

struct Client
{
	struct Server* server_ptr;

	struct ClientVariable* cvar;
	int cvar_count;

	int socket_control;
	int socket_data;
	int socket_pasv;

	data_callback cb_data;
	char lastcmd[32];
};

/* cvar functions */
void* client_get_cvar(struct Client* client, const char name[32]);
void client_set_cvar(struct Client* client, const char name[32], void* ptr);

/* control socket functions */
void client_send_message(struct Client* client, const char* message);
void client_send_code(struct Client* client, int code, const char* message);
void client_send_multicode(struct Client* client, int code, const char* message);
void client_send_multimessage(struct Client* client, const char* message);

/* data socket functions */
bool client_data_start(struct Client* client, data_callback callback, short pevents);
void client_data_end(struct Client* client);
bool client_pasv_enter(struct Client* client, struct sockaddr_in* pasv_addr);

/* event functions */
void client_socket_event(struct Client* client, int socket_ev);
void client_socket_disconnect(struct Client* client, int socket_dc);

/* internal functions */
void client_free(struct Client* client);