
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
#include <vector>
#include <sstream>
#include <string>
#include <iostream>

#define PORT "34125" // the port client will be connecting to
#define IP "127.0.0.1"

#define MAXDATASIZE 100 // max number of bytes we can get at once
#define MAX_LEN_IN_CHAR 700 // 693 actually

using namespace std;

struct query {
    char map_id;
    int src_vtx;
    int des_vtx;
    int file_size;
};

struct answer {
    float min_len;
    float Tt;
    float Tp;
    float delay;
    vector<int> path;
};

struct answer string2ans (string s) {
    struct answer ans;
    istringstream iss(s);
    string line;
    getline(iss, line); 
    ans.min_len = stof(line);
    getline(iss, line); 
    ans.Tt = stof(line);
    getline(iss, line);
    ans.Tp = stof(line);
    getline(iss, line);
    ans.delay = stof(line);

    getline(iss, line);
    istringstream iss_l(line);
    while (iss_l) {
        string word;
        iss_l >> word;
        if (word.length() == 0) break;
        ans.path.push_back(stoi(word));
    }
    
    return ans;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr); 
}

int main(int argc, char *argv[]) {
    int sockfd, numbytes, bytes_sent;
    // char buf[MAXDATASIZE];
    struct query q;
    struct answer ans;
    char buf[MAX_LEN_IN_CHAR];
    struct addrinfo hints, *servinfo, *p; 
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 5) {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(IP, PORT, &hints, &servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("client: connect");
            continue; 
        }

        break; 
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), 
            s, sizeof s); // convert the IP to a string:
    // printf("client: connecting to %s\n", s);
    printf("The client is up and running\n");

    struct query *qp = &q;
    qp->map_id = argv[1][0];
    qp->src_vtx = atoi(argv[2]);
    qp->des_vtx = atoi(argv[3]);
    qp->file_size = atoi(argv[4]);

    if ((bytes_sent = send(sockfd, qp, sizeof(*qp), 0)) == -1) {
        perror("send");
        exit(1);
    };
    printf("The client has sent query to AWS using TCP: start vertex %d; " 
    "destination vertex %d, map %c; file size %d\n", qp->src_vtx, qp->des_vtx, qp->map_id, qp->file_size);

    freeaddrinfo(servinfo); // all done with this structure

    if ((numbytes = recv(sockfd, &buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    } else {
        if (!strcmp(buf, "NM")) {
            printf("No map id %c found\n", q.map_id);
        } else if (!strcmp(buf, "NVS") || !strcmp(buf, "NVD")) {
            if (!strcmp(buf, "NVS")) {
                printf("No vertex id %d found\n", q.src_vtx);
            }
            if (!strcmp(buf, "NVD")) {
                printf("No vertex id %d found\n", q.des_vtx);
            }
        } else {
            string s_buf(buf);
            ans = string2ans(s_buf);
            printf("The client has received results from AWS:\n");
            printf("-----------------------------------------\n");
            printf("Source   Destination    Min Length    Tt      Tp      Delay   \n");
            printf("%d       ", q.src_vtx);
            printf("%d               ", q.des_vtx);
            printf("%.2f     ", ans.min_len);
            printf("%.2f    ", ans.Tt);
            printf("%.2f    ", ans.Tp);
            printf("%.2f     \n", ans.delay);
            printf("-----------------------------------------\n");
            printf("Shortest path: ");
            for (int i = 0; i < ans.path.size(); i++) {
                if (i == ans.path.size() - 1) {
                    printf("%d\n", ans.path[i]);
                } else {
                    printf("%d -- ", ans.path[i]);
                }
            }
        }
    }

    buf[numbytes] = '\0';
    close(sockfd);

    return 0;
}