#include <iostream>
#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/client_context.h>

#include "dpf_pir.grpc.pb.h"
#include "dpf.h"
#include "hashdatastore.h"
#include <immintrin.h>
#include <boost/program_options.hpp>
#include <future> // muti-thread

namespace po = boost::program_options;
using namespace std;
using dpfpir::Answer;
using dpfpir::DPFPIRInterface;
using dpfpir::FuncKey;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;

class DpfPirClient
{
public:
    string client_id;
    size_t num;

private:
    std::unique_ptr<DPFPIRInterface::Stub> stub_;
    string serverAddr;

public:
    explicit DpfPirClient(std::shared_ptr<Channel> channel, string &client_id, string serverAddr, size_t num) : stub_(DPFPIRInterface::NewStub(channel)), client_id(client_id), serverAddr(serverAddr), num(num){};

    std::vector<hashdatastore::hash_type, AlignmentAllocator<hashdatastore::hash_type, sizeof(hashdatastore::hash_type)>> DpfPir(std::vector<uint8_t> funckey)
    {
        std::vector<hashdatastore::hash_type, AlignmentAllocator<hashdatastore::hash_type, sizeof(hashdatastore::hash_type)>> result;

        FuncKey request;
        Answer reply;
        ClientContext context;
        context.AddMetadata("client_id", this->client_id);

        /* set funckey */
        string key_str(funckey.begin(), funckey.end());
        request.set_funckey(key_str);

        Status status = stub_->DpfPir(&context, request, &reply);
        // // pad
        // string answer = reply.answer();
        // size_t originalSize = answer.size();
        // size_t newSize = (originalSize + 31) / 32 * 32;
        // answer.append(newSize - originalSize, ' ');

        if (status.ok())
        {

            std::cout << "[" << this->client_id << "][" << this->serverAddr << "] "
                      << "2.Receive PIR result." << std::endl;
            for (size_t i = 0; i < this->num; i++)
            {
                result.push_back(stringToM256i(reply.answer().substr(i * 32, 32)));
            }
        }
        else
        {
            std::cout << "RPC failed" << std::endl;
            std::cout << status.error_code() << ": " << status.error_message()
                      << std::endl;
            for (size_t i = 0; i < this->num; i++)
            {
                result.push_back(__m256i());
            }
        }
        return result;
    }

private:
    // Convert string to __m256i
    __m256i stringToM256i(const std::string &str)
    {
        __m256i result;
        alignas(32) uint8_t buffer[32];

        // Ensure the string is large enough to fill the buffer
        if (str.size() >= sizeof(buffer))
        {
            // Copy characters from the string to the buffer
            for (size_t i = 0; i < sizeof(buffer); ++i)
            {
                buffer[i] = static_cast<uint8_t>(str[i]);
            }

            // Load the buffer into the __m256i variable
            result = _mm256_load_si256((__m256i *)buffer);
        }
        else
        {
            // Handle the case where the string is too short
            std::cerr << "Error: String is too short to convert to __m256i." << std::endl;
            // You might want to handle this differently based on your requirements
        }

        return result;
    }
};

class Utils
{
public:
    Utils() = delete;
    static size_t string2uint64(string query_keyword)
    {
        size_t result = 0;
        for (size_t i = 0; i < 8 && i < query_keyword.size(); i++)
        {
            result |= static_cast<size_t>(query_keyword[i]) << (8 * i);
        }
        return result;
    }
    static string m256i2string(hashdatastore::hash_type value)
    {
        std::string result;
        size_t re0 = _mm256_extract_epi64(value, 3);
        size_t re1 = _mm256_extract_epi64(value, 2);
        size_t re2 = _mm256_extract_epi64(value, 1);
        size_t re3 = _mm256_extract_epi64(value, 0);
        for (size_t j = 0; j < 8; j++)
        {
            unsigned char byte = (re0 >> (j * 8)) & 0xFF;
            result += static_cast<char>(byte);
        }
        for (size_t j = 0; j < 8; j++)
        {
            unsigned char byte = (re1 >> (j * 8)) & 0xFF;
            result += static_cast<char>(byte);
        }
        for (size_t j = 0; j < 8; j++)
        {
            unsigned char byte = (re2 >> (j * 8)) & 0xFF;
            result += static_cast<char>(byte);
        }
        for (size_t j = 0; j < 8; j++)
        {
            unsigned char byte = (re3 >> (j * 8)) & 0xFF;
            result += static_cast<char>(byte);
        }
        return result;
    }
};

