
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#define SELF_TCP_PORT "34125" // the port client will be connecting to
#define SELF_UDP_PORT "33125"
#define A_PORT "30125"
#define B_PORT "31125"
#define C_PORT "32125"
#define IP "127.0.0.1" 
#define BACKLOG 10 // how many pending connections queue will hold
#define MAXDATASIZE 1000000 // max number of bytes we can get at once
#define MAX_LEN_IN_CHAR 700 // 693 actually

using namespace std;

struct query {
    char map_id;
    int src_vtx;
    int des_vtx;
    int file_size;
};

struct edge_info {
    int node1;
    int node2;
    float dist;
    //struct graph_info *next;
};

struct answer {
    float min_len;
    float Tt;
    float Tp;
    float delay;
    //struct node *path;
    vector<int> path;
    bool map_not_found;
    bool vtx_not_found;
    int missing_vtx;
};

struct answer_AB {
    // int map_not_found_A;
    // int map_not_found_B;
    float prop_spd;
    int trans_spd;
    vector<struct edge_info> edges;
};

struct answer_AB string2ansab (string s) {
    struct answer_AB ansab;
    istringstream iss(s);
    string line;
    getline(iss, line); // skip prop_spd line
    ansab.prop_spd = stof(line);
    getline(iss, line); // skip trans_spd line
    ansab.trans_spd = stoi(line);

    while (getline(iss, line)) {
        istringstream iss_l(line);
        struct edge_info ei;
        vector<string> splitted;
        while (iss_l){
            string word;
            iss_l >> word;
            if (word.length() == 0) continue;
            splitted.push_back(word);
        } 
        if (splitted.size() != 3) continue;
        if (splitted[0].length() == 0) continue;
        ei.node1 = stoi(splitted[0]);
        if (splitted[1].length() == 0) continue;
        ei.node2 = stoi(splitted[1]);
        if (splitted[2].length() == 0) continue;
        ei.dist = stof(splitted[2]);
        ansab.edges.push_back(ei);
    }
    return ansab;
}

struct query2c {
    char map_id;
    int src_vtx;
    int des_vtx;
    int file_size;
    float prop_spd;
    int trans_spd;
    vector<struct edge_info> edges;
};

char * q2c2string(struct query2c q2c, char *p, char *ansab) {
    int len;
    *p++ = q2c.map_id;
    *p++ = '\n';
    len = sprintf(p, "%d", q2c.src_vtx);
    p += len;
    *p++ = '\n';
    len = sprintf(p, "%d", q2c.des_vtx);
    p += len;
    *p++ = '\n';
    len = sprintf(p, "%d", q2c.file_size);
    p += len;
    *p++ = '\n';
    while (*ansab != '\0') {
        *p++ = *ansab++;
    }
    *p = '\0';
    return p;
}

struct answer_C {
    float min_len;
    float Tt;
    float Tp;
    float delay;
    //struct node *path;
    vector<int> path;
};

struct answer_C string2ansc (string s) {
    struct answer_C ansc;
    istringstream iss(s);
    string line;
    getline(iss, line); 
    ansc.min_len = stof(line);
    getline(iss, line); 
    ansc.Tt = stof(line);
    getline(iss, line);
    ansc.Tp = stof(line);
    getline(iss, line);
    ansc.delay = stof(line);

    getline(iss, line);
    istringstream iss_l(line);
    while (iss_l) {
        string word;
        iss_l >> word;
        if (word.length() == 0) continue;
        //cout << word << endl;
        ansc.path.push_back(stoi(word));
    }
    
    return ansc;
}

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0); 
    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr); 
}


