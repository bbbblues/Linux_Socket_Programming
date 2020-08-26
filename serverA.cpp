
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
#include <fstream>
#include <map> 
#include <vector> 
#include <sstream>
#include <iostream>

#define AWS_UDP_PORT "33125" 
#define A_PORT "30125"
#define IP "127.0.0.1" 

#define MAXDATASIZE 100 // max number of bytes we can get at once
#define MAX_LEN_IN_CHAR 700 // 693 actually

using namespace std;

struct answer_AB {
    // bool map_not_found_A;
    // bool map_not_found_B;
    int map_not_found_A;
    int map_not_found_B;
    float prop_spd;
    int trans_spd;
    //struct graph_info *edges;
    vector<struct edge_info> edges;
};

struct edge_info {
    int node1;
    int node2;
    float dist;
    //struct graph_info *next;
};

struct map_info {
    float prop_spd;
    int trans_spd;
    //struct graph_info *edges;
    vector<struct edge_info> edges;
};

char * ans2string(struct answer_AB ansab, char *p) {
    int len;
    // sprintf(p++, "%d", ansab.map_not_found_A);
    // *p++ = '\n';
    // sprintf(p++, "%d", ansab.map_not_found_B);
    // *p++ = '\n';
    len = sprintf(p, "%.2f", ansab.prop_spd);
    p += len;
    *p++ = '\n';
    len = sprintf(p, "%d", ansab.trans_spd);
    p += len;
    *p++ = '\n';

    for (int i = 0; i < ansab.edges.size(); i++) {
        struct edge_info ei = ansab.edges[i];
        len = sprintf(p, "%d", ei.node1);
        p += len;
        *p++ = ' ';
        len = sprintf(p, "%d", ei.node2);
        p += len;
        *p++ = ' ';
        len = sprintf(p, "%.2f", ei.dist);
        p += len;
        *p++ = '\n';
    }
    *p = '\0';
    return p;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr); 
}

map<char, struct map_info> load_map(string map_src) {
    ifstream fin;
    fin.open(map_src);
    string line;
    map<char, struct map_info> all_maps;

    char cur_id = '\0';
    struct map_info cur_map;
    vector<struct edge_info> cur_edges;
    while (fin) { 
        getline(fin, line); 
        if (isalpha(line[0])) {
            if (cur_id != '\0') {
                cur_map.edges = cur_edges;
                all_maps.insert(pair<char, struct map_info>(cur_id, cur_map));
                cur_edges.clear();
            }
            cur_id = line[0];
            getline(fin, line);
            cur_map.prop_spd = stof(line);
            getline(fin, line);
            cur_map.trans_spd = stoi(line); 
        } else {
            struct edge_info e;
            vector<string> splitted;
            istringstream iss(line);
            do {
                string s;
                iss >> s;
                splitted.push_back(s);
            } while (iss);
            e.node1 = stoi(splitted[0]);
            e.node2 = stoi(splitted[1]);
            e.dist = stof(splitted[2]);
            cur_edges.push_back(e);
        }
    }
    fin.close();
      
    return all_maps;
}

int main(int argc, char *argv[]) {

    map<char, struct map_info> all_maps = load_map("map1.txt");

    // initialize UDP socket(reference from Beej's guide)

    int sockfd, numbytes;
    // char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage udp_their_addr;
    socklen_t udp_addr_len; 
    int rv;
    char s[INET6_ADDRSTRLEN];
    char query_map_id;
    struct answer_AB ansab;
    char msg[MAX_LEN_IN_CHAR];

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(IP, A_PORT, &hints, &servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd);
            perror("listener: bind");
            continue; 
        }

        break; 
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    printf("The Server A is up and running using UDP on port %s\n\n", A_PORT);

    // get address of AWS (reference from Beej's guide to socket programming)

    struct addrinfo aws_hints, *aws_servinfo, *aws_p;
    int aws_status;

    memset(&aws_hints, 0, sizeof aws_hints);
    aws_hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version 
    aws_hints.ai_socktype = SOCK_DGRAM;
    if ((aws_status = getaddrinfo(IP, AWS_UDP_PORT, &aws_hints, &aws_servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(aws_status)); 
        return 2;
    }

    aws_p = aws_servinfo;
    while (aws_p == NULL) aws_p = aws_p->ai_next;
    if (aws_p == NULL) {
        printf("Error with Server A's address\n");
        return 2;
    }

    // infinite loop to receive queries
    while (1) {
        
        // receive message from AWS
        if ((numbytes = recvfrom(sockfd, &query_map_id, MAXDATASIZE-1, 
            0, (struct sockaddr *)&udp_their_addr, &udp_addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
        };
        printf("The Server A has received input for finding graph of %c\n", query_map_id);

        // check graph id
        auto itr = all_maps.find(query_map_id);
        if (itr == all_maps.end()) {
            printf("The Server A does not have the required graph id %c.\n", query_map_id);
            //ansab.map_not_found_A = 1;
            char * p = msg;
            *p++ = 'N';
            *p++ = 'O';
            *p = '\0';
            printf("The Server A has sent \"Graph not Found\" to AWS.\n\n");
        } else {
            // ansab.map_not_found_A = 0;
            // ansab.map_not_found_B = 1;
            struct map_info found_map = itr->second;
            ansab.prop_spd = found_map.prop_spd;
            ansab.trans_spd = found_map.trans_spd;
            ansab.edges = found_map.edges;
            char * p = msg;
            p = ans2string(ansab, p);
            printf("The Server A has sent Graph to AWS.\n\n");
        }

        if ((numbytes = sendto(sockfd, &msg, sizeof(msg), 
                    0, aws_p->ai_addr, aws_p->ai_addrlen)) == -1) {
            perror("send");
            exit(1);
        };

    }
    
    close(sockfd);

    return 0;
}