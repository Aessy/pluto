#include "HttpServer.h"

#include <map>

namespace social
{

static std::map<unsigned int, std::string> const http_status_codes
{
    { 100, "Continue"            }
   ,{ 101, "Switching Protocols" }
   ,{ 200, "OK"                  }
   ,{ 400, "Bad Request"         }
   ,{ 401, "Unauthorized"        }
   ,{ 401, "Not Found"           }
   ,{ 401, "Method Not Allowed"  }
};

std::string getStatusCode(unsigned int code)
{
    auto i = http_status_codes.find(code);
    if (i != http_status_codes.end())
        return i->second;
    return "Unknown";
}

}
