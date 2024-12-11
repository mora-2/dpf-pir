#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include "omp.h"

// 计算哈希冲突概率的函数
double calculateCollisionProbability(size_t dataBits, size_t hashValueBits)
{
    double dataSize = pow(2, dataBits);
    double hashTableSize = pow(2, hashValueBits);

    // 计算哈希冲突的概率
    double collisionProbability = 1.0 - exp(-(dataSize * (dataSize - 1)) / (2.0 * hashTableSize));

    return collisionProbability;
}
TEST(HashCollision, hash)
{
    for (int dataBits = 20; dataBits < 40; dataBits++)
    {
        double dataSize = pow(2, dataBits);
        for (int hashValueBits = 20; hashValueBits < 100000; hashValueBits++)
        {
            double hashTableSize = pow(2, hashValueBits);
            // 计算哈希冲突的概率
            double collisionProbability = 1.0 - exp(-(dataSize * (dataSize - 1)) / (2.0 * hashTableSize));
            if (collisionProbability < 0.0013)
            {
                std::cout << dataSize << std::endl;
                std::cout << hashValueBits << std::endl;
                std::cout << collisionProbability << std::endl;
                // std::cout << "dataSize:" << dataSize << std::endl;
                // std::cout << "hashValueBits:" << hashValueBits << std::endl;
                // std::cout << "collisionProbability:" << collisionProbability << std::endl;
                break;
            }
        }
    }
}

TEST(DISABLED_HashCollision, HashCollision_test)
{
    // 数据的数量
    size_t dataBits = 30;

    // 哈希值的位数
    size_t hashValueBits = 68; // 你可以根据实际情况调整哈希值的位数

    // 计算哈希冲突概率
    double collisionProbability = calculateCollisionProbability(dataBits, hashValueBits);

    // 输出结果
    std::cout << "Hash Collision Probability: " << collisionProbability << std::endl;
}
using json = nlohmann::json;
TEST(DISABLED_JSON_SWAP, swap)
{
    std::string json_file_path = "/home/yuance/Work/Encryption/PIR/code/PIR/dpf-pir/test/data/random_data.json";
    // 打开 JSON 文件
    std::ifstream file(json_file_path);
    if (!file.is_open())
    {
        std::cerr << "Error opening file.\n";
    }

    // 从文件读取 JSON 数据
    json originalData;
    file >> originalData;

    // 创建新的 JSON 对象并互换键值
    json swappedData;
    for (auto it = originalData.begin(); it != originalData.end(); ++it)
    {
        swappedData[it.value()] = it.key();
    }

    // 将新的 JSON 对象写回文件
    std::ofstream outputFile(json_file_path);
    if (!outputFile.is_open())
    {
        std::cerr << "Error opening file for writing.\n";
    }

    outputFile << swappedData.dump(); // 没有缩进

    std::cout << "Key and value swapped and written back to file.\n";
}

TEST(DISABLED_JSON_READ, json_test)
{
    // 打开 JSON 文件
    std::ifstream file("/home/yuance/Work/Encryption/PIR/code/PIR/dpf-pir/test/data/random_data.json");
    if (!file.is_open())
    {
        std::cerr << "Error opening file.\n";
    }
    GTEST_ASSERT_EQ(!file.is_open(), false);

    // 从文件读取 JSON 数据
    json jsonData;
    file >> jsonData;

    // 创建两个向量用于存储键和值
    std::vector<std::string> keys;
    std::vector<std::string> values;

    // 遍历 JSON 对象的成员
    for (auto it = jsonData.begin(); it != jsonData.end(); ++it)
    {
        // 提取键并将其添加到键向量
        keys.push_back(it.key());

        // 提取值并将其添加到值向量
        values.push_back(it.value());
    }

    // 输出键和值
    std::cout << "Keys:\t Values:\n";
    for (int i = 0; i < 10; i++)
    {
        std::cout << keys[i] << "\t" << values[i] << std::endl;
    }
}

TEST(DISABLED_JSON_READ, json_trunc1)
{
    // 读取包含1 million对key-value的JSON文件
    std::ifstream inputJsonFile("/home/yuance/Work/Encryption/PIR/code/PIR/dpf-pir/test/data/random_data.json");
    nlohmann::json root;
    inputJsonFile >> root;
    inputJsonFile.close();

    // 创建一个新的 JSON 对象以保存1k对
    nlohmann::json newJson;

    // 从原始 JSON 中抽取1k对
    for (int i = 0; i < 10000; ++i)
    {
        // 获取对应的键
        auto it = std::next(root.begin(), i);
        std::string key = it.key();
        std::string value = it.value();

        // 确保原始 JSON 中包含该 key
        if (root.contains(key))
        {
            // 将 key-value 添加到新的 JSON 对象中
            std::string newkey = value + value;
            newJson[newkey] = root[key];
            std::cout << "Added key: " << key << std::endl;
        }
        else
        {
            std::cout << "Key not found: " << key << std::endl;
        }
    }

    // 将新的 JSON 对象写入新的 JSON 文件
    std::ofstream outputJsonFile("/home/yuance/Work/Encryption/PIR/code/PIR/dpf-pir/test/data/random_data10k.json");
    outputJsonFile << newJson.dump(); // 4 是缩进级别，可根据需要调整
    outputJsonFile.close();
}

TEST(DISABLED_Hash, Hash48bit)
{
    std::string input = "Hello, World!"; // 你要哈希的输入数据

    // 使用 std::hash 获取哈希值
    std::hash<std::string> hashFunction;
    size_t hashValue = hashFunction(input);

    // 获取低 48 位
    size_t truncatedHashValue = hashValue & 0xFFFFFFFFFFFF;

    // 输出哈希值
    std::cout << "Full Hash Value: " << hashValue << "\n";
    std::cout << "Truncated Hash Value (48 bits): " << truncatedHashValue << "\n";
}

TEST(DISABLED_OMP, omp_test)
{
#pragma omp parallel for num_threads(32)
    for (size_t i = 0; i < 100; i++)
    {
        std::cout << "i:" << i << std::endl;
        // sleep(2);
    }
    std::cout << "Size of size_t: " << sizeof(size_t) << " bytes" << std::endl;
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}