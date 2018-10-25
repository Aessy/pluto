#ifndef SERVICE_H_INCLUDED
#define SERIVCE_H_INCLUDED

#include <memory>

#include "boost/asio.hpp"
#include "boost/bind.hpp"

#ifdef SSL
#include "boost/asio/ssl.hpp"
#endif

#include <thread>
#include <future>
#include <type_traits>

namespace social
{

using NormalSocketType = boost::asio::buffered_stream<boost::asio::ip::tcp::socket>;

#ifdef SSL
using SslSocketType = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
#endif

template<typename SocketType>
inline constexpr bool is_ssl()
{

#ifdef SSL
    return std::is_same<SocketType, SslSocketType>::value;
#else
    return false;
#endif
}

template<typename SocketType>
struct ServiceBase
{
    ServiceBase(unsigned short port)
        : io_service {  }
        , acceptor   { io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port) }
    {}

    void run(size_t threads)
    {
        accept();

        std::vector<std::future<void>> futures;
        for (size_t i = 1; i < threads; ++i)
        {
            futures.push_back(std::async(std::launch::async,
                [this] { this->io_service.run(); }));
        }

        io_service.run();

        for (auto & thread : futures)
        {
            thread.get();
        }
    }

protected:
    virtual void accept() = 0;
    virtual void onConnect(std::shared_ptr<SocketType> & socket) = 0;


public:
    boost::asio::io_service io_service;

protected:
    boost::asio::ip::tcp::acceptor acceptor;

    std::shared_ptr<SocketType> socket;
};

template<typename SocketType>
struct Service : public ServiceBase<SocketType>
{};

template<>
struct Service<NormalSocketType> : public ServiceBase<NormalSocketType>
{
    Service(unsigned short port)
        : ServiceBase<NormalSocketType> { port }
    {}

    void accept() override
    {
        socket = std::make_shared<NormalSocketType>(io_service);

        acceptor.async_accept(socket->lowest_layer(),
                boost::bind(&Service<NormalSocketType>::handle_accept, this, socket,
                boost::asio::placeholders::error));
    }

private:
    void handle_accept(std::shared_ptr<NormalSocketType> socket, boost::system::error_code const error)
    {
        std::string const s = socket->lowest_layer().remote_endpoint().address().to_string();
        boost::asio::ip::tcp::no_delay option(true);
        socket->lowest_layer().set_option(option);

        accept();
        onConnect(socket);

    }
};

#ifdef SSL
template<>
struct Service<SslSocketType> : public ServiceBase<SslSocketType>
{
    Service(unsigned short port, std::string const& certificate, std::string const& private_key)
        : ServiceBase<SslSocketType> { port }
        , ssl_context(boost::asio::ssl::context::sslv23)
    {
        ssl_context.set_options(
                  boost::asio::ssl::context::default_workarounds
                | boost::asio::ssl::context::no_sslv2);
        ssl_context.use_certificate_chain_file(certificate);
        ssl_context.use_private_key_file(private_key, boost::asio::ssl::context::pem);
    }

    void accept() override
    {
        socket = std::make_shared<SslSocketType>(io_service, ssl_context);

        acceptor.async_accept(socket->lowest_layer(),
                boost::bind(&Service<SslSocketType>::handle_accept, this, socket,
                boost::asio::placeholders::error));
    }

private:
    void handle_accept(std::shared_ptr<SslSocketType> socket, boost::system::error_code const error)
    {
        if (error)
        {
            return;
        }

       accept();

       socket->async_handshake(boost::asio::ssl::stream_base::server,
               boost::bind(&Service<SslSocketType>::handshakeComplete, this, socket,
                   boost::asio::placeholders::error));
    }

    void handshakeComplete(std::shared_ptr<SslSocketType> socket, boost::system::error_code const error)
    {
        if (error)
        {
            return;
        }

        accept();
        onConnect(socket);
    }


protected:
    boost::asio::ssl::context ssl_context;
};

#endif

}

#endif
