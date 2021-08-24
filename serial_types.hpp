#pragma once
#include <boost/json.hpp>

class NetworkObject;
class Vector3;
class Vector3{
public:
    Vector3() : json(new boost::json::object) {}
    ~Vector3() { delete json; }
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    boost::json::object* json;
    boost::json::object serialize() {
        delete json;
        json = new boost::json::object;
        json["pos_x"] = x;
        json["pos_y"] = y;
        json["pos_z"] = z;
        return *json;
    }
};



class NetworkObject{};