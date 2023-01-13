#pragma once
#include "messages/server_msg.pb.h"
#include <cstring>
#include <memory>
#include <tuple>

int seq = 0;

struct Msg {
	int sequencer = 0;
	static constexpr int kKey_sz = 8;
	static constexpr int kBatchSize = 1;
	static constexpr int kMsgSize = 8;

	inline tutorial::ServerReqMsg_KV* construct_KVPair(google::protobuf::Arena& arena, size_t msg_size) {
		auto kv =  google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_KV>(&arena);

		kv->set_key_sz(kKey_sz);
		kv->set_value_sz(msg_size);

		//FIXME: @dimitra allocate key/value space here appropriately
		kv->set_key("dimitra");
		kv->set_value("dimitra giantsidi");
		return kv;
	}

	inline tutorial::ServerReqMsg::Req* create_batchedRequest(google::protobuf::Arena& arena, size_t batch_size, std::vector<tutorial::ServerReqMsg_KV*> pairs) {
		//TODO
		return nullptr;
	}

	inline tutorial::ServerReqMsg_Msg* create_batchedRequest(google::protobuf::Arena& arena, size_t batch_size, tutorial::ServerReqMsg_KV* kv) {
		auto req = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Req>(&arena);
		req->set_allocated_kv_pair(kv);
		req->set_rtype(tutorial::ServerReqMsg_ReqType_PUT);

		::tutorial::ServerReqMsg_Msg* msg = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Msg>(&arena);
		msg->set_seq(sequencer);

		sequencer++;
		for (size_t i = 0ULL; i < batch_size; i++) {
			auto ptr = msg->add_rtype();
			*ptr = *req;
		}
		return msg;
	}
	
	inline void serialize_MsgToBeSend(google::protobuf::Arena& arena, tutorial::ServerReqMsg_Msg* batched_req, std::string& output) {

		::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);
		tb->set_payload_size(batched_req->ByteSizeLong());
		tb->set_allocated_payload(batched_req);
		tb->SerializeToString(&output);
	}

	std::tuple<const char*, size_t> GetMsg(size_t msg_size, size_t batch_size, std::string& output) {
		google::protobuf::Arena arena;

		auto kv_pair = construct_KVPair(arena, msg_size);
#if PRINT
		std::cout << kv_pair->DebugString() << "\n";
#endif 
		auto batched_req = create_batchedRequest(arena, batch_size, kv_pair);

		serialize_MsgToBeSend(arena, batched_req, output);

		return {output.data(), output.size()};

	}
};
