/*
 *      netlink.c
 *      
 *      Copyright 2012-2017 Alex <alex@linuxonly.ru>
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
 
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/inet_diag.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <net/if.h>
#include <limits.h>

#include "netlink.h"

#define MMGUI_NETLINK_INTERNAL_SEQUENCE_NUMBER 100000


static gboolean mmgui_netlink_numeric_name(gchar *dirname);
static gboolean mmgui_netlink_process_access(gchar *dirname, uid_t uid);
static gboolean mmgui_netlink_socket_access(gchar *dirname, gchar *sockname, guint inode);
static gchar *mmgui_netlink_process_name(gchar *dirname, gchar *appname, gsize appsize);
static gboolean mmgui_netlink_get_process(guint inode, gchar *appname, gsize namesize, pid_t *apppid);
static gboolean mmgui_netlink_hash_clear_foreach(gpointer key, gpointer value, gpointer user_data);
struct sockaddr_nl *mmgui_netlink_get_connections_monitoring_socket_address(mmgui_netlink_t netlink);
struct sockaddr_nl *mmgui_netlink_get_interfaces_monitoring_socket_address(mmgui_netlink_t netlink);
static mmgui_netlink_connection_change_t mmgui_netlink_create_connection_change(mmgui_netlink_t netlink, guint event, guint inode);


static gboolean mmgui_netlink_numeric_name(gchar *dirname)
{
	if ((dirname == NULL) || (!*dirname)) return FALSE;
	
	while (*dirname) {
		if (!isdigit(*dirname)) {
			return FALSE;
		} else {
			(void)*dirname++;
		}
	}
		
	return TRUE;
}

static gboolean mmgui_netlink_process_access(gchar *dirname, uid_t uid)
{
	gchar fullpath[PATH_MAX];
	struct stat pathstat;
	
	if (!mmgui_netlink_numeric_name(dirname)) return FALSE;
	
	memset(fullpath, 0, sizeof(fullpath));
	
	sprintf(fullpath, "/proc/%s", dirname);
	
	if (stat(fullpath, &pathstat) == -1) {
		return FALSE;
	}
	
	if (pathstat.st_uid != uid) {
		return FALSE;
	}
	
	return TRUE;
}

static gboolean mmgui_netlink_socket_access(gchar *dirname, gchar *sockname, guint inode)
{
	gchar fullpath[PATH_MAX];
	struct stat fdstat;
	
	if (!mmgui_netlink_numeric_name(sockname)) return FALSE;
		
	memset(fullpath, 0, sizeof(fullpath));
	
	snprintf(fullpath, sizeof(fullpath), "/proc/%s/fd/%s", dirname, sockname);
	
	if (stat(fullpath, &fdstat) == -1) {
		return FALSE;
	}
	
	if (((fdstat.st_mode & S_IFMT) == S_IFSOCK) && (fdstat.st_ino == inode)) {
		return TRUE;
	}
	
	return FALSE;
}

static gchar *mmgui_netlink_process_name(gchar *dirname, gchar *appname, gsize appsize)
{
	gint fd, i;
	gchar fpath[PATH_MAX];
	ssize_t linkchars;
		
	if ((dirname == NULL) || (dirname[0] == '\0')) return NULL;
	if ((appname == NULL) || (appsize == 0)) return NULL;
	
	memset(fpath, 0, sizeof(fpath));
	snprintf(fpath, sizeof(fpath), "/proc/%s/exe", dirname);
	
	linkchars = readlink(fpath, appname, appsize-1);
	
	if (linkchars == 0) {
		memset(fpath, 0, sizeof(fpath));
		snprintf(fpath, sizeof(fpath), "/proc/%s/comm", dirname);
		
		fd = open(fpath, O_RDONLY);
		if (fd != -1) {
			linkchars = read(fd, appname, appsize-1);
			close(fd);
		} else {
			return NULL;
		}
	}
		
	appname[linkchars] = '\0';
	
	for (i=linkchars; i>=0; i--) {
		if (appname[i] == '/') {
			memmove(appname+0, appname+i+1, linkchars-i-1);
			linkchars -= i+1;
			break;
		}
	}
	
	appname[linkchars] = '\0';
	
	return appname;
}

static gboolean mmgui_netlink_get_process(guint inode, gchar *appname, gsize namesize, pid_t *apppid)
{
	DIR *procdir, *fddir;
	struct dirent *procde, *fdde;
	gchar fdirpath[PATH_MAX];
	
	if ((appname == NULL) || (namesize == 0) || (apppid == NULL)) return FALSE;
	
	procdir = opendir("/proc");
	if (procdir != NULL) {
		while ((procde = readdir(procdir))) {
			if (mmgui_netlink_process_access(procde->d_name, getuid())) {
				memset(fdirpath, 0, sizeof(fdirpath));
				snprintf(fdirpath, sizeof(fdirpath), "/proc/%s/fd", procde->d_name);
				//enumerate file descriptors
				fddir = opendir(fdirpath);
				if (fddir != NULL) {
					while ((fdde = readdir(fddir))) {
						if (mmgui_netlink_socket_access(procde->d_name, fdde->d_name, inode)) {
							//printf("%s:%s (%s)\n", procde->d_name, fdde->d_name, nlconnections_process_name(procde->d_name, appname, sizeof(appname)));
							*apppid = atoi(procde->d_name);
							mmgui_netlink_process_name(procde->d_name, appname, namesize);
							closedir(fddir);
							closedir(procdir);
							return TRUE;
						}
					}
					closedir(fddir);
				}
			}
		}
		closedir(procdir);
	}
	
	return FALSE;
}

gboolean mmgui_netlink_terminate_application(pid_t pid)
{
	if (kill(pid, 0) == 0) {
		if (kill(pid, SIGTERM) == 0) {
			return TRUE;
		}
	}
	
	return FALSE;
}

gchar *mmgui_netlink_socket_state(guchar state)
{
	switch (state) {
		case TCP_ESTABLISHED:
			return "Established";
		case TCP_SYN_SENT:
			return "SYN sent";
		case TCP_SYN_RECV:
			return "SYN recv";
		case TCP_FIN_WAIT1:
			return "FIN wait";
		case TCP_FIN_WAIT2:
			return "FIN wait";
		case TCP_TIME_WAIT:
			return "Time wait";
		case TCP_CLOSE:
			return "Close";
		case TCP_CLOSE_WAIT:
			return "Close wait";
		case TCP_LAST_ACK:
			return "Last ACK";
		case TCP_LISTEN:
			return "Listen";
		case TCP_CLOSING:
			return "Closing";
		default:
			return "Unknown";
	}
}

void mmgui_netlink_hash_destroy(gpointer data)
{
	mmgui_netlink_connection_t connection;
	
	connection = (mmgui_netlink_connection_t)data;
	
	if (connection == NULL) return;
	
	if (connection->appname != NULL) g_free(connection->appname);
	if (connection->dsthostname != NULL) g_free(connection->dsthostname);
	
	g_free(connection);
}

static gboolean mmgui_netlink_hash_clear_foreach(gpointer key, gpointer value, gpointer user_data)
{
	mmgui_netlink_connection_t connection;
	mmgui_netlink_t netlink;
	mmgui_netlink_connection_change_t change;
	
	connection = (mmgui_netlink_connection_t)value;
	netlink = (mmgui_netlink_t)user_data;
	
	if (connection->updatetime == netlink->currenttime) {
		return FALSE;
	} else {
		if (netlink->changequeue != NULL) {
			change = mmgui_netlink_create_connection_change(netlink, MMGUI_NETLINK_CONNECTION_EVENT_REMOVE, connection->inode);
			if (change != NULL) {
				g_async_queue_push(netlink->changequeue, change);
			}								
		}
		
		g_debug("Connection removed: inode %u\n", connection->inode);
		return TRUE;
	}
}

gboolean mmgui_netlink_request_connections_list(mmgui_netlink_t netlink, guint family)
{
	struct _mmgui_netlink_connection_info_request request;
	gint status;
		
	if ((netlink == NULL) || ((family != AF_INET) && (family != AF_INET6))) return FALSE;
	
	memset(&request.msgheader, 0, sizeof(struct nlmsghdr));
	
	request.msgheader.nlmsg_len = sizeof(struct _mmgui_netlink_connection_info_request);
	request.msgheader.nlmsg_type = TCPDIAG_GETSOCK;
	request.msgheader.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
	request.msgheader.nlmsg_pid = ((1 & 0xffc00000) << 22) | (getpid() & 0x3fffff);
	request.msgheader.nlmsg_seq = 0;
	
	memset(&request.nlreq, 0, sizeof(struct inet_diag_req));
	request.nlreq.idiag_family = family;
	request.nlreq.idiag_states = ((1 << (TCP_CLOSING + 1)) - 1);
	
	request.msgheader.nlmsg_len = NLMSG_ALIGN(request.msgheader.nlmsg_len);
	
	status = send(netlink->connsocketfd, &request, request.msgheader.nlmsg_len, 0);
	
	if (status != -1) {
		return TRUE;
	} else {
		return FALSE;
	}
}

gboolean mmgui_netlink_read_connections_list(mmgui_netlink_t netlink, gchar *data, gsize datasize)
{
	struct nlmsghdr *msgheader;
	struct inet_diag_msg *entry;
	mmgui_netlink_connection_t connection;
	mmgui_netlink_connection_change_t change;
	/*struct hostent *dsthost;*/
	gchar srcbuf[INET6_ADDRSTRLEN];
	gchar dstbuf[INET6_ADDRSTRLEN];
	gchar appname[1024];
	pid_t apppid;
	gboolean needupdate;
			
	if ((netlink == NULL) || (data == NULL) || (datasize == 0)) return FALSE;
	
	//Get current time
	netlink->currenttime = time(NULL);
	
	//Work with data
	for (msgheader = (struct nlmsghdr *)data; NLMSG_OK(msgheader, (unsigned int)datasize); msgheader = NLMSG_NEXT(msgheader, datasize)) {
		if ((msgheader->nlmsg_type == NLMSG_ERROR) || (msgheader->nlmsg_type == NLMSG_DONE)) {
			break;
		}
		//New connections list
		if (msgheader->nlmsg_type == TCPDIAG_GETSOCK) {
			entry = (struct inet_diag_msg *)NLMSG_DATA(msgheader);
			if (entry != NULL) {
				if ((entry->idiag_uid == netlink->userid) || (netlink->userid == 0)) {
					if (!g_hash_table_contains(netlink->connections, (gconstpointer)&entry->idiag_inode)) {
						//Add new connection
						if (mmgui_netlink_get_process(entry->idiag_inode, appname, sizeof(appname), &apppid)) {
							connection = g_new(struct _mmgui_netlink_connection, 1);
							connection->inode = entry->idiag_inode;
							connection->family = entry->idiag_family;
							connection->userid = entry->idiag_uid;
							connection->updatetime = netlink->currenttime;
							connection->dqueue = entry->idiag_rqueue + entry->idiag_wqueue;
							connection->state = entry->idiag_state;
							connection->srcport = ntohs(entry->id.idiag_sport);
							g_snprintf(connection->srcaddr, sizeof(connection->srcaddr), "%s:%u", inet_ntop(entry->idiag_family, entry->id.idiag_src, srcbuf, INET6_ADDRSTRLEN), ntohs(entry->id.idiag_sport));
							g_snprintf(connection->dstaddr, sizeof(connection->dstaddr), "%s:%u", inet_ntop(entry->idiag_family, entry->id.idiag_dst, dstbuf, INET6_ADDRSTRLEN), ntohs(entry->id.idiag_dport));
							connection->appname = g_strdup(appname);
							connection->apppid = apppid;
							connection->dsthostname = NULL;
							/*dsthost = gethostbyaddr(entry->id.idiag_dst, sizeof(entry->id.idiag_dst), entry->idiag_family);
							if (dsthost != NULL) {
								connection->dsthostname = g_strdup(dsthost->h_name);
							} else {*/
								connection->dsthostname = g_strdup(connection->dstaddr);
							/*}*/
							g_hash_table_insert(netlink->connections, (gpointer)&connection->inode, connection);
							/*Add change*/
							if (netlink->changequeue != NULL) {
								change = mmgui_netlink_create_connection_change(netlink, MMGUI_NETLINK_CONNECTION_EVENT_ADD, connection->inode);
								if (change != NULL) {
									g_async_queue_push(netlink->changequeue, change);
								}								
							}
							g_debug("Connection added: inode %u\n", entry->idiag_inode);
						}
					} else {
						//Update connection information (state, buffers fill, time)
						connection = g_hash_table_lookup(netlink->connections, (gconstpointer)&entry->idiag_inode);
						if (connection != NULL) {
							connection->updatetime = netlink->currenttime;
							needupdate = FALSE;
							if (connection->dqueue != (entry->idiag_rqueue + entry->idiag_wqueue)) {
								connection->dqueue = entry->idiag_rqueue + entry->idiag_wqueue;
								needupdate = TRUE;
							}
							if (connection->state != entry->idiag_state) {
								connection->state = entry->idiag_state;
								needupdate = TRUE;
							}
							if (needupdate) {
								if (netlink->changequeue != NULL) {
									change = mmgui_netlink_create_connection_change(netlink, MMGUI_NETLINK_CONNECTION_EVENT_MODIFY, connection->inode);
									if (change != NULL) {
										g_async_queue_push(netlink->changequeue, change);
									}								
								}
								g_debug("Connection updated: inode %u\n", entry->idiag_inode);
							}
						}	
					}
				}
			}
		}
	}
	
	//Remove connections that disappear
	g_hash_table_foreach_remove(netlink->connections, mmgui_netlink_hash_clear_foreach, netlink);
		
	return TRUE;
}