int main(void) {

    // initialize TCP(with client) socket
    int sockfd, new_fd, numbytes; // listen on sock_fd, new connection on new_fd
    //char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p; 
    struct sockaddr_storage their_addr; // connector's address information 
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    int rv;
    char s[INET6_ADDRSTRLEN];
    struct query q;
    struct answer ans;
    struct answer_AB ansab;
    //struct answer_B ansb;
    struct query2c q2c;
    struct answer_C ansc;
    char buffer_a [MAX_LEN_IN_CHAR];
    char buffer_b [MAX_LEN_IN_CHAR];
    char buffer_c [MAX_LEN_IN_CHAR];
    char msg_c [MAX_LEN_IN_CHAR];
    char msg_a [MAX_LEN_IN_CHAR];
    char * real_ans;

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    // hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(IP, SELF_TCP_PORT, &hints, &servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1); 
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue; 
        }

        break; 
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) { 
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1); 
    }

    // initialize UDP(with backend servers) socket 
    int udp_sockfd;
    //char buf[MAXDATASIZE];
    struct addrinfo udp_hints, *udp_servinfo, *udp_p; 
    struct sockaddr_storage udp_their_addr;
    socklen_t udp_addr_len;
    int udp_rv;
    char udp_s[INET6_ADDRSTRLEN];

    memset(&udp_hints, 0, sizeof udp_hints); 
    udp_hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    udp_hints.ai_socktype = SOCK_DGRAM; 
    // hints.ai_flags = AI_PASSIVE; // use my IP

    if ((udp_rv = getaddrinfo(IP, SELF_UDP_PORT, &udp_hints, &udp_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(udp_rv));
    }

    // loop through all the results and bind to the first we can
    for(udp_p = udp_servinfo; udp_p != NULL; udp_p = udp_p->ai_next) {
        if ((udp_sockfd = socket(udp_p->ai_family, udp_p->ai_socktype, udp_p->ai_protocol)) == -1) { 
            perror("listener: socket"); 
            continue;
        }

        if (bind(udp_sockfd, udp_p->ai_addr, udp_p->ai_addrlen) == -1) { 
            close(udp_sockfd);
            perror("listener: bind");
            continue; 
        }

        break; 
    }

    if (udp_p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n"); 
        return 2;
    }

    freeaddrinfo(udp_servinfo);

    printf("The AWS is up and running\n");


    // get address of Server A (reference from Beej's guide to socket programming)
    struct addrinfo a_hints, *a_servinfo, *a_p;
    int a_status;

    memset(&a_hints, 0, sizeof a_hints);
    a_hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version 
    a_hints.ai_socktype = SOCK_DGRAM;
    if ((a_status = getaddrinfo(IP, A_PORT, &a_hints, &a_servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(a_status)); 
        return 2;
    }

    a_p = a_servinfo;
    while (a_p == NULL) a_p = a_p->ai_next;
    if (a_p == NULL) {
        printf("Error with Server A's address\n");
        return 2;
    }

    // get address of Server B
    struct addrinfo b_hints, *b_servinfo, *b_p;
    int b_status;

    memset(&b_hints, 0, sizeof b_hints);
    b_hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version 
    b_hints.ai_socktype = SOCK_DGRAM;
    if ((b_status = getaddrinfo(IP, B_PORT, &b_hints, &b_servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(b_status)); 
        return 2;
    }

    b_p = b_servinfo;
    while (b_p == NULL) b_p = b_p->ai_next;
    if (b_p == NULL) {
        printf("Error with Server B's address\n");
        return 2;
    }

    // get address of Server C
    struct addrinfo c_hints, *c_servinfo, *c_p;
    int c_status;

    memset(&c_hints, 0, sizeof c_hints);
    c_hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version 
    c_hints.ai_socktype = SOCK_DGRAM;
    if ((c_status = getaddrinfo(IP, C_PORT, &c_hints, &c_servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(c_status)); 
        return 2;
    }

    c_p = c_servinfo;
    while (c_p == NULL) c_p = c_p->ai_next;
    if (c_p == NULL) {
        printf("Error with Server C's address\n");
        return 2;
    }


    // Keep listening
    while (1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) { 
            perror("accept");
            continue;
        }

        if (!fork()) { // this is the child process 
            close(sockfd); // child doesn't need the listener

            // receive query from client 
            if ((numbytes = recv(new_fd, &q, MAXDATASIZE-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            
            printf("The AWS has received map ID %c, start vertex %d, "
            "destination vertex %d and file size %d from the client "
            "using TCP over port %s\n", q.map_id, q.src_vtx, q.des_vtx, q.file_size, SELF_TCP_PORT);

            // send query to server A
            if ((numbytes = sendto(udp_sockfd, &q.map_id, sizeof(q.map_id), 
                    0, a_p->ai_addr, a_p->ai_addrlen)) == -1) {
                perror("send");
                exit(1);
            };
            printf("The AWS has sent map ID to server A using UDP over port %s\n", SELF_UDP_PORT);

            // receive answer of server A
            if ((numbytes = recvfrom(udp_sockfd, &buffer_a, MAXDATASIZE-1, 
                    0, (struct sockaddr *)&udp_their_addr, &udp_addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            };

            if (strcmp(buffer_a, "NO")) {
                printf("The AWS has received map information from server A\n");
                real_ans = buffer_a;
            } else {
                // If map not found in server A, send query to server B
                if ((numbytes = sendto(udp_sockfd, &q.map_id, sizeof(q.map_id), 
                        0, b_p->ai_addr, b_p->ai_addrlen)) == -1) {
                    perror("send");
                    exit(1);
                };
                printf("The AWS has sent map ID to server B using UDP over port %s\n", SELF_UDP_PORT);

                // receive answer of server B
                if ((numbytes = recvfrom(udp_sockfd, &buffer_b, MAXDATASIZE-1, 
                        0, (struct sockaddr *)&udp_their_addr, &udp_addr_len)) == -1) {
                    perror("recvfrom");
                    exit(1);
                };

                if (strcmp(buffer_b, "NO")) {
                    printf("The AWS has received map information from server B\n");
                    real_ans = buffer_b;
                }
            }


            // No map id found

            if (!strcmp(buffer_a, "NO") && !strcmp(buffer_b, "NO")) {
                // ans.map_not_found = true;
                char * p = msg_a;
                *p++ = 'N';
                *p++ = 'M';
                *p = '\0';
                if ((numbytes = send(new_fd, &msg_a, sizeof(msg_a), 0)) == -1) {
                    perror("send");
                    exit(1);
                };
                close(new_fd);
                exit(0);
                continue; // keep listening
            }


            // Check nodes in graph
            string s_real_ans(real_ans);
            ansab = string2ansab(s_real_ans);
            bool src_found = false;
            bool des_found = false;
            
            for (int i = 0; i < ansab.edges.size(); i++) {
                if (ansab.edges[i].node1 == q.src_vtx || ansab.edges[i].node2 == q.src_vtx)
                    src_found = true;
                if (ansab.edges[i].node1 == q.des_vtx || ansab.edges[i].node2 == q.des_vtx)
                    des_found = true;
                if (src_found && des_found){
                    printf("The source and destination vertex are in the graph\n\n");
                    break;
                }
            }

            if (!src_found) {
                printf("Source vertex not found in the graph, sending error to client "
                "using TCP over port %s\n", SELF_TCP_PORT);
                // ans.missing_vtx = q.src_vtx;
                // char * p = msg_a;
            }

            if (!des_found) {
                printf("Destination vertex not found in the graph, sending error to client "
                "using TCP over port %s\n", SELF_TCP_PORT);
                // ans.missing_vtx = q.des_vtx;
            }

            if (!src_found || !des_found) {
                // ans.vtx_not_found = true;
                char * p = msg_a;
                *p++ = 'N';
                *p++ = 'V';
                if (!src_found) {
                    *p++ = 'S';
                }
                else if (!des_found) {
                    *p++ = 'D';
                }
                *p = '\0';
                if ((numbytes = send(new_fd, &msg_a, sizeof(msg_a), 0)) == -1) {
                    perror("send");
                    exit(1);
                };
                printf("\n");
                close(new_fd);
                exit(0);
                continue; // keep listening
            }

            // send query to C

            q2c.src_vtx = q.src_vtx;
            q2c.des_vtx = q.des_vtx;
            q2c.map_id = q.map_id;
            q2c.file_size = q.file_size;
            q2c.prop_spd = ansab.prop_spd;
            q2c.trans_spd = ansab.trans_spd;
            q2c.edges = ansab.edges;

            char * p_msg_c  = msg_c;
            p_msg_c = q2c2string(q2c, p_msg_c, real_ans);
            if ((numbytes = sendto(udp_sockfd, &msg_c, sizeof(msg_c), 
                    0, c_p->ai_addr, c_p->ai_addrlen)) == -1) {
                perror("send");
                exit(1);
            };
            printf("The AWS has sent map, source ID, destination ID, propagation speed and "
            "transmission speed to server C using UDP over port %s\n", SELF_UDP_PORT);

            // receive answer from C

            if ((numbytes = recvfrom(udp_sockfd, &buffer_c, MAXDATASIZE-1, 
                    0, (struct sockaddr *)&udp_their_addr, &udp_addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            };

            string s_buffer_c(buffer_c);
            ansc = string2ansc(s_buffer_c);

            printf("The AWS has received results from server C:\n"
            "Shortest path: ");
            //"<src> -- <hop1> -- <hop2> ... -- <dest>\n"
            
            for (int i = 0; i < ansc.path.size(); i++) {
                if (i == ansc.path.size() - 1) {
                    cout << to_string(ansc.path[i]) << endl;
                } else {
                    cout << to_string(ansc.path[i]) << " -- ";
                }
            }
            printf("Shortest distance: %.2f km\n"
            "Transmission delay: %.2f s\n"
            "Propagation delay: %.2f s\n\n", ansc.min_len, ansc.Tt, ansc.Tp);


            // send answer back to client

            // ans.map_not_found = false;
            // ans.vtx_not_found = false;
            // ans.Tp = ansc.Tp;
            // ans.Tt = ansc.Tt;
            // ans.min_len = ansc.min_len;
            // ans.delay = ansc.delay;
            // ans.path = ansc.path;

            if ((numbytes = send(new_fd, &buffer_c, sizeof(buffer_c), 0)) == -1) {
                perror("send");
                exit(1);
            };
            printf("The AWS has sent calculated results to client using TCP over port %s\n\n", SELF_TCP_PORT);

            close(new_fd);
            exit(0);
        }

        close(new_fd); // parent doesn't need this
    }

    return 0;
}