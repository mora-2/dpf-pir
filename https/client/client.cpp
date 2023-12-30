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
#include <cassert>
#include <thread>

namespace po = boost::program_options;
using namespace std;
using dpfpir::Answer;
using dpfpir::DPFPIRInterface;
using dpfpir::FuncKey;
using dpfpir::Info;
using dpfpir::Params;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;

class DpfPirClient
{
public:
    string client_id;
    size_t num_slice;
    size_t logN;

    std::unique_ptr<DPFPIRInterface::Stub> stub_;
    string serverAddr;

public:
    explicit DpfPirClient(std::shared_ptr<Channel> channel, string &client_id, string serverAddr) : stub_(DPFPIRInterface::NewStub(channel)), client_id(client_id), serverAddr(serverAddr){};

    void DpfParams()
    {
        Info request;
        Params reply;
        ClientContext context;
        context.AddMetadata("client_id", this->client_id);

        Status status = stub_->DpfParams(&context, request, &reply);
        if (status.ok())
        {
            this->logN = reply.logn();
            this->num_slice = reply.num_slice();
            return;
        }
        else
        {
            std::cout << "RPC failed" << std::endl;
            std::cout << status.error_code() << ": " << status.error_message()
                      << std::endl;
            return;
        }
    }

    static void checkParams(DpfPirClient &client0, DpfPirClient &client1)
    {
        assert(client0.logN == client1.logN);
        assert(client0.num_slice == client1.num_slice);
    }

    static std::pair<std::vector<uint8_t>, std::vector<uint8_t>> GenFuncKeys(string &query_keyword, size_t logN)
    {
        uint64_t HASH_MASK = (1ULL << logN) - 1;
        std::hash<std::string> hashFunction;
        size_t query_index = hashFunction(query_keyword) & HASH_MASK; // 48 bits
        std::pair<std::vector<uint8_t>, std::vector<uint8_t>> keys = DPF::Gen(query_index, logN);
        return keys;
    }

    std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> DpfPir(std::vector<uint8_t> &funckey)
    {
        std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> result;

        FuncKey request;
        Answer reply;
        ClientContext context;
        context.AddMetadata("client_id", this->client_id);

        /* set funckey */
        string key_str(funckey.begin(), funckey.end());
        request.set_funckey(key_str);

        Status status = stub_->DpfPir(&context, request, &reply);

        if (status.ok())
        {

            std::cout << "[" << this->client_id << "][" << this->serverAddr << "] "
                      << "3.Receive PIR result." << std::endl;
            for (size_t i = 0; i < this->num_slice; i++)
            {
                result.push_back(stringToM256i(reply.answer().substr(i * 32, 32)));
            }
        }
        else
        {
            std::cout << "RPC failed" << std::endl;
            std::cout << status.error_code() << ": " << status.error_message()
                      << std::endl;
            for (size_t i = 0; i < this->num_slice; i++)
            {
                result.push_back(__m256i());
            }
        }
        return result;
    }

    static string Reconstruction(std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> &answer0, std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> &answer1, size_t num_slice)
    {
        std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> answer;
        for (size_t i = 0; i < num_slice; i++)
        {
            answer.push_back(_mm256_xor_si256(answer0[i], answer1[i]));
        }
        std::string answer_str;
        for (size_t i = 0; i < num_slice; i++)
        {
            answer_str += DpfPirClient::m256i2string(answer[i]);
        }
        return answer_str;
    }

public: // for parallel
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

private:
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

void DpfPir_Parallel(DpfPirClient &rpc, std::vector<uint8_t> &funckey, std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> &ans);

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
    DpfPirClient rpc_client0(grpc::CreateChannel(serverAddr0, grpc::InsecureChannelCredentials()), client_id, serverAddr0);
    DpfPirClient rpc_client1(grpc::CreateChannel(serverAddr1, grpc::InsecureChannelCredentials()), client_id, serverAddr1);

    /* Params */
    rpc_client0.DpfParams();
    rpc_client1.DpfParams();
    DpfPirClient::checkParams(rpc_client0, rpc_client1); // check identity
    std::cout << "[" << client_id << "] 1.Params received." << std::endl;

    /* GenFuncKeys */
    std::pair<std::vector<uint8_t>, std::vector<uint8_t>> keys = DpfPirClient::GenFuncKeys(query_keyword, rpc_client0.logN);
    std::cout << "[" << client_id << "] 2.GenFuncKeys." << std::endl;

    /* PIR */
    std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> answer0, answer1;
    std::thread pir0(std::bind(DpfPir_Parallel, std::ref(rpc_client0), std::ref(keys.first), std::ref(answer0)));
    std::thread pir1(std::bind(DpfPir_Parallel, std::ref(rpc_client1), std::ref(keys.second), std::ref(answer1)));

    pir0.join(); // wait for finishing
    pir1.join();
    // std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator>
    //     answer0 = rpc_client0.DpfPir(keys.first);
    // std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> answer1 = rpc_client1.DpfPir(keys.second);

    /* Answer reconstructed */
    string answer_str = DpfPirClient::Reconstruction(answer0, answer1, rpc_client0.num_slice);
    std::cout << "[" << client_id << "] "
              << "4.Answer reconstructed: " << std::endl;
    std::cout << "\tanswer:" << answer_str << std::endl;

    return 0;
}

void DpfPir_Parallel(DpfPirClient &rpc, std::vector<uint8_t> &funckey, std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> &ans)
{
    FuncKey request;
    Answer reply;
    ClientContext context;
    context.AddMetadata("client_id", rpc.client_id);

    /* set funckey */
    string key_str(funckey.begin(), funckey.end());
    request.set_funckey(key_str);

    Status status = rpc.stub_->DpfPir(&context, request, &reply);

    if (status.ok())
    {

        std::cout << "[" << rpc.client_id << "][" << rpc.serverAddr << "] "
                  << "3.Receive PIR result." << std::endl;
        for (size_t i = 0; i < rpc.num_slice; i++)
        {
            ans.push_back(rpc.stringToM256i(reply.answer().substr(i * 32, 32)));
        }
    }
    else
    {
        std::cout << "RPC failed" << std::endl;
        std::cout << status.error_code() << ": " << status.error_message()
                  << std::endl;
        for (size_t i = 0; i < rpc.num_slice; i++)
        {
            ans.push_back(__m256i());
        }
    }
}