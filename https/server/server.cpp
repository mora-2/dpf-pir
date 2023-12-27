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
#include <fstream>
#include <nlohmann/json.hpp>

namespace po = boost::program_options;
using json = nlohmann::json;
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

public:
    DpfPirImpl(uint8_t server_id, size_t logN, vector<string> &db_keys, vector<string> &db_elems) : server_id(server_id), logN(logN)
    {
        assert(db_keys.size() <= ((1ULL << logN) - 1));
        assert(db_keys.size() == db_elems.size());
        this->db_size = db_keys.size();
        // Fill Datastore
        for (size_t i = 0; i < db_size; i++)
        {
            this->db.push_back(db_keys[i], db_elems[i], hashdatastore::KeywordType::STRING);
        }
        // Pad
        if (db_size % 8 != 0)
        {
            for (size_t i = 0; i < (8 - db_size % 8); i++)
            {
                this->db.push_back("", "", hashdatastore::KeywordType::STRING);
            }
        }
    };
    DpfPirImpl(uint8_t server_id, size_t logN, string json_data_path) : server_id(server_id), logN(logN)
    {
        std::ifstream file(json_data_path);
        if (!file.is_open())
        {
            std::cerr << "Error opening file.\n";
        }

        json jsonData;
        file >> jsonData;

        std::vector<std::string> db_keys;
        std::vector<std::string> db_elems;

        for (auto it = jsonData.begin(); it != jsonData.end(); ++it)
        {
            db_keys.push_back(it.key());
            db_elems.push_back(it.value());
        }

        assert(db_keys.size() <= ((1ULL << logN) - 1));
        assert(db_keys.size() == db_elems.size());
        this->db_size = db_keys.size();
        // Fill Datastore
        for (size_t i = 0; i < db_size; i++)
        {
            this->db.push_back(db_keys[i], db_elems[i], hashdatastore::KeywordType::HASH);
        }
        // Pad
        if (db_size % 8 != 0)
        {
            for (size_t i = 0; i < (8 - db_size % 8); i++)
            {
                this->db.push_back("", "", hashdatastore::KeywordType::HASH);
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
        DPF::EvalKeywords(func_key, db.hashs_, logN, query);

        /* answer query */
        hashdatastore::hash_type answer = db.answer_pir2(query);

        /* send answer */
        response->set_answer(m256iToStr(answer));

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
};

void RunServer(uint8_t server_id)
{
    // vector<string> db_keys = {"a", "b", "c", "d"}; // logN = 22, max_bits = 2
    // vector<string> db_elems = {"Aapple", "Abanana", "Acat", "Adog"};
    string json_path = "/home/yuance/Work/Encryption/PIR/code/PIR/dpf-pir/test/data/random_data.json";
    size_t logN = 48; // 48 bit hash for one million entries
    DpfPirImpl service(server_id, logN, json_path);

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