gboolean mmgui_netlink_request_interface_statistics(mmgui_netlink_t netlink, gchar *interface)
{
	struct _mmgui_netlink_interface_info_request request;
	guint ifindex;
	gint status;
		
	if ((netlink == NULL) || (interface == NULL)) return FALSE;
	if ((netlink->intsocketfd == -1) || (interface[0] == '\0')) return FALSE;
	
	ifindex = if_nametoindex(interface);
	
	if ((ifindex == 0) && (errno == ENXIO)) return FALSE;
	
	memset(&request, 0, sizeof(request));
	
	request.msgheader.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	request.msgheader.nlmsg_type = RTM_GETLINK;
	request.msgheader.nlmsg_flags = NLM_F_REQUEST;
	request.msgheader.nlmsg_seq = MMGUI_NETLINK_INTERNAL_SEQUENCE_NUMBER;
	request.msgheader.nlmsg_pid = ((2 & 0xffc00000) << 22) | (getpid() & 0x3fffff);
	
	request.ifinfo.ifi_family = AF_UNSPEC;
	request.ifinfo.ifi_index = ifindex;
	request.ifinfo.ifi_type = 0;
	request.ifinfo.ifi_flags = 0;
	request.ifinfo.ifi_change = 0xFFFFFFFF;
	
	request.msgheader.nlmsg_len = NLMSG_ALIGN(request.msgheader.nlmsg_len);
	
	status = send(netlink->intsocketfd, &request, request.msgheader.nlmsg_len, 0);
	
	if (status != -1) {
		return TRUE;
	} else {
		return FALSE;
	}
}

