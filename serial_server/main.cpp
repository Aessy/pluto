#include <chrono>
#include <thread>
#include <iostream>

#include "HttpServer.h"
#include "super_metroid.hpp"
#include "json11.hpp"
#include "httplib.h"

int main(int argc, char * argv[])
{
    if (argc != 2)
    {
        std::cout << "Missing arguments\n";
        return 0;
    }

    while (true)
    {
        auto port = open_port(argv[1]);
        try
        {
            std::cout << "Starting server\n";

            httplib::Server svr;

            std::mutex mutex;

            svr.Get("/state", [&port, &mutex](auto const& req, auto & rsp)
                    {
                        std::lock_guard guard(mutex);
                        json11::Json::object obj;
                        try
                        {
                            if (!port)
                            {
                                return;
                            }

                            std::cout << "Got state request\n";
                            auto sm_state = get_sm_state(port.get());
                            for (auto & [key, value] : sm_state)
                            {
                                obj[key] = value;
                            }
                        } catch(std::exception const& e)
                        {
                            std::cerr << "Serial error: " << e.what() << '\n';
                            return;
                        }


                        auto jsn = json11::Json(obj).dump();

                        rsp.set_content(jsn, "json/application");
                        rsp.status = 200;
                        std::cout << "Setting rsp\n";
                    });
            svr.Get("/game_started", [&port, &mutex](auto const& req, auto & rsp)
                    {
                        std::lock_guard<std::mutex> guard(mutex);
                        bool started = false;
                        try
                        {
                            if (!port)
                            {
                                return;
                            }
                            std::cout << "Got /game_started request\n";
                            started = game_started(port.get());
                        } catch(std::exception const& e)
                        {
                            std::cerr << "Serial error: " << e.what() << '\n';
                            return;
                        }

                        std::cout << "Game started: " << started << '\n';

                        auto content = json11::Json(json11::Json::object({{"started", started}})).dump();
                        rsp.set_content(content, "json/application");
                        rsp.status = 200;
                    });
            svr.Get("/game_ended", [&port, &mutex](auto const& req, auto & rsp)
                    {
                        std::lock_guard<std::mutex> guard(mutex);
                        bool ended = false;
                        try
                        {
                            if (!port)
                            {
                                return;
                            }
                            std::cout << "Got /game_ended \n";
                            ended = entered_ship(port.get());
                            std::cout << "Game ended: " << ended << '\n';

                        } catch(std::exception const& e)
                        {
                            std::cerr << "Serial error: " << e.what() << '\n';
                            return;
                        }

                        auto content = json11::Json(json11::Json::object({{"ended", ended}})).dump();
                        rsp.set_content(content, "json/application");
                        rsp.status = 200;
                    });

            svr.listen("localhost", 8080);
        } catch (std::exception const& e) {
            std::cerr << "Exception: " << e.what() << '\n';
        }
    }


}
