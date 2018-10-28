#pragma once

#include <memory>
#include <string>
#include <map>

#include <libserialport.h>

using SerialPort = std::unique_ptr<sp_port, decltype(&sp_close)>;

SerialPort open_port(std::string const& name);
std::map<std::string, bool> get_sm_state(sp_port * serial_port);
bool game_started(sp_port * serial_port);
bool entered_ship(sp_port * serial_port);
