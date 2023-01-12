
#include <iostream>
#include <cstring>
#include "server_msg.pb.h"
#include <google/protobuf/arena.h>


int main() {
	tutorial::ServerReqMsg::Req* test_req2;
	google::protobuf::Arena arena;
	{
		std::string output;
		tutorial::ServerReqMsg::Req* test_req;
		{
			tutorial::ServerReqMsg_KV* test_kv =  google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_KV>(&arena);
			test_kv->set_key_sz(sizeof("dimitra"));
			test_kv->set_value_sz(sizeof("dimitra giantsidi"));
			test_kv->set_key("dimitra");
			test_kv->set_value("dimitra giantsidi");
			test_req = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Req>(&arena);
			test_req->set_allocated_kv_pair(test_kv);
			test_req->set_rtype(tutorial::ServerReqMsg_ReqType_PUT);
			std::cout << "[Test #1]\n" << test_req->DebugString() << "\n";
			test_req->SerializeToString(&output);
		}

		{
			// desirialize the message
			// google::protobuf::Arena arena;
			test_req2 = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Req>(&arena);
			test_req2->ParseFromString(output);
			std::cout << "[Test #2]\n" << test_req2->DebugString() << "\n";
		}
	}

	// std::cout << "[Test #3]\n" << test_req2->DebugString() << "\n"; 
	{

		google::protobuf::Arena arena;
		::tutorial::ServerReqMsg_Msg* test_msg = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Msg>(&arena);
		test_msg->set_seq(1001);
#if 1
		for (size_t i = 0ULL; i < 10; i++) {
			// tutorial::ServerReqMsg::Req* test_req =  google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Req>(&arena);
			auto ptr = test_msg->add_rtype();
			*ptr = *test_req2;
		}

		std::cout << "[Test #3]\n" << test_msg->DebugString() << "\n";
#endif
		::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);
		tb->set_payload_size(test_msg->ByteSizeLong());
		tb->set_allocated_payload(test_msg);
		std::cout << "[Test #4]\n" << tb->DebugString() << "\n";

	}
	return 1;
}