gboolean mmgui_netlink_read_interface_event(mmgui_netlink_t netlink, gchar *data, gsize datasize, mmgui_netlink_interface_event_t event)
{
	struct nlmsghdr *msgheader;
	struct ifinfomsg *ifi;
	struct rtattr *rta;
	struct rtnl_link_stats *ifstats;
	struct rtnl_link_stats64 *ifstats64;
	gchar ifname[IFNAMSIZ];
	gboolean have64bitstats;
	
	if ((netlink == NULL) || (data == NULL) || (datasize == 0) || (event == NULL)) return FALSE;
	
	//Work with data
	for (msgheader = (struct nlmsghdr *)data; NLMSG_OK(msgheader, (unsigned int)datasize); msgheader = NLMSG_NEXT(msgheader, datasize)) {
		if ((msgheader->nlmsg_type == NLMSG_ERROR) || (msgheader->nlmsg_type == NLMSG_DONE)) {
			break;
		}
		//New event
		if ((msgheader->nlmsg_type == RTM_NEWLINK) || (msgheader->nlmsg_type == RTM_DELLINK) || (msgheader->nlmsg_type == RTM_GETLINK)) {
			ifi = NLMSG_DATA(msgheader);
			rta = IFLA_RTA(ifi);
			//Copy valuable data first
			event->running = (ifi->ifi_flags & IFF_RUNNING);
			event->up = (ifi->ifi_flags & IFF_UP);
			if (if_indextoname(ifi->ifi_index, ifname) != NULL) {
				strncpy(event->ifname, ifname, sizeof(event->ifname));
			}
			event->type = MMGUI_NETLINK_INTERFACE_EVENT_TYPE_UNKNOWN;
			//Detrmine type of event
			if (msgheader->nlmsg_seq == MMGUI_NETLINK_INTERNAL_SEQUENCE_NUMBER) {
				event->type = MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS;
			} else {
				if (msgheader->nlmsg_type == RTM_NEWLINK) {
					event->type = MMGUI_NETLINK_INTERFACE_EVENT_TYPE_ADD;
					g_debug("[%u] New interface state: Running: %s, Up: %s, Name: %s\n", msgheader->nlmsg_seq, (ifi->ifi_flags & IFF_RUNNING) ? "Yes" : "No", (ifi->ifi_flags & IFF_UP) ? "Yes" : "No",  if_indextoname(ifi->ifi_index, ifname));
				} else if (msgheader->nlmsg_type == RTM_DELLINK) {
					event->type = MMGUI_NETLINK_INTERFACE_EVENT_TYPE_REMOVE;
					g_debug("[%u] Deleted interface state: Running: %s, Up: %s, Name: %s\n", msgheader->nlmsg_seq, (ifi->ifi_flags & IFF_RUNNING) ? "Yes" : "No", (ifi->ifi_flags & IFF_UP) ? "Yes" : "No",  if_indextoname(ifi->ifi_index, ifname));
				} else if (msgheader->nlmsg_type == RTM_GETLINK) {
					event->type = MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS;
					g_debug("[%u] Requested interface state: Running: %s, Up: %s, Name: %s\n", msgheader->nlmsg_seq, (ifi->ifi_flags & IFF_RUNNING) ? "Yes" : "No", (ifi->ifi_flags & IFF_UP) ? "Yes" : "No",  if_indextoname(ifi->ifi_index, ifname));
				}
			} 
			//If 64bit traffic statistics values are not available, 32bit values will be used anyway
			have64bitstats = FALSE;
			//Use tags to get additional data
			while (RTA_OK(rta, msgheader->nlmsg_len)) {
				if (rta->rta_type == IFLA_IFNAME) {
					strncpy(event->ifname, (char *)RTA_DATA(rta), sizeof(event->ifname));
					g_debug("Tag: Device name: %s\n", (char *)RTA_DATA(rta));
				} else if (rta->rta_type == IFLA_STATS) {
					ifstats = (struct rtnl_link_stats *)RTA_DATA(rta);
					if (!have64bitstats) {
						event->rxbytes = ifstats->rx_bytes;
						event->txbytes = ifstats->tx_bytes;
						if (!(event->type & MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS)) {
							event->type |= MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS;
						}
					}
					g_debug("Tag: Interface Statistics (32bit): RX: %u, TX: %u\n", ifstats->rx_bytes, ifstats->tx_bytes);
				} else if (rta->rta_type == IFLA_STATS64) {
					ifstats64 = (struct rtnl_link_stats64 *)RTA_DATA(rta);
					event->rxbytes = ifstats64->rx_bytes;
					event->txbytes = ifstats64->tx_bytes;
					have64bitstats = TRUE;
					if (!(event->type & MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS)) {
						event->type |= MMGUI_NETLINK_INTERFACE_EVENT_TYPE_STATS;
					}
					g_debug("Tag: Interface Statistics (64bit): RX: %llu, TX: %llu\n", ifstats64->rx_bytes, ifstats64->tx_bytes);
				} else if (rta->rta_type == IFLA_LINK) {
					g_debug("Tag: Link type\n");
				} else if (rta->rta_type == IFLA_ADDRESS) {
					g_debug("Tag: interface L2 address\n");
				} else if (rta->rta_type == IFLA_UNSPEC) {
					g_debug("Tag: unspecified\n");
				} else {
					g_debug("Tag: %u\n", rta->rta_type);
				}
				rta = RTA_NEXT(rta, msgheader->nlmsg_len);
			}
		}
	}
	
	return TRUE;
}

