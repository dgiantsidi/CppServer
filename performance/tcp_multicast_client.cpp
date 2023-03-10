//
// Created by Ivan Shynkarenka on 29.03.2018
//

#include "server/asio/service.h"
#include "server/asio/tcp_client.h"

#include "benchmark/reporter_console.h"
#include "system/cpu.h"
#include "threads/thread.h"
#include "time/timestamp.h"

#include <atomic>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include <OptionParser.h>
#include "messages/server_msg.pb.h"


using namespace CppCommon;
using namespace CppServer::Asio;

std::atomic<uint64_t> timestamp_start(Timestamp::nano());
std::atomic<uint64_t> timestamp_stop(Timestamp::nano());

std::atomic<uint64_t> total_errors(0);
std::atomic<uint64_t> total_bytes(0);
std::atomic<uint64_t> total_messages(0);

class MulticastClient : public TCPClient
{
	public:
		using TCPClient::TCPClient;

	protected:
		std::string output;
		std::string last_mem;
		std::string last_headers;
		bool hdr = false;
		int last_payload = -1;
		int prev_cnt = -1;


		void process_interrupted_msg(const char *& buffer, size_t& size) {
			if (hdr) {
				auto missing = sizeof(int) - last_headers.size();
				if (size > missing) {
					google::protobuf::Arena arena;

					last_headers.append(std::string(reinterpret_cast<const char*>(buffer), missing));
					std::string tmp(last_headers.data(), sizeof(int));

					::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);
					tb->ParsePartialFromString(tmp);
					size -= missing;

					fmt::print("[1] {}:{}, retrieving missing headers:\tpayload_size={}\tremaining_bytes={}\n", __PRETTY_FUNCTION__, std::hash<std::thread::id>()(std::this_thread::get_id()), tb->payload_size(), size);
					int payload_sz = tb->payload_size();

					if (payload_sz > size) {
						// TODO: need to keep memo
						fmt::print("HERE\n");
					}
					else {
						// consume this record, reset hdr and last_headers, and then procceed
						output = std::string(last_headers.data(), last_headers.size());
						output.append(std::string(reinterpret_cast<const char*>(buffer) + missing, payload_sz));
						::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);
						tb->ParsePartialFromString(output);
						const ::tutorial::ServerReqMsg_Msg& test_msg = tb->payload(); 
						fmt::print("[2] {}, consume the record:\toutput.size()=={}\tsequencer={}\tremaining_bytes={}\n", __PRETTY_FUNCTION__, output.size(), test_msg.seq(), size);
						if ((prev_cnt+1) != test_msg.seq()) {
							fmt::print("[10] {}:{}, sequencers not as expected (exp={}\treal={})...\n", __PRETTY_FUNCTION__,  std::hash<std::thread::id>()(std::this_thread::get_id()), (prev_cnt+1), test_msg.seq());
							std::cout << " " << tb->DebugString() << "\n";
							std::this_thread::sleep_for(std::chrono::milliseconds(5000));
						}
						prev_cnt += 1;
						size = size - payload_sz;
						hdr = false;
						last_headers.clear();
						buffer += missing + payload_sz;
					}
				}
				else {
					last_headers.append(std::string(reinterpret_cast<const char*>(buffer), size));
					size = 0;
					fmt::print("HERE2\n");
				}
				return;

			}

			if (last_payload > 0) {
				auto missing = last_payload - (last_mem.size() - sizeof(int)) ;
				if (size >= missing) {
					// consume last record
					last_mem.append(std::string(reinterpret_cast<const char*>(buffer), missing));
					buffer += missing;
					size -= missing;


					google::protobuf::Arena arena;
					::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);
					tb->ParseFromString(last_mem);
					const ::tutorial::ServerReqMsg_Msg& test_msg = tb->payload(); 
					if ((prev_cnt+1) != test_msg.seq()) {
						fmt::print("[3] {}:{}, sequencers not as expected (exp={}\treal={})...\n", __PRETTY_FUNCTION__,  std::hash<std::thread::id>()(std::this_thread::get_id()), (prev_cnt+1), test_msg.seq());
						std::cout << " " << tb->DebugString() << "\n";
						std::this_thread::sleep_for(std::chrono::milliseconds(5000));
					}
					// reset last_payload + last_mem
					// prev_cnt = test_msg.seq();
					prev_cnt += 1;
					last_payload = -1;
					last_mem.clear();
				}
				else {
					// update the last_mem
					last_mem.append(std::string(reinterpret_cast<const char*>(buffer), size));
					size = 0;
				}
			}

		}

		void onReceived(const void* _buffer, size_t size) override
		{	
			int processed_bytes = size;

			// remove const qualifier here (oops!)
			auto buffer = reinterpret_cast<const char*>(_buffer);

			// process missing data from previous send/recv
			process_interrupted_msg(buffer, size);

			// process messages and update buffers if message is not complete

			while (size != 0) {
				google::protobuf::Arena arena;
				int payload_sz = 0;
				if (size == 0) {
					fmt::print("{} ********size=0\n", __PRETTY_FUNCTION__);
					return ;
				}
				if (size < sizeof(int)) {
					// todo last memo
					hdr = true;
					last_headers = std::string(reinterpret_cast<const char*>(buffer), size);
					fmt::print("[4] {}:{} we did not retrieved headers (size={}) yet\n", __PRETTY_FUNCTION__, std::hash<std::thread::id>()(std::this_thread::get_id()), size);
	//				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					size = 0;
					break;
				}
				else {
					std::string tmp(reinterpret_cast<const char*>(buffer), sizeof(int));
					::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);
					tb->ParsePartialFromString(tmp);
#if PRINT_DEBUG
					fmt::print("[5] {}\tpayload_size={} total_size={}\n", __PRETTY_FUNCTION__, tb->payload_size(), size);
#endif
					payload_sz = tb->payload_size();
				}

				if (payload_sz > size - sizeof(int)) {
#if 0
					fmt::print("[6] {} did not received the entire message yet (payload={}, left_bytes={}, cur_seq={})\n", __PRETTY_FUNCTION__, payload_sz, (size-sizeof(int)), prev_cnt);
#endif
					// update last memo
					last_payload = payload_sz;
					last_mem = std::string(reinterpret_cast<const char*>(buffer), size);
	//				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					size = 0;
				}
				else {
					// we got all the message
					output = std::string(reinterpret_cast<const char*>(buffer), payload_sz);
					::tutorial::ServerReqMsg_ToBeSend* tb = google::protobuf::Arena::CreateMessage<tutorial::ServerReqMsg_ToBeSend>(&arena);

					if (tb->ParseFromString(output) == true) {
#if PRINT_DEBUG
						fmt::print("[7] {}, size={}, payload_sz={}\n", __func__, size, payload_sz);
						std::this_thread::sleep_for(std::chrono::milliseconds(5000));
#endif
					}
					const ::tutorial::ServerReqMsg_Msg& test_msg = tb->payload(); 
#if PRINT_DEBUG
					fmt::print("[8] {}:{}, output.size()=={} received_seq={} left_bytes={}\n", __PRETTY_FUNCTION__,  std::hash<std::thread::id>()(std::this_thread::get_id()), output.size(), test_msg.seq(), size);
					// fmt::print("{}\n", tb->DebugString());
#endif


					if ((prev_cnt+1) != test_msg.seq()) {
						fmt::print("{}:{}\t[Error] sequencers not as expected (exp={}\treceived={})...\n", __PRETTY_FUNCTION__, std::hash<std::thread::id>()(std::this_thread::get_id()), (prev_cnt+1), test_msg.seq());

						fmt::print("{}\n", tb->DebugString());
						std::this_thread::sleep_for(std::chrono::milliseconds(5000));
					}
					prev_cnt += 1;
					// prev_cnt = test_msg.seq();
					size = size - sizeof(int) - payload_sz;
				}

				if ((size) > 0) {
#if PRINT_DEBUG
					fmt::print("[9] {}, we received {} extra bytes\n", __PRETTY_FUNCTION__, (size));
#endif
					buffer += payload_sz + sizeof(int);
				}
			}


			total_bytes += processed_bytes;

		}

		void onError(int error, const std::string& category, const std::string& message) override
		{
			std::cout << "TCP client caught an error with code " << error << " and category '" << category << "': " << message << std::endl;
			++total_errors;
		}
};

