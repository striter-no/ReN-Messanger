all:
	g++ --std=c++23 -O3 -o client ./source/client.cpp -I src/include -lgmpxx -lgmp
	g++ --std=c++23 -O3 -o server ./source/server.cpp -I src/include -lgmpxx -lgmp

msg:
	g++ --std=c++23 -O3 -o server ./source/msg_server.cpp -I src/include -lgmpxx -lgmp
	g++ --std=c++23 -O3 -o client ./source/msg_client.cpp -I src/include -lgmpxx -lgmp

st:
	g++ --std=c++23 -O3 -o server ./source/st_server.cpp -I src/include -lgmpxx -lgmp
	g++ --std=c++23 -O3 -o client ./source/st_client.cpp -I src/include -lgmpxx -lgmp

server:
	g++ --std=c++23 -O3 -o server ./source/server.cpp -I src/include -lgmpxx -lgmp

client:
	g++ --std=c++23 -O3 -o client ./source/client.cpp -I src/include -lgmpxx -lgmp

term:
	g++ --std=c++23 -O3 -o term_test ./source/term_test.cpp -I src/include -lgmpxx -lgmp

test:
	g++ --std=c++23 -O3 -o test ./source/test.cpp -I src/include -lgmpxx -lgmp