gint mmgui_netlink_get_connections_monitoring_socket_fd(mmgui_netlink_t netlink)
{
	if (netlink == NULL) return -1;
	
	return netlink->connsocketfd;
}

gint mmgui_netlink_get_interfaces_monitoring_socket_fd(mmgui_netlink_t netlink)
{
	if (netlink == NULL) return -1;
	
	return netlink->intsocketfd;
}

struct sockaddr_nl *mmgui_netlink_get_connections_monitoring_socket_address(mmgui_netlink_t netlink)
{
	if (netlink == NULL) return NULL;
	
	return &(netlink->connaddr);
}

struct sockaddr_nl *mmgui_netlink_get_interfaces_monitoring_socket_address(mmgui_netlink_t netlink)
{
	if (netlink == NULL) return NULL;
	
	return &(netlink->intaddr);
}

static mmgui_netlink_connection_change_t mmgui_netlink_create_connection_change(mmgui_netlink_t netlink, guint event, guint inode)
{
	mmgui_netlink_connection_change_t change;
	mmgui_netlink_connection_t connection;
	
	if (netlink == NULL) return NULL;
	
	change = NULL;
	
	switch (event) {
		case MMGUI_NETLINK_CONNECTION_EVENT_ADD:
			connection = g_hash_table_lookup(netlink->connections, &inode);
			if (connection != NULL) {
				change = g_new0(struct _mmgui_netlink_connection_change, 1);
				change->inode = inode;
				change->event = event;
				change->data.connection = g_new0(struct _mmgui_netlink_connection, 1);
				change->data.connection->state = connection->state;
				change->data.connection->family = connection->family;
				change->data.connection->srcport = connection->srcport;
				change->data.connection->dqueue = connection->dqueue;
				change->data.connection->inode = connection->inode;
				change->data.connection->userid = connection->userid;
				change->data.connection->apppid = connection->apppid;
				change->data.connection->appname = g_strdup(connection->appname);
				change->data.connection->dsthostname = g_strdup(connection->dsthostname);
				strncpy(change->data.connection->srcaddr, connection->srcaddr, sizeof(change->data.connection->srcaddr));
				strncpy(change->data.connection->dstaddr, connection->dstaddr, sizeof(change->data.connection->dstaddr));
			}
			break;
		case MMGUI_NETLINK_CONNECTION_EVENT_REMOVE:
			change = g_new0(struct _mmgui_netlink_connection_change, 1);
			change->inode = inode;
			change->event = event;
			change->data.connection = NULL;
			break;
		case MMGUI_NETLINK_CONNECTION_EVENT_MODIFY:
			connection = g_hash_table_lookup(netlink->connections, &inode);
			if (connection != NULL) {
				change = g_new0(struct _mmgui_netlink_connection_change, 1);
				change->inode = inode;
				change->event = event;
				change->data.params = g_new0(struct _mmgui_netlink_connection_changed_params, 1);
				change->data.params->state = connection->state;
				change->data.params->dqueue = connection->dqueue;
			}
			break;
		 default:
			break;
	}
	
	return change;
}

