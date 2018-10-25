#pragma once

#include <memory>
#include <string>
#include <map>

#include <libserialport.h>

using SerialPort = std::unique_ptr<sp_port, decltype(&sp_free_port)>;

SerialPort open_port(std::string const& name);
std::map<std::string, bool> get_sm_state(sp_port * serial_port);
