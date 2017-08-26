/*
 *      netlink.h
 *      
 *      Copyright 2012-2013 Alex <alex@linuxonly.ru>
 *      
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NETLINK_H__
#define __NETLINK_H__

#include <time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/inet_diag.h>
#include <linux/rtnetlink.h>

enum _mmgui_netlink_interface_event_type {
	MMGUI_NETLINK_INTERFACE_EVENT_TYPE_UNKNOWN = 0,
	MMGUI_NETLINK_INTERFACE_EVENT_TYPE_ADD     = 1 << 0,
	MMGUI_NETLINK_INTERFACE_EVENT_TYPE_REMOVE  = 1 << 1,
	MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS   = 1 << 2,
};
/*Connection change identifiers*/
enum _mmgui_netlink_connection_event_type {
	MMGUI_NETLINK_CONNECTION_EVENT_ADD          = 0,
	MMGUI_NETLINK_CONNECTION_EVENT_REMOVE       = 1,
	MMGUI_NETLINK_CONNECTION_EVENT_MODIFY       = 2
};
/*Connection paramerters*/
struct _mmgui_netlink_connection {
	guint inode;
	guint dqueue;
	uid_t userid;
	time_t updatetime;
	pid_t apppid;
	gchar *appname;
	gchar *dsthostname;
	gchar srcaddr[INET6_ADDRSTRLEN + 8];
	gchar dstaddr[INET6_ADDRSTRLEN + 8];
	gushort srcport;
	guchar state;
	guchar family;
};

typedef struct _mmgui_netlink_connection *mmgui_netlink_connection_t;
/*Changed parameters of connection */
struct _mmgui_netlink_connection_changed_params {
	guchar state;
	guint dqueue;
};

typedef struct _mmgui_netlink_connection_changed_params *mmgui_netlink_connection_changed_params_t;
/*Unified connection changes structure*/
struct _mmgui_netlink_connection_change {
	guint inode;
	guint event;
	union {
		mmgui_netlink_connection_changed_params_t params;
		mmgui_netlink_connection_t connection;
	} data;
};

typedef struct _mmgui_netlink_connection_change *mmgui_netlink_connection_change_t;


struct _mmgui_netlink_connection_info_request {
	struct nlmsghdr msgheader;
	struct inet_diag_req nlreq;
};

struct _mmgui_netlink_interface_info_request {
	struct nlmsghdr msgheader;
	struct ifinfomsg ifinfo;
};

struct _mmgui_netlink_interface_event {
	enum _mmgui_netlink_interface_event_type type;
	gchar ifname[IFNAMSIZ];
	gboolean running;
	gboolean up;
	guint64 rxbytes;
	guint64 txbytes;
};

typedef struct _mmgui_netlink_interface_event *mmgui_netlink_interface_event_t;

struct _mmgui_netlink {
	//Connections monitoring
	gint connsocketfd;
	pid_t userid;
	time_t currenttime;
	GHashTable *connections;
	GAsyncQueue *changequeue; //single queue for now
	struct sockaddr_nl connaddr;
	//Network interfaces monitoring
	gint intsocketfd;
	struct sockaddr_nl intaddr;
};

typedef struct _mmgui_netlink *mmgui_netlink_t;

gboolean mmgui_netlink_terminate_application(pid_t pid);
gchar *mmgui_netlink_socket_state(guchar state);
gboolean mmgui_netlink_update(mmgui_netlink_t netlink);
gboolean mmgui_netlink_request_connections_list(mmgui_netlink_t netlink, guint family);
gboolean mmgui_netlink_read_connections_list(mmgui_netlink_t netlink, gchar *data, gsize datasize);
gboolean mmgui_netlink_request_interface_statistics(mmgui_netlink_t netlink, gchar *interface);
gboolean mmgui_netlink_read_interface_event(mmgui_netlink_t netlink, gchar *data, gsize datasize, mmgui_netlink_interface_event_t event);
gint mmgui_netlink_get_connections_monitoring_socket_fd(mmgui_netlink_t netlink);
gint mmgui_netlink_get_interfaces_monitoring_socket_fd(mmgui_netlink_t netlink);
struct sockaddr_nl *mmgui_netlink_get_connections_monitoring_socket_address(mmgui_netlink_t netlink);
struct sockaddr_nl *mmgui_netlink_get_interfaces_monitoring_socket_address(mmgui_netlink_t netlink);
void mmgui_netlink_free_connection_change(mmgui_netlink_connection_change_t change);
void mmgui_netlink_free_connection(mmgui_netlink_connection_t connection);
GSList *mmgui_netlink_open_interactive_connections_list(mmgui_netlink_t netlink);
void mmgui_netlink_close_interactive_connections_list(mmgui_netlink_t netlink);
GSList *mmgui_netlink_get_connections_changes(mmgui_netlink_t netlink);
void mmgui_netlink_close(mmgui_netlink_t netlink);
mmgui_netlink_t mmgui_netlink_open(void);

#endif /* __NETLINK_H__ */
