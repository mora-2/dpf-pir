#include <iostream>
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include "dpf_pir.grpc.pb.h"
#include "dpf.h"
#include "hashdatastore.h"
#include <immintrin.h> // Include the necessary header for
#include <boost/program_options.hpp>
#include <stdexcept> // throw
#include <cassert>

namespace po = boost::program_options;
using namespace std;
using dpfpir::Answer;
using dpfpir::DPFPIRInterface;
using dpfpir::FuncKey;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;

class DpfPirImpl final : public DPFPIRInterface::Service
{
private:
    std::mutex mu_;
    uint8_t server_id;
    size_t logN; // number of keyword bits
    hashdatastore db;
    size_t db_size;
    size_t num;

public:
    DpfPirImpl(uint8_t server_id, size_t logN, vector<string> &db_keys, vector<string> &db_elems) : server_id(server_id), logN(logN)
    {
        assert(db_keys.size() <= (1 << logN - 1));
        assert(db_keys.size() == db_elems.size());
        this->db_size = db_keys.size();
        num = getnum(db_elems);
        // Fill Datastore
        db.init(num);
        for (size_t i = 0; i < db_size; i++)
        {
            this->db.push_back(db_keys[i], str2vecstr(db_elems[i], num),num);
        }
        // Pad
        if (db_size % 8 != 0)
        {
            vector<string> emp;
            for(int i=0; i<num; i++)
            {
                emp.push_back("");
            }
            for (size_t i = 0; i < (8 - db_size % 8); i++)
            {
                this->db.push_back("", emp, num);
            }
        }
    };

    Status DpfPir(ServerContext *context, const FuncKey *request, Answer *response)
    {
        const string client_id = context->client_metadata().find("client_id")->second.data();
        std::cout << "[" << client_id << "] "
                  << "1.Start PIR." << std::endl;

        /* receive func_key */
        std::vector<uint8_t> func_key(request->funckey().begin(), request->funckey().end());

        /* make query vector */
        std::vector<uint8_t> query;
        DPF::EvalKeywords(func_key, db.keyword_, logN, query);

        /* answer query */
        std::vector<hashdatastore::hash_type, AlignmentAllocator<hashdatastore::hash_type, sizeof(hashdatastore::hash_type)>> answer;
        for(int i=0; i<num; i++)
        {
            answer.push_back(db.answer_pir2_s(query,i));
        }
        /* send answer */
        std::string ans;
        for(int i=0; i<num; i++)
        {
            ans+=m256iToStr(answer[i]);
        }
        response->set_answer(ans);
        std::cout << "[" << client_id << "] "
                  << "2.End PIR." << std::endl;
        return Status::OK;
    }

private:
    // Convert __m256i to string
    std::string m256iToStr(__m256i value)
    {
        alignas(32) uint8_t result[32]; // Assuming __m256i is 256 bits (32 bytes)
        _mm256_store_si256((__m256i *)result, value);
        std::string resultString;
        for (int i = 0; i < 32; ++i)
        {
            resultString += static_cast<char>(result[i]);
        }
        return resultString;
    }

    std::vector<std::string> str2vecstr(std::string s, int num)
    {
        std::vector<std::string> result;
        int n = s.size();
        for(int i=0; i<num; i++)
        {
            if(n > 32){
                result.push_back(s.substr(i*32, 32));
                n = n-32;
            }
            else{
                result.push_back(s.substr(i*32, n));
                break;
            }
        }
        if(result.size() < num){
            result.resize(num);
        }
        return result;
    }

    int getnum(std::vector<std::string> &s)
    {
        int n=0;
        for(int i=0; i<s.size(); i++){
            if(s[i].size() > n){
                n = s[i].size();
            }
        }
        if(n%32==0) {return n/32;}
        return n/32 + 1;
    }
};

void RunServer(uint8_t server_id)
{
    vector<string> db_keys = {"a", "b", "c", "d"}; // logN = 22, max_bits = 2
    vector<string> db_elems = {"AappleAappleAappleAappleAappleaaAappleAAHSJAappleAappleAappleAappleAappleaaAappleAAHSJ","AbananaAbanana","AcatAcat","AdogAdog"};

    size_t logN = 22;
    DpfPirImpl service(server_id, logN, db_keys, db_elems);

    /* gRPC build */
    ServerBuilder builder;
    std::string server_address;
    if (server_id == 0)
        server_address = "0.0.0.0:50050";
    else if (server_id == 1)
        server_address = "0.0.0.0:50051";
    else
        throw std::invalid_argument("Invalid Server ID: " + std::to_string(server_id));

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<::grpc::Server> rpc_server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    /* wait for call */
    rpc_server->Wait();
}

int main(int argc, char *argv[])
{
#pragma region args
    /* args */
    uint8_t server_id;
    try
    {
        // def options
        po::options_description desc("Allowed options");
        desc.add_options()("help,h", "Produce help message")("id", po::value<std::string>()->required(), "server id (0/1)");

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
            server_id = std::stoi(vm["id"].as<std::string>());
            if (server_id != 0 && server_id != 1)
                throw("Invalid Server ID: " + std::to_string(server_id));
            // std::cout << "Input file: " << vm["input-file"].as<std::string>() << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#pragma endregion args

    /* run */
    RunServer(server_id);
    return 0;
}