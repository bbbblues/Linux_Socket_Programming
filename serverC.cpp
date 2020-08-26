
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
#include <set>
#include <queue> 

#define AWS_UDP_PORT "33125" 
#define C_PORT "32125"
#define IP "127.0.0.1" 

#define MAX_DISTANCE 450000
#define MAXDATASIZE 10000 // max number of bytes we can get at once
#define MAX_LEN_IN_CHAR 700 // 693 actually

using namespace std;

typedef pair<float, int> pp;

map<int, float> min_dis;

struct edge_info {
    int node1;
    int node2;
    float dist;
    //struct graph_info *next;
};

struct query2c {
    char map_id;
    int src_vtx;
    int des_vtx;
    int file_size;
    float prop_spd;
    int trans_spd;
    // struct graph_info *edges;
    vector<struct edge_info> edges;
};

struct query2c string2q2c (string s) {
    struct query2c q2c;
    istringstream iss(s);
    string line;
    getline(iss, line); 
    q2c.map_id = line[0];
    getline(iss, line); 
    q2c.src_vtx = stoi(line);
    getline(iss, line);
    q2c.des_vtx = stoi(line);
    getline(iss, line);
    q2c.file_size = stoi(line);
    getline(iss, line);
    q2c.prop_spd = stof(line);
    getline(iss, line);
    q2c.trans_spd = stoi(line);

    while (getline(iss, line)) {
        istringstream iss_l(line);
        struct edge_info ei;
        vector<string> splitted;
        while (iss_l) {
            string word;
            iss_l >> word;
            splitted.push_back(word);
        } 
        ei.node1 = stoi(splitted[0]);
        ei.node2 = stoi(splitted[1]);
        ei.dist = stof(splitted[2]);
        q2c.edges.push_back(ei);
    }
    
    return q2c;
}

struct map_info {
    float prop_spd;
    int trans_spd;
    //struct graph_info *edges;
    vector<struct edge_info> edges;
};

struct answer_C {
    float min_len;
    float Tt;
    float Tp;
    float delay;
    //struct node *path;
    vector<int> path;
};

