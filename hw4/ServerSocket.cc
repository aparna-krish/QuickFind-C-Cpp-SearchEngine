/*
 * Copyright ©2025 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2025 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::string;
using std::to_string;
using std::cerr;
using std::endl;

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int *const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:


  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = ai_family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE | AI_V4MAPPED;
  hints.ai_protocol = IPPROTO_TCP;

  string port_string = to_string(this -> port_);

  // Get list of potential addresses to bind to
  // If successful addr_list will point to first address structure
  struct addrinfo* addr_list = nullptr;
  int addrinfo_status = getaddrinfo(nullptr, port_string.c_str(), &hints, &addr_list);
  if (addrinfo_status != 0) {
    cerr << "getaddrinfo() failed: " << gai_strerror(addrinfo_status) << endl;
    return false;
  }

  int server_fd = -1;
  int reuse_flag = 1;
  struct addrinfo* current_addr = addr_list;

  
  // Iterate over each address structure until successful binding
  while (current_addr != nullptr) {
    // Create new socket
    server_fd = socket(current_addr -> ai_family, current_addr -> ai_socktype, current_addr -> ai_protocol);
    if (server_fd == -1) {
      cerr << "socket() error: " << strerror(errno) << endl;
      current_addr = current_addr -> ai_next;
      continue;
    }

    // Set socket options to allow immediate reuse
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(reuse_flag)) < 0) {
      cerr << "setsockopt() error: " << strerror(errno) << endl;
      close(server_fd);
      server_fd = -1;
      current_addr = current_addr -> ai_next;
      continue;
    }

    // Try to bind socket
    if (bind(server_fd, current_addr -> ai_addr, current_addr -> ai_addrlen) == 0) {
      // If binding succeeded
      this -> sock_family_ = current_addr -> ai_family;
      break;
    }

    // If binding failed clean up and try next address
    cerr << "bind() error: " << strerror(errno) << endl;
    close(server_fd);
    server_fd = -1;
    current_addr = current_addr -> ai_next;
  }

  freeaddrinfo(addr_list);

  // Final check if binding succeeded
  if (server_fd == -1) {
    cerr << "Unable to bind to any address." << endl;
    return false;
  }

  // Set up the socket for listening
  if (listen(server_fd, SOMAXCONN) != 0) {
    cerr << "listen() error: " << strerror(errno) << endl;
    close(server_fd);
    return false;
  }

  // Save file descriptor for listening socket
  this -> listen_sock_fd_ = server_fd;
  *listen_fd = server_fd;

  return true;
}

bool ServerSocket::Accept(int *const accepted_fd,
                          string *const client_addr,
                          uint16_t *const client_port,
                          string *const client_dns_name,
                          string *const server_addr,
                          string *const server_dns_name) const {
  // STEP 2:


  // Accept incoming connection on listening socket
  int client_fd;
  struct sockaddr_storage client_sockaddr;
  socklen_t client_addr_len = sizeof(client_sockaddr);

  // Keep trying to accept in case of temporary errors
  bool accepted = false;
  while (!accepted) {
    client_fd = accept(this->listen_sock_fd_, reinterpret_cast<struct sockaddr*>(&client_sockaddr), &client_addr_len);

    if (client_fd < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        // Temporary error loop again
      } else {
        cerr << "accept() error: " << strerror(errno) << endl;
        close(this->listen_sock_fd_);
        return false;
      }
    }
    
    else {
      accepted = true;
    }
  }

  // Updated accepted socket descriptor
  *accepted_fd = client_fd;

  // Convert and store client IP address and port
  char ip_buffer[INET6_ADDRSTRLEN];
  if (client_sockaddr.ss_family == AF_INET) {
    struct sockaddr_in* ipv4_addr = reinterpret_cast<struct sockaddr_in*>(&client_sockaddr);
    inet_ntop(AF_INET, &(ipv4_addr -> sin_addr), ip_buffer, INET_ADDRSTRLEN);
    *client_addr = string(ip_buffer);
    // Host byte order needed
    *client_port = ntohs(ipv4_addr -> sin_port);
  } else {
    struct sockaddr_in6* ipv6_addr = reinterpret_cast<struct sockaddr_in6*>(&client_sockaddr);
    inet_ntop(AF_INET6, &(ipv6_addr -> sin6_addr), ip_buffer, INET6_ADDRSTRLEN);
    *client_addr = string(ip_buffer);
    *client_port = ntohs(ipv6_addr -> sin6_port);
  }

  // Perform reverse DNS lookup on client
  char hostname_buf[NI_MAXHOST] = {0};
  if (getnameinfo(reinterpret_cast<struct sockaddr*>(&client_sockaddr), client_addr_len, hostname_buf, NI_MAXHOST, nullptr, 0, 0) != 0) {
    snprintf(hostname_buf, NI_MAXHOST, "[reverse DNS unavailable]");
  }
  *client_dns_name = string(hostname_buf);

  // Determine server own address/hostname
  char self_ip[INET6_ADDRSTRLEN];
char self_host[NI_MAXHOST] = {0};
socklen_t local_len;

// Create sockaddr_storage to hold either IPv4 or IPv6
struct sockaddr_storage local_addr;
local_len = sizeof(local_addr);

if (getsockname(client_fd, reinterpret_cast<struct sockaddr*>(&local_addr), &local_len) != 0) {
  cerr << "getsockname() error: " << strerror(errno) << endl;
} else {
  void* addr_ptr = nullptr;
  if (local_addr.ss_family == AF_INET) {
    addr_ptr = &reinterpret_cast<struct sockaddr_in*>(&local_addr) -> sin_addr;
  } else if (local_addr.ss_family == AF_INET6) {
    addr_ptr = &reinterpret_cast<struct sockaddr_in6*>(&local_addr) -> sin6_addr;
  }

  if (addr_ptr) {
    inet_ntop(local_addr.ss_family, addr_ptr, self_ip, sizeof(self_ip));

    getnameinfo(reinterpret_cast<struct sockaddr*>(&local_addr), local_len, self_host, NI_MAXHOST, nullptr, 0, 0);
  }
}

  // Save server IP and DNS name
  *server_addr = string(self_ip);
  *server_dns_name = string(self_host);

  return true;
}

}  // namespace hw4
