#include <filesystem>
#include <iostream>

#include <boost/program_options.hpp>

#include "client.hpp"
#include "receiver.hpp"
#include "sender.hpp"

namespace PingPong
{

namespace po = boost::program_options;

Operation readArguments(int argc, char **argv)
{
    Operation op;

    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help", "Available options:\nsend <filepath>\nreceive <code-phrase>")
        ("send", po::value<fs::path>(), "File to send")
        ("receive", po::value<std::string>(), "File to send");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << "\n";
        op.type = EOperationType::Help;
        return op;
    }

    if (vm.count("send"))
    {
        op.type = EOperationType::Send;
        op.filepath = vm["send"].as<fs::path>();
        DBG_LOG("Client wants to send file ", op.filepath);
        if (!fs::exists(op.filepath))
        {
            throw std::runtime_error("File doesn't exist");
        }
    }
    else if (vm.count("receive"))
    {
        op.type = EOperationType::Receive;
        op.receival_code_phrase = vm["receive"].as<std::string>();
        DBG_LOG("Client wants to receive file by a code phrase ", op.receival_code_phrase);
    }
    else
    {
        throw std::runtime_error("Invalid arguments");
    }

    return op;
}

} // namespace PingPong

int main(int argc, char *argv[])
{
    PingPong::Operation op;

    try
    {
        op = PingPong::readArguments(argc, argv);
    }
    catch (std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cerr << "Exception of unknown type!\n";
        return 1;
    }

    if (op.type == PingPong::EOperationType::Help)
    {
        return 0;
    }

    bool result;

    if (op.type == PingPong::EOperationType::Send)
    {
        result = sendRoutine(op);
    }
    else if (op.type == PingPong::EOperationType::Receive)
    {
        result = receiveRoutine(op);
    }

    DBG_LOG("Exiting...");

    return !result;
}