void mmgui_netlink_free_connection_change(mmgui_netlink_connection_change_t change)
{
	if (change == NULL) return;
	
	switch (change->event) {
		case MMGUI_NETLINK_CONNECTION_EVENT_ADD:
			if (change->data.connection != NULL) {
				if (change->data.connection->appname != NULL) {
					g_free(change->data.connection->appname);
				}
				if (change->data.connection->dsthostname != NULL) {
					g_free(change->data.connection->dsthostname);
				}
				g_free(change->data.connection);
			}
			g_free(change);
			break;
		case MMGUI_NETLINK_CONNECTION_EVENT_REMOVE:
			g_free(change);
			break;
		case MMGUI_NETLINK_CONNECTION_EVENT_MODIFY:
			if (change->data.params != NULL) {
				g_free(change->data.params);				
			}
			g_free(change);
			break;
		 default:
			break;
	}
}

void mmgui_netlink_free_connection(mmgui_netlink_connection_t connection)
{
	if (connection == NULL) return;
	
	if (connection->appname != NULL) {
		g_free(connection->appname);
	}
	
	if (connection->dsthostname != NULL) {
		g_free(connection->dsthostname);
	}
	
	g_free(connection);
}

GSList *mmgui_netlink_open_interactive_connections_list(mmgui_netlink_t netlink)
{
	GSList *connections;
	GHashTableIter iter;
	gpointer key, value;
	mmgui_netlink_connection_t srcconn, dstconn;
	
	if (netlink == NULL) return NULL;
	
	connections = NULL;
	
	g_hash_table_iter_init(&iter, netlink->connections);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		srcconn = (mmgui_netlink_connection_t)value;
		if (srcconn != NULL) {
			dstconn = g_new(struct _mmgui_netlink_connection, 1);
			dstconn->inode = srcconn->inode;
			dstconn->family = srcconn->family;
			dstconn->userid = srcconn->userid;
			dstconn->updatetime = srcconn->updatetime;
			dstconn->dqueue = srcconn->dqueue;
			dstconn->state = srcconn->state;
			dstconn->srcport = srcconn->srcport;
			strncpy(dstconn->srcaddr, srcconn->srcaddr, sizeof(dstconn->srcaddr));
			strncpy(dstconn->dstaddr, srcconn->dstaddr, sizeof(dstconn->dstaddr));
			dstconn->appname = g_strdup(srcconn->appname);
			dstconn->apppid = srcconn->apppid;
			dstconn->dsthostname = g_strdup(srcconn->dsthostname);
			connections = g_slist_prepend(connections, dstconn);
		}
	}
	if (connections != NULL) {
		connections = g_slist_reverse(connections);
	}
	
	if (netlink->changequeue == NULL) {
		netlink->changequeue = g_async_queue_new_full((GDestroyNotify)mmgui_netlink_free_connection_change);
	}
	
	return connections;
}

