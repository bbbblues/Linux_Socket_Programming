
# Linux socket programming

- A distributed application to compute the shortest path with Dikjstra’s algorithm from client’s query 
- Implemented TCP sockets between client and main server, UDP sockets between main server and 3 back-end servers
- Accomplished data serializationand deserialization modules and computed different types of delays


## Code files:
1. client.cpp: Send queries to AWS server, receive messages from AWS server, using TCP.
2. aws.cpp: Receive queries from client, then send queries to server A and server B. If map ID exists, send query to server C, receive results from server C and send it back to client. Use TCP with client. Use UDP with back-end servers.
3. serverA.cpp: Read "map1.txt". Search the query map ID.
4. serverB.cpp: Read "map1.txt". Search the query map ID.
5. serverC.cpp: Compute shortest distance between source vertex and destination vertex, 	as well as delays.

## Format of messages exchanges:
- Strings are used as message exchanging format. 
- When sending, the messages are first stored into structs. Then the structs are serialized into C strings and ready to sent.
- When receiving, C string buffers are used to receive messages. Then the strings are deserialized into structs.

## Reused code:
Sample codes from "Beej's Guide to Network Programming".