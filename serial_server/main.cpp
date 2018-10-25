#include <chrono>
#include <thread>
#include <iostream>

#include "HttpServer.h"
#include "super_metroid.hpp"
#include "json11.hpp"
#include "httplib.h"

int main()
{

    while (true)
    {
        auto port = open_port("/dev/ttyACM0");
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

                        rsp.status = started ? 200 : 404;
                    });
            svr.Get("/game_ended", [&port, &mutex](auto const& req, auto & rsp)
                    {
                        std::lock_guard<std::mutex> guard(mutex);
                        bool ended = false;
                        try
                        {
                            auto port = open_port("/dev/ttyACM0");
                            if (!port)
                            {
                                return;
                            }
                            std::cout << "Got /game_ended \n";
                            ended = entered_ship(port.get());
                        } catch(std::exception const& e)
                        {
                            std::cerr << "Serial error: " << e.what() << '\n';
                            return;
                        }

                        rsp.status = ended ? 200 : 404;
                    });

            svr.listen("localhost", 8080);
        } catch (std::exception const& e) {
            std::cerr << "Exception: " << e.what() << '\n';
        }
    }


}