int main(int argc, char *argv[])
{
    /* 219.245.186.51 */
    string Addr = "localhost";
    string serverAddr0 = Addr + ":50053";
    string serverAddr1 = Addr + ":50054";

#pragma region args
    /* args */
    string client_id;
    string query_keyword;
    try
    {
        // def options
        po::options_description desc("Allowed options");
        desc.add_options()("help,h", "Produce help message")("id", po::value<std::string>()->required(), "client id (string)")("q", po::value<string>()->required(), "query_keyword (string)");

        // parse params
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        // result
        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return 0;
        }

        if (vm.count("id"))
        {
            client_id = vm["id"].as<std::string>();
            // std::cout << "Input file: " << vm["input-file"].as<std::string>() << std::endl;
        }
        if (vm.count("q"))
        {
            query_keyword = vm["q"].as<string>();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#pragma endregion args

    /* RPC */
    size_t num = 3;
    DpfPirClient rpc_client0(grpc::CreateChannel(serverAddr0, grpc::InsecureChannelCredentials()), client_id, serverAddr0, num);
    DpfPirClient rpc_client1(grpc::CreateChannel(serverAddr1, grpc::InsecureChannelCredentials()), client_id, serverAddr1, num);

    /* GenFuncKeys */
    size_t logN = 48; // 48 bit hash for one million entries
    // size_t query_index = Utils::string2uint64(query_keyword);
    uint64_t HASH_MASK = 0xFFFFFFFFFFFF;
    std::hash<std::string> hashFunction;
    size_t query_index = hashFunction(query_keyword) & HASH_MASK; // 48 bits
    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> keys = DPF::Gen(query_index, logN);
    std::cout << "[" << client_id << "] "
              << "1.GenFuncKeys." << std::endl;

    /* PIR */
    std::vector<hashdatastore::hash_type, AlignmentAllocator<hashdatastore::hash_type, sizeof(hashdatastore::hash_type)>> answer0 = rpc_client0.DpfPir(keys.first);
    std::vector<hashdatastore::hash_type, AlignmentAllocator<hashdatastore::hash_type, sizeof(hashdatastore::hash_type)>> answer1 = rpc_client1.DpfPir(keys.second);
    // // Async call to rpc_client0.DpfPir(keys.first)
    // std::future<hashdatastore::hash_type> future_answer0 =
    //     std::async(std::launch::async, [&rpc_client0, &keys]()-> hashdatastore::hash_type
    //                { return rpc_client0.DpfPir(keys.first); });

    // // Async call to rpc_client1.DpfPir(keys.second)
    // std::future<hashdatastore::hash_type> future_answer1 =
    //     std::async(std::launch::async, [&rpc_client1, &keys]()-> hashdatastore::hash_type
    //                  { return rpc_client1.DpfPir(keys.second); });

    // // Wait for both futures to be ready and get the results
    // hashdatastore::hash_type answer0 = future_answer0.get();
    // hashdatastore::hash_type answer1 = future_answer1.get();

    /* Answer reconstructed */
    std::cout << "[" << client_id << "] "
              << "3.Answer reconstructed: " << std::endl;
    std::vector<hashdatastore::hash_type, AlignmentAllocator<hashdatastore::hash_type, sizeof(hashdatastore::hash_type)>> answer;
    for (size_t i = 0; i < num; i++)
    {
        answer.push_back(_mm256_xor_si256(answer0[i], answer1[i]));
    }
    std::string answer_str;
    for (size_t i = 0; i < num; i++)
    {
        answer_str += Utils::m256i2string(answer[i]);
    }
    std::cout << "\tanswer:" << answer_str << std::endl;

    return 0;
}