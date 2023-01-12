//
// Created by Ivan Shynkarenka on 29.03.2018
//

#include "server/asio/service.h"
#include "server/asio/tcp_server.h"
#include "system/cpu.h"
#include "threads/thread.h"
#include "time/timestamp.h"
// #include "/home/dimitra/CppServer/messages/server_msg.pb.h"
#include "messages/server_msg.pb.h"

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

#include <OptionParser.h>

using namespace CppCommon;
using namespace CppServer::Asio;

class MulticastSession : public TCPSession
{
	public:
		using TCPSession::TCPSession;

		bool SendAsync(const void* buffer, size_t size) override
		{
			// Limit session send buffer to 1 megabyte
			const size_t limit = 1 * 1024 * 1024;
			size_t pending = bytes_pending();
			if ((pending + size) > limit)
				return false;
			if (size > (limit - pending))
				size = limit - pending;

			return TCPSession::SendAsync(buffer, size);
		}

	protected:
		void onError(int error, const std::string& category, const std::string& message) override
		{
			std::cout << "TCP session caught an error with code " << error << " and category '" << category << "': " << message << std::endl;
		}
};

class MulticastServer : public TCPServer
{
	public:
		using TCPServer::TCPServer;

	protected:
		std::shared_ptr<TCPSession> CreateSession(const std::shared_ptr<TCPServer>& server) override
		{
			return std::make_shared<MulticastSession>(server);
		}

	protected:
		void onError(int error, const std::string& category, const std::string& message) override
		{
			std::cout << "TCP server caught an error with code " << error << " and category '" << category << "': " << message << std::endl;
		}
};

int main(int argc, char** argv)
{
	auto parser = optparse::OptionParser().version("1.0.0.0");

	parser.add_option("-p", "--port").dest("port").action("store").type("int").set_default(1111).help("Server port. Default: %default");
	parser.add_option("-t", "--threads").dest("threads").action("store").type("int").set_default(CPU::PhysicalCores()).help("Count of working threads. Default: %default");
	parser.add_option("-m", "--messages").dest("messages").action("store").type("int").set_default(1000000).help("Rate of messages per second to send. Default: %default");
	parser.add_option("-s", "--size").dest("size").action("store").type("int").set_default(32).help("Single message size. Default: %default");

	optparse::Values options = parser.parse_args(argc, argv);

	// Print help
	if (options.get("help"))
	{
		parser.print_help();
		return 0;
	}

	// Server port
	int port = options.get("port");
	int threads = options.get("threads");
	int messages_rate = options.get("messages");
	int message_size = options.get("size");

	std::cout << "Server port: " << port << std::endl;
	std::cout << "Working threads: " << threads << std::endl;
	std::cout << "Messages rate: " << messages_rate << std::endl;
	std::cout << "Message size: " << message_size << std::endl;

	std::cout << std::endl;

	// Create a new Asio service
	auto service = std::make_shared<Service>(threads);

	// Start the Asio service
	std::cout << "Asio service starting...";
	service->Start();
	std::cout << "Done!" << std::endl;

	// Create a new multicast server
	auto server = std::make_shared<MulticastServer>(service, port);
	// server->SetupNoDelay(true);
	server->SetupReuseAddress(true);
	server->SetupReusePort(true);

	// Start the server
	std::cout << "Server starting...";
	server->Start();
	std::cout << "Done!" << std::endl;

	// Start the multicasting thread
	std::atomic<bool> multicasting(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	auto multicaster = std::thread([&server, &multicasting, messages_rate, message_size]()
			{
			// Prepare message to multicast
			std::vector<uint8_t> message_to_send(message_size);
			int seq = 0;

			// Multicasting loop
			while (multicasting)
			{
			auto start = UtcTimestamp();
			for (int i = 0; i < messages_rate; ++i) {
			//TODO: message serialization
			google::protobuf::Arena arena;
			std::string output;
			tutorial::ServerReqMsg::Req* test_req;

			tutorial::ServerReqMsg_KV* test_kv =  google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_KV>(&arena);

			test_kv->set_key_sz(sizeof("dimitra"));
			test_kv->set_value_sz(sizeof("dimitra giantsidi"));
			test_kv->set_key("dimitra");
			test_kv->set_value("dimitra giantsidi");
			test_req = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Req>(&arena);
			test_req->set_allocated_kv_pair(test_kv);
			test_req->set_rtype(tutorial::ServerReqMsg_ReqType_PUT);

			::tutorial::ServerReqMsg_Msg* test_msg = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Msg>(&arena);
			test_msg->set_seq(seq);

			seq++;
			for (size_t i = 0ULL; i < 1; i++) {
				// tutorial::ServerReqMsg::Req* test_req =  google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_Req>(&arena);
				auto ptr = test_msg->add_rtype();
				*ptr = *test_req;
			}

			::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);
			tb->set_payload_size(test_msg->ByteSizeLong());
			tb->set_allocated_payload(test_msg);
			tb->SerializeToString(&output);
			server->Multicast(output.data(), output.size());
		//	fmt::print("{} stream_size={}, payload_size={}, seq={}\n", __PRETTY_FUNCTION__, output.size(), tb->payload_size(), seq-1);
			}
			// server->Multicast(message_to_send.data(), message_to_send.size());
			auto end = UtcTimestamp();

			// Sleep for remaining time or yield
			auto milliseconds = (end - start).milliseconds();
			if (milliseconds < 1000)
				Thread::Sleep(1000 - milliseconds);
			else
				Thread::Yield();
			}
			});

	std::cout << "Press Enter to stop the server or '!' to restart the server..." << std::endl;

	// Perform text input
	std::string line;
	while (getline(std::cin, line))
	{
		if (line.empty())
			break;

		// Restart the server
		if (line == "!")
		{
			std::cout << "Server restarting...";
			server->Restart();
			std::cout << "Done!" << std::endl;
			continue;
		}
	}

	// Stop the multicasting thread
	multicasting = false;
	multicaster.join();

	// Stop the server
	std::cout << "Server stopping...";
	server->Stop();
	std::cout << "Done!" << std::endl;

	// Stop the Asio service
	std::cout << "Asio service stopping...";
	service->Stop();
	std::cout << "Done!" << std::endl;

	return 0;
}
