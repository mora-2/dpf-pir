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
#include <bitset>

namespace po = boost::program_options;
using json = nlohmann::json;
using namespace std;
using dpfpir::Answer;
using dpfpir::DPFPIRInterface;
using dpfpir::FuncKey;
using dpfpir::Info;
using dpfpir::Params;
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
    size_t num_slice; // num_value_slice

public:
    DpfPirImpl(uint8_t server_id, size_t logN, vector<string> &db_keys, vector<string> &db_elems) : server_id(server_id), logN(logN)
    {
        assert(db_keys.size() <= ((1ULL << logN) - 1));
        assert(db_keys.size() == db_elems.size());
        this->db_size = db_keys.size();
        this->num_slice = getnum(db_elems);
        db.resize_data(num_slice);
        // Fill Datastore
        for (size_t i = 0; i < db_size; i++)
        {
            this->db.push_back(db_keys[i], hashdatastore::KeywordType::STRING, str2vecstr(db_elems[i], num_slice), num_slice);
        }
        // Pad
        if (db_size % 8 != 0)
        {
            vector<string> emp;
            for (size_t i = 0; i < num_slice; i++)
            {
                emp.push_back("");
            }
            for (size_t i = 0; i < (8 - db_size % 8); i++)
            {
                this->db.push_back("", hashdatastore::KeywordType::STRING, emp, num_slice);
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

        this->num_slice = getnum(db_elems);
        db.resize_data(num_slice);
        assert(db_keys.size() <= ((1ULL << logN) - 1));
        assert(db_keys.size() == db_elems.size());
        this->db_size = db_keys.size();
        // Fill Datastore
        for (size_t i = 0; i < db_size; i++)
        {
            this->db.push_back(db_keys[i], hashdatastore::KeywordType::HASH, str2vecstr(db_elems[i], num_slice), num_slice);
        }
        // Pad
        if (db_size % 8 != 0)
        {
            vector<string> emp;
            for (size_t i = 0; i < num_slice; i++)
            {
                emp.push_back("");
            }
            for (size_t i = 0; i < (8 - db_size % 8); i++)
            {
                this->db.push_back("", hashdatastore::KeywordType::HASH, emp, num_slice);
            }
        }
    };

    Status DpfParams(ServerContext *context, const Info *request, Params *response)
    {
        const string client_id = context->client_metadata().find("client_id")->second.data();
        std::cout << "[" << client_id << "] "
                  << "1.Sending Params.";

        response->set_logn(this->logN);
        response->set_num_slice(this->num_slice);
        std::cout << "\r[" << client_id << "] "
                  << "1.Params sent.   " << std::endl;
        return Status::OK;
    }

    Status DpfPir(ServerContext *context, const FuncKey *request, Answer *response)
    {
        const string client_id = context->client_metadata().find("client_id")->second.data();
        std::cout << "\r[" << client_id << "] "
                  << "2.PIR..." << std::flush;

        /* receive func_key */
        std::vector<uint8_t> func_key(request->funckey().begin(), request->funckey().end());

        /* make query vector */
        std::vector<uint8_t> query;
        DPF::EvalKeywords(func_key, db.hashs_, logN, query);

        /* answer query */
        std::vector<hashdatastore::hash_type, hashdatastore::HashTypeAllocator> answer;
        for (size_t i = 0; i < num_slice; i++)
        {
            answer.push_back(db.answer_pir2(query, i));
        }

        /* set answer */
        std::string ans;
        for (size_t i = 0; i < num_slice; i++)
        {
            ans += m256iToStr(answer[i]);
        }

        /* send answer */
        response->set_answer(ans);

        std::cout << "\r[" << client_id << "] "
                  << "2.PIR end." << std::endl;
        return Status::OK;
    }

private:
    // Convert __m256i to string
    std::string m256iToStr(__m256i value)
    {
        alignas(32) uint8_t result[32]; // Assuming __m256i is 256 bits (32 bytes)
        _mm256_store_si256((__m256i *)result, value);

        std::string resultString;
        for (size_t i = 0; i < 32; ++i)
        {
            resultString += static_cast<char>(result[i]);
        }

        return resultString;
    }

    std::vector<std::string> str2vecstr(std::string s, size_t num_slice)
    {
        std::vector<std::string> result;
        size_t n = s.size();
        for (size_t i = 0; i < num_slice; i++)
        {
            if (n > 32)
            {
                result.push_back(s.substr(i * 32, 32));
                n = n - 32;
            }
            else
            {
                result.push_back(s.substr(i * 32, n));
                break;
            }
        }
        if (result.size() < num_slice)
        {
            result.resize(num_slice);
        }
        return result;
    }

    size_t getnum(std::vector<std::string> &s)
    {
        size_t n = 0;
        for (size_t i = 0; i < s.size(); i++)
        {
            if (s[i].size() > n)
            {
                n = s[i].size();
            }
        }
        if (n % 32 == 0)
        {
            return n / 32;
        }
        return n / 32 + 1;
    }
};

void RunServer(uint8_t server_id)
{
    // vector<string> db_keys = {"a", "b", "c", "d"}; // logN = 22, max_bits = 2
    // vector<string> db_elems = {"AappleAappleAappleAappleAappleaaAappleAAHSJAappleAappleAappleAappleAappleaaAappleAAHSJ", "AbananaAbanana", "AcatAcat", "AdogAdog"};
    string json_path = "/home/yuance/Work/Encryption/PIR/code/PIR/dpf-pir/test/data/random_data.json";
    size_t logN = 48; // 48 bit hash for one million entries
    DpfPirImpl service(server_id, logN, json_path);

    /* gRPC build */
    ServerBuilder builder;
    std::string server_address;
    if (server_id == 0)
        server_address = "0.0.0.0:50053";
    else if (server_id == 1)
        server_address = "0.0.0.0:50054";
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