void mmgui_netlink_close_interactive_connections_list(mmgui_netlink_t netlink)
{
	if (netlink->changequeue != NULL) {
		g_async_queue_unref(netlink->changequeue);
		netlink->changequeue = NULL;
	}
}

GSList *mmgui_netlink_get_connections_changes(mmgui_netlink_t netlink)
{
	GSList *changes;
	mmgui_netlink_connection_change_t change;
	
	if (netlink->changequeue == NULL) return NULL;
	
	changes = NULL;
	
	while ((change = g_async_queue_try_pop(netlink->changequeue)) != NULL) {
		changes = g_slist_prepend(changes, change);
	}
	
	if (changes != NULL) {
		changes = g_slist_reverse(changes);
	}
	
	return changes;
}

void mmgui_netlink_close(mmgui_netlink_t netlink)
{
	if (netlink == NULL) return;
	
	if (netlink->connsocketfd != -1) {
		close(netlink->connsocketfd);
		g_hash_table_destroy(netlink->connections);
		if (netlink->changequeue != NULL) {
			g_async_queue_unref(netlink->changequeue);
		}
	}
	
	if (netlink->intsocketfd != -1) {
		close(netlink->intsocketfd);
	}
	
	g_free(netlink);
}

mmgui_netlink_t mmgui_netlink_open(void)
{
	mmgui_netlink_t netlink;
		
	netlink = g_new(struct _mmgui_netlink, 1);
	
	#ifndef NETLINK_SOCK_DIAG
		netlink->connsocketfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
	#else
		netlink->connsocketfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
	#endif
	
	if (netlink->connsocketfd != -1) {
		memset(&netlink->connaddr, 0, sizeof(struct sockaddr_nl));
		netlink->connaddr.nl_family = AF_NETLINK;
		netlink->connaddr.nl_pid = ((1 & 0xffc00000) << 22) | (getpid() & 0x3fffff);
		netlink->connaddr.nl_groups = 0;
		
		netlink->userid = getuid();
		netlink->changequeue = NULL;
		netlink->connections = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, (GDestroyNotify)mmgui_netlink_hash_destroy);
	} else {
		netlink->connections = NULL;
		g_debug("Failed to open connections monitoring netlink socket\n");
	}
	
	netlink->intsocketfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	
	if (netlink->intsocketfd != -1) {
		memset(&netlink->intaddr, 0, sizeof(netlink->intaddr));
		netlink->intaddr.nl_family = AF_NETLINK;
		netlink->intaddr.nl_groups = RTMGRP_LINK;
		netlink->intaddr.nl_pid = ((2 & 0xffc00000) << 22) | (getpid() & 0x3fffff);
		
		if (bind(netlink->intsocketfd, (struct sockaddr *)&(netlink->intaddr), sizeof(netlink->intaddr)) == -1) {
			g_debug("Failed to bind network interfaces monitoring netlink socket\n");
			close(netlink->intsocketfd);
			netlink->intsocketfd = -1;
		}
	} else {
		g_debug("Failed to open network interfaces monitoring netlink socket\n");
	}
	
	return netlink;
}
