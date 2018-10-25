#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED

#include "service.h"

#include "boost/asio.hpp"
#include "boost/algorithm/string.hpp"

#include <iostream>
#include <istream>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <functional>

namespace social
{

struct Request
{
    std::string type, path, version;
    std::map<std::string, std::string> headers;
    unsigned int content_size = 0;
    boost::asio::streambuf streambuf;

    bool parseHeader()
    {
        std::istream request_stream(&streambuf);

        request_stream >> type >> path >> version;

        std::string header;
        std::getline(request_stream, header);

        while (std::getline(request_stream, header) && header != "\r")
        {
            size_t pos = header.find(": ");
            if (pos == std::string::npos)
                continue;

            std::string tag = header.substr(0, pos);
            std::string value = header.substr(pos+2, header.size());
            headers[tag] = value;
            if (tag == "Content-Length")
                content_size = stoull(value);
            else if (tag == "content-length")
                content_size = stoull(value);
        }

        return true;
    }

    std::string contentAsString()
    {
        std::istream stream(&streambuf);
        std::stringstream ss;
        ss << stream.rdbuf();
        return ss.str();
    }
};

std::string getStatusCode(unsigned int code);

struct Response
{
    Response(unsigned int status_code)
        : status_code { status_code }
        , reason { getStatusCode(status_code) }
    {}

    std::shared_ptr<boost::asio::streambuf> pack() const
    {
        auto out = std::make_shared<boost::asio::streambuf>();

        std::ostream stream(&(*out));
        stream << "HTTP/1.1 " << status_code << " " << reason << "\r\n";
        for (auto const& [k, v] : headers)
        {
            stream << k << ": " << v << "\r\n";
        }
        stream << "Content-Length: " << content.size() << "\r\n";
        stream << "\r\n";
        stream.write(reinterpret_cast<char const*>(content.data()), content.size());
        return out;
    }

    unsigned int status_code;
    std::string reason;

    std::map<std::string, std::string> headers;
    std::vector<unsigned char> content;
};

struct RequestString
{
    std::string version;
    std::string path;
};

template<typename SocketType>
struct HttpBase 
{
    void startReceive(std::shared_ptr<SocketType> socket)
    {
        std::shared_ptr<Request> request(new Request());

        boost::asio::async_read_until(*socket, request->streambuf, "\r\n\r\n",
                    [this, socket, request](const boost::system::error_code& ec, size_t bytes_transferred) {
                        if (ec == boost::asio::error::not_found)
                        {
                            return;
                        }
                        if (ec)
                        {
                            return;
                        }

                        size_t rest = request->streambuf.size() - bytes_transferred;
                        if (!request->parseHeader())
                        {
                            return;
                        }

                        // Read rest of content from socket is needed.
                        if (request->content_size > 0 && request->content_size != rest)
                        {
                            // Read content with timeout.
                            boost::asio::async_read(*socket, request->streambuf,
                                    boost::asio::transfer_exactly(request->content_size - rest),
                                    [this, socket, request]
                                    (boost::system::error_code const& ec, size_t bytes_received)
                                    {

                                        if (ec)
                                        {
                                            return;
                                        }

                                        // Process request.
                                        processRequest(request, socket);
                                    });
                        }
                        else
                        {
                            // Process request.
                            processRequest(request, socket);
                        }

                    });
    }

    void processRequest(std::shared_ptr<Request> request, std::shared_ptr<SocketType> socket)
    {

        std::string lookup_str = request->type + request->path;
        auto it = callbacks.find(lookup_str);
        if (it == callbacks.end())
        {
            processOutBuffer(Response(404).pack(), socket);
        }
        else
        {
            auto response = (it->second)(*request);

            processOutBuffer(response.pack(), socket);
        }
    }

    void processOutBuffer(std::shared_ptr<boost::asio::streambuf> stream, std::shared_ptr<SocketType> socket)
    {
        if (stream->size())
        {
            boost::asio::async_write(*socket, *stream, [this, stream, socket](boost::system::error_code const& ec, size_t bytes_transferred)
            {
                handleWritten(stream, socket, ec, bytes_transferred);
            });
        }
    }

    void handleWritten(std::shared_ptr<boost::asio::streambuf> stream, std::shared_ptr<SocketType> socket, boost::system::error_code const& ec, size_t bytes_transferred)
    {
        if (ec)
        {
            return;
        }

        flush(socket);
    }

    void registerCallback(std::string const& type, std::string const& path, std::function<Response(Request &)> callback)
    {
        callbacks[type+path] = callback;
    }

    void flush(std::shared_ptr<SocketType> & socket)
    {
        // Only flush normal socket
        if constexpr(!is_ssl<SocketType>())
        {
            socket->flush();
        }
    }

private:
    // Example: Map GET/blabla/bla -> callback
    std::map<std::string, std::function<Response(Request &)>> callbacks;
};

/**
 * Wrapper for HTTP server
 */
struct HttpServer : public Service<NormalSocketType>, public HttpBase<NormalSocketType>
{
    HttpServer(unsigned short port)
        : Service<NormalSocketType> { port }
        , HttpBase {}
    {}

    virtual ~HttpServer()
    {}

protected:
    void onConnect(std::shared_ptr<NormalSocketType> & socket) override
    {
        startReceive(socket);
    }
};

#ifdef SSL
struct HttpsServer : public Service<SslSocketType>, public HttpBase<SslSocketType>
{
    HttpsServer(unsigned short port, std::string const& cert, std::string const& key)
        : Service<SslSocketType> { port, cert, key }
        , HttpBase {}
    {}

    virtual ~HttpsServer()
    {}

protected:
    void onConnect(std::shared_ptr<SslSocketType> & socket) override
    {
        startReceive(socket);
    }
};
#endif

}

#endif