char * ansc2string(struct answer_C ansc, char *p) {
    int len;

    len = sprintf(p, "%.2f", ansc.min_len);
    p += len;
    *p++ = '\n';

    len = sprintf(p, "%.2f", ansc.Tt);
    p += len;
    *p++ = '\n';

    len = sprintf(p, "%.2f", ansc.Tp);
    p += len;
    *p++ = '\n';

    len = sprintf(p, "%.2f", ansc.delay);
    p += len;
    *p++ = '\n';

    for (int i = 0; i < ansc.path.size(); i++) {
        int node = ansc.path[i];
        len = sprintf(p, "%d", node);
        p += len;
        *p++ = ' ';
    }
    p--;
    *p++ = '\n';
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

struct answer_C dijkstra(struct query2c q2c) {

    struct answer_C res;

    // build graph
    map< int, vector< pair <int, float> > > adj;
    for (int i = 0; i < q2c.edges.size(); i++) {
        int n1 = q2c.edges[i].node1;
        int n2 = q2c.edges[i].node2;
        float dis = q2c.edges[i].dist;
        auto itr = adj.find(n1);
        if (itr == adj.end()) {
            vector< pair<int, float> > list;
            list.push_back(pair<int, float>(n2, dis));
            adj.insert(pair<int, vector<pair<int, float> > >(n1, list));
        } else {
            vector< pair<int, float> > *v = &(itr->second);
            (*v).push_back(pair<int, float>(n2, dis));
        }

        itr = adj.find(n2);
        if (itr == adj.end()) {
            vector< pair<int, float> > list;
            list.push_back(pair<int, float>(n1, dis));
            adj.insert(pair<int, vector<pair<int, float> > >(n2, list));
        } else {
            vector<pair<int, float> > *v = &(itr->second);
            (*v).push_back(pair<int, float>(n1, dis));
        }
    }

    int src = q2c.src_vtx;
    int des = q2c.des_vtx;
    map<int, float> min_dis;
    vector<int> path;
    // class myComparator {
    //     public:
    //         int operator() (int n1, int n2) {
    //             return min_dis.find(n1)->second > min_dis.find(n1)->second;
    //         }
    // };
    priority_queue<pp, vector<pp>, greater<pp> > pq;
    //set<int> visited;
    map<int, int> father;

    for (auto const& itr: adj) {
        int node = itr.first;
        if (node == src) {
            min_dis.insert(pair<int, float>(node, 0));
            //pq.push(pp(0, node));
        } else {
            min_dis.insert(pair<int, float>(node, MAX_DISTANCE));
            //pq.push(pp(MAX_DISTANCE, node));
        }
    }

    pq.push(pp(0, src));

    while (pq.top().second != des) {
        int cur = pq.top().second;
        //visited.insert(cur);
        float curmin = min_dis.find(cur)->second;
        for (pair<int, float> neib: adj.find(cur)->second) {
            int n = neib.first;
            float curdis = neib.second;
            if (min_dis.find(n)->second > curmin + curdis) {
                min_dis.find(n)->second = curmin + curdis;
                pq.push(pp(min_dis[n], n));


                auto fitr = father.find(n);
                if (fitr == father.end()) {
                    father.insert(pair<int, int> (n, cur));
                } else {
                    fitr->second = cur;
                }
            }
        }
    
        pq.pop();
    }

    res.min_len = min_dis.find(des)->second;
    path.insert(path.begin(), des);
    int n = des;
    while (n != src) {
        n = father.find(n)->second;
        path.insert(path.begin(), n);
    }
    res.path = path;
    res.Tp = res.min_len / q2c.prop_spd;
    res.Tt = (float)q2c.file_size / (float)q2c.trans_spd;
    res.delay = res.Tp + res.Tt;
    return res;
}

int main(int argc, char *argv[]) {

    // initialize UDP socket(reference from Beej's guide)

    int sockfd, numbytes;
    // char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage udp_their_addr;
    socklen_t udp_addr_len; 
    int rv;
    char s[INET6_ADDRSTRLEN];
    char query_map_id;
    struct query2c q2c;
    struct answer_C ansc;
    char buffer[MAX_LEN_IN_CHAR];
    char ans2aws[MAX_LEN_IN_CHAR];

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(IP, C_PORT, &hints, &servinfo)) != 0) { 
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

    printf("The Server C is up and running using UDP on port %s\n\n", C_PORT);

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
        printf("Error with Server C's address\n");
        return 2;
    }


    // infinite loop to receive queries
    while (1) {

        // receive message from AWS

        if ((numbytes = recvfrom(sockfd, &buffer, MAXDATASIZE-1, 
                0, (struct sockaddr *)&udp_their_addr, &udp_addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        };

        string s_buffer(buffer);
        q2c = string2q2c(s_buffer);

        printf("The Server C has received data for calculation:\n"
        "* Propagation speed: %.2f km/s;\n"
        "* Transmission speed %d KB/s;\n"
        "* map ID: %c;\n"
        "* Source ID: %d    Destination ID: %d;\n\n", q2c.prop_spd, q2c.trans_spd, 
        q2c.map_id, q2c.src_vtx, q2c.des_vtx);

        // compute the result
        ansc = dijkstra(q2c);

        printf("The Server C has finished the calculation:\n"
            "Shortest path: ");
        // <src> -- <hop1> -- <hop2> ... -- <dest>
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

        // send back to AWS

        char *p = ans2aws;
        p = ansc2string(ansc, p);

        if ((numbytes = sendto(sockfd, &ans2aws, sizeof(ans2aws), 
                0, aws_p->ai_addr, aws_p->ai_addrlen)) == -1) {
            perror("send");
            exit(1);
        };
        printf("The Server C has finished sending the output to AWS\n\n");

    }

    close(sockfd);
    return 0;
}