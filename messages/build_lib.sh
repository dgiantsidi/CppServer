
rm -f msg_proto.a
protoc -I=. --cpp_out=. server_msg.proto
g++ -Wall -c server_msg.pb.cc -o main.o -lprotobuf
ar rcs msg_proto.a *.o
#g++ -Wall server_msg.pb.cc main.cpp -o main -lprotobuf
