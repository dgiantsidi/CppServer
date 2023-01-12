rm -f main
protoc -I=. --cpp_out=. server_msg.proto
g++ -Wall -fsanitize=address server_msg.pb.cc main.cpp -o main -lprotobuf -fsanitize=address
#g++ -Wall server_msg.pb.cc main.cpp -o main -lprotobuf
