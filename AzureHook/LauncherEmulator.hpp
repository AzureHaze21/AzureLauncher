#pragma once

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

#include <curl/curl.h>
#include <curl/easy.h>
#pragma comment(lib, "crypt32")

#include <cstdlib>
#include <iostream>
#include <format>
#include <sstream>

#include <nlohmann/json.hpp>

#include "Hook.h"

namespace Haapi
{
    static std::size_t WriteCallback(void* data, size_t size, size_t nmemb, void* pUserData)
    {
        ((std::string*)pUserData)->append((char*)data, nmemb);
        return nmemb * size;
    }

    static std::optional<std::string> GetToken(std::string const& login, std::string const& password, std::string const& endpoint)
    {
        try
        {
            CURL* curl;
            CURLcode res;
            curl = curl_easy_init();
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            std::string response;
            const auto requestData = std::format("login={}&password={}&game_id=101", login, password);

            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
            curl_easy_setopt(curl, CURLOPT_URL, "https://haapi.ankama.com/json/Ankama/v5/Api/CreateApiKey");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Zaap 3.7.4");
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestData.data());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            if (endpoint.length() > 0)
                curl_easy_setopt(curl, CURLOPT_INTERFACE, endpoint.data());

            res = curl_easy_perform(curl);

            std::cout << response.data() << std::endl;


            auto json = nlohmann::json::parse(response);
            auto apiKey = json["key"].get<std::string>();

            response.clear();

            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
            curl_easy_setopt(curl, CURLOPT_URL, "https://haapi.ankama.com/json/Ankama/v5/Account/CreateToken?game=101");
            headers = curl_slist_append(headers, std::format("apiKey: {}", apiKey.data()).data());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            res = curl_easy_perform(curl);

            try
            {
                json = nlohmann::json::parse(response);
                const auto token = json["token"].get<std::string>();

                return token;
            }
            catch (nlohmann::json::parse_error& err)
            {
                std::cerr << "[ERROR] " << err.what() << std::endl;
            }

            return {};
        }
        catch (...)
        {
            std::cout << "Error: Failed to retrieve game token" << std::endl;
        }

        return {};
    }
}

using boost::asio::ip::tcp;

class session
{
private:

    const std::string login_, password_, endpoint_;

public:
    session(boost::asio::io_context& io_context, std::string const& login, std::string const& password, std::string const& endpoint)
        : socket_(io_context), login_(login), password_(password), endpoint_(endpoint)
    {

    }

    tcp::socket& socket()
    {
        return socket_;
    }

    void start()
    {
        boost::asio::async_read_until(
            socket_,
            clientStreamBuf,
            '\0',
            boost::bind(&session::handle_client_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
        );
    }

private:

    std::string GetMessage(std::size_t const& bytes_transferred)
    {
        std::string msg{
          boost::asio::buffers_begin(clientStreamBuf.data()),
          boost::asio::buffers_begin(clientStreamBuf.data()) + bytes_transferred - 1
        };

        clientStreamBuf.consume(bytes_transferred);

        return msg;
    }

    void handle_client_read(const boost::system::error_code& error, size_t bytes_transferred)
    {
        if (!error)
        {
            const auto msg = GetMessage(bytes_transferred);

            std::cout << "[client] " << msg.data() << std::endl;

            std::stringstream ss(msg);
            std::vector<std::string> tokens;
            std::string s;

            while (ss >> s)
            {
                tokens.push_back(s);
            }

            if (tokens.size() > 0 && (!tokens[0].compare("connect") || !tokens[0].compare("auth_getGameToken")))
            {
                const auto token = Haapi::GetToken(login_, password_, endpoint_);
                const std::string authMsg = std::format("auth_getGameToken {}", token ? token->data() : "-1") + '\0';

                std::cout << "[launcher] " << authMsg.data() << std::endl;

                boost::asio::async_write(socket_,
                    boost::asio::buffer(authMsg.data(), authMsg.size()),
                    boost::bind(&session::handle_server_write, this,
                        boost::asio::placeholders::error));
            }
        }
        else
        {
            delete this;
        }
    }

    void handle_server_write(const boost::system::error_code& error)
    {
        if (!error)
        {
            boost::asio::async_read_until(
                socket_,
                clientStreamBuf,
                '\0',
                boost::bind(&session::handle_client_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
            );
        }
        else
        {
            delete this;
        }
    }

    tcp::socket socket_;
    static constexpr std::size_t bufferLength{ (1 << 11) };
    boost::asio::streambuf clientStreamBuf;
    boost::asio::streambuf serverStreamBuf;
};

class LauncherEmulator
{
public:
    LauncherEmulator(boost::asio::io_context& io_context, short port, std::string const& login = "", std::string const& password = "", std::string const& endpoint = "") :
        io_context_(io_context), login_(login), password_(password), endpoint_(endpoint), acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        start_accept();
    }

private:
    void start_accept()
    {
        session* new_session = new session(io_context_, login_, password_, endpoint_);
        acceptor_.async_accept(new_session->socket(),
            boost::bind(&LauncherEmulator::handle_accept, this, new_session,
                boost::asio::placeholders::error));
    }

    void handle_accept(session* new_session,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            new_session->start();
        }
        else
        {
            delete new_session;
        }

        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    const std::string login_, password_, endpoint_;
};