int main(int argc, char** argv)
{
	auto parser = optparse::OptionParser().version("1.0.0.0");

	parser.add_option("-a", "--address").dest("address").set_default("127.0.0.1").help("Server address. Default: %default");
	parser.add_option("-p", "--port").dest("port").action("store").type("int").set_default(1111).help("Server port. Default: %default");
	parser.add_option("-t", "--threads").dest("threads").action("store").type("int").set_default(CPU::PhysicalCores()).help("Count of working threads. Default: %default");
	parser.add_option("-c", "--clients").dest("clients").action("store").type("int").set_default(100).help("Count of working clients. Default: %default");
	parser.add_option("-s", "--size").dest("size").action("store").type("int").set_default(32).help("Single message size. Default: %default");
	parser.add_option("-z", "--seconds").dest("seconds").action("store").type("int").set_default(10).help("Count of seconds to benchmarking. Default: %default");

	optparse::Values options = parser.parse_args(argc, argv);

	// Print help
	if (options.get("help"))
	{
		parser.print_help();
		return 0;
	}

	// Client parameters
	std::string address(options.get("address"));
	int port = options.get("port");
	int threads_count = options.get("threads");
	int clients_count = options.get("clients");
	int message_size = options.get("size");
	int seconds_count = options.get("seconds");

	std::cout << "Server address: " << address << std::endl;
	std::cout << "Server port: " << port << std::endl;
	std::cout << "Working threads: " << threads_count << std::endl;
	std::cout << "Working clients: " << clients_count << std::endl;
	std::cout << "Message size: " << message_size << std::endl;
	std::cout << "Seconds to benchmarking: " << seconds_count << std::endl;

	std::cout << std::endl;

	// Create a new Asio service
	auto service = std::make_shared<Service>(threads_count);

	// Start the Asio service
	std::cout << "Asio service starting...";
	service->Start();
	std::cout << "Done!" << std::endl;

	// Create multicast clients
	std::vector<std::shared_ptr<MulticastClient>> clients;
	for (int i = 0; i < clients_count; ++i)
	{
		auto client = std::make_shared<MulticastClient>(service, address, port);
		// client->SetupNoDelay(true);
		clients.emplace_back(client);
	}

	timestamp_start = Timestamp::nano();

	// Connect clients
	std::cout << "Clients connecting...";
	for (auto& client : clients)
		client->ConnectAsync();
	std::cout << "Done!" << std::endl;
	for (const auto& client : clients)
		while (!client->IsConnected())
			Thread::Yield();
	std::cout << "All clients connected!" << std::endl;

	// Wait for benchmarking
	fmt::print("\n{} Benhcmarking \n", __PRETTY_FUNCTION__);
	Thread::Sleep(seconds_count * 1000);
	std::cout << "Done!" << std::endl;

	// Disconnect clients
	std::cout << "Clients disconnecting...";
	for (auto& client : clients)
		client->DisconnectAsync();
	std::cout << "Done!" << std::endl;
	for (const auto& client : clients)
		while (client->IsConnected())
			Thread::Yield();
	std::cout << "All clients disconnected!" << std::endl;

	timestamp_stop = Timestamp::nano();

	// Stop the Asio service
	std::cout << "Asio service stopping...";
	service->Stop();
	std::cout << "Done!" << std::endl;

	std::cout << std::endl;

	std::cout << "Errors: " << total_errors << std::endl;

	std::cout << std::endl;

	total_messages = total_bytes / message_size;

	std::cout << "Total time: " << CppBenchmark::ReporterConsole::GenerateTimePeriod(timestamp_stop - timestamp_start) << std::endl;
	std::cout << "Total data: " << CppBenchmark::ReporterConsole::GenerateDataSize(total_bytes) << std::endl;
	std::cout << "Total messages: " << total_messages << std::endl;
	std::cout << "Data throughput: " << CppBenchmark::ReporterConsole::GenerateDataSize(total_bytes * 1000000000 / (timestamp_stop - timestamp_start)) << "/s" << std::endl;
	if (total_messages > 0)
	{
		std::cout << "Message latency: " << CppBenchmark::ReporterConsole::GenerateTimePeriod((timestamp_stop - timestamp_start) / total_messages) << std::endl;
		std::cout << "Message throughput: " << total_messages * 1000000000 / (timestamp_stop - timestamp_start) << " msg/s" << std::endl;
	}

	return 0;
}
