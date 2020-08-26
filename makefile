all: 
	g++ -std=c++11 serverA.cpp -o serverA
	g++ -std=c++11 serverB.cpp -o serverB
	g++ -std=c++11 serverC.cpp -o serverC
	g++ -std=c++11 aws.cpp -o aws
	g++ -std=c++11 client.cpp -o client

serverA:
	./serverA

serverB:
	./serverB

serverC:
	./serverC

aws:	
	./aws

client:
	./client


