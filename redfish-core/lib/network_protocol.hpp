/*
// Copyright (c) 2018 Intel Corporation
// Copyright (c) 2018 Ampere Computing LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include "error_messages.hpp"
#include "node.hpp"

#include <fstream>
#include <utils/ampere-utils.hpp>

namespace redfish
{

enum NetworkProtocolUnitStructFields
{
    NET_PROTO_UNIT_NAME,
    NET_PROTO_UNIT_DESC,
    NET_PROTO_UNIT_LOAD_STATE,
    NET_PROTO_UNIT_ACTIVE_STATE,
    NET_PROTO_UNIT_SUB_STATE,
    NET_PROTO_UNIT_DEVICE,
    NET_PROTO_UNIT_OBJ_PATH,
    NET_PROTO_UNIT_ALWAYS_0,
    NET_PROTO_UNIT_ALWAYS_EMPTY,
    NET_PROTO_UNIT_ALWAYS_ROOT_PATH
};

enum NetworkProtocolListenResponseElements
{
    NET_PROTO_LISTEN_TYPE,
    NET_PROTO_LISTEN_STREAM
};

/**
 * @brief D-Bus Unit structure returned in array from ListUnits Method
 */
using UnitStruct =
    std::tuple<std::string, std::string, std::string, std::string, std::string,
               std::string, sdbusplus::message::object_path, uint32_t,
               std::string, sdbusplus::message::object_path>;

struct ServiceConfiguration
{
    const char* socketFile;
    const char* servicePath;
    const char* socketPath;
};

const static boost::container::flat_map<boost::beast::string_view,
                                        ServiceConfiguration>
    protocolToDBus{
        {"SSH",
         {"dropbear.socket",
          "/org/freedesktop/systemd1/unit/dropbear_2eservice",
          "/org/freedesktop/systemd1/unit/dropbear_2esocket"}},
        {"HTTPS",
         {"phosphor-gevent.socket",
          "/org/freedesktop/systemd1/unit/phosphor_2dgevent_2eservice",
          "/org/freedesktop/systemd1/unit/phosphor_2dgevent_2esocket"}},
        {"IPMI",
         {"phosphor-ipmi-net.socket",
          "/org/freedesktop/systemd1/unit/phosphor_2dipmi_2dnet_2eservice",
          "/org/freedesktop/systemd1/unit/phosphor_2dipmi_2dnet_2esocket"}}};

class NetworkProtocol : public Node
{
  public:
    NetworkProtocol(CrowApp& app) :
        Node(app, "/redfish/v1/Managers/bmc/NetworkProtocol")
    {
        Node::json["@odata.type"] =
            "#ManagerNetworkProtocol.v1_1_0.ManagerNetworkProtocol";
        Node::json["@odata.id"] = "/redfish/v1/Managers/bmc/NetworkProtocol";
        Node::json["@odata.context"] =
            "/redfish/v1/"
            "$metadata#ManagerNetworkProtocol.ManagerNetworkProtocol";
        Node::json["Id"] = "NetworkProtocol";
        Node::json["Name"] = "Manager Network Protocol";
        Node::json["Description"] = "Manager Network Service";
        Node::json["Status"]["Health"] = "OK";
        Node::json["Status"]["HealthRollup"] = "OK";
        Node::json["Status"]["State"] = "Enabled";

        entityPrivileges = {
            {boost::beast::http::verb::get, {{"Login"}}},
            {boost::beast::http::verb::head, {{"Login"}}},
            {boost::beast::http::verb::patch, {{"ConfigureManager"}}},
            {boost::beast::http::verb::put, {{"ConfigureManager"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureManager"}}},
            {boost::beast::http::verb::post, {{"ConfigureManager"}}}};
    }

  private:
    void doGet(crow::Response& res, const crow::Request& req,
               const std::vector<std::string>& params) override
    {
        std::shared_ptr<AsyncResp> asyncResp = std::make_shared<AsyncResp>(res);

        getData(asyncResp);
    }

    void doPatch(crow::Response& res, const crow::Request& req,
                 const std::vector<std::string>& params) override
    {
        std::shared_ptr<AsyncResp> asyncResp = std::make_shared<AsyncResp>(res);

        nlohmann::json patchRequest;
        if (!json_util::processJsonFromRequest(res, req, patchRequest))
        {
            return;
        }

        for (const auto& item : patchRequest.items())
        {
            const std::string& key = item.key();
            if (protocolToDBus.find(key.c_str()) != protocolToDBus.end())
            {
                changeProtocolInfo(asyncResp, key, item.value());
            }
        }
    }

    std::string getHostName() const
    {
        std::string hostName;

        std::array<char, HOST_NAME_MAX> hostNameCStr;
        if (gethostname(hostNameCStr.data(), hostNameCStr.size()) == 0)
        {
            hostName = hostNameCStr.data();
        }
        return hostName;
    }

    void getData(const std::shared_ptr<AsyncResp>& asyncResp)
    {
        std::string hostName = getHostName();
        Node::json["HostName"] = hostName;
        Node::json["FQDN"] = hostName + DOMAIN_NAME;
        asyncResp->res.jsonValue = Node::json;

        for (auto& kv : protocolToDBus)
        {
            const char* socketPath = kv.second.socketPath;
            crow::connections::systemBus->async_method_call(
                [asyncResp, service{std::string(kv.first)}](
                    const boost::system::error_code ec,
                    const sdbusplus::message::variant<std::vector<
                        std::tuple<std::string, std::string>>>& resp) {
                    if (ec)
                    {
                        messages::addMessageToJson(asyncResp->res.jsonValue,
                                                   messages::internalError(),
                                                   "/" + service);
                        return;
                    }
                    const std::vector<std::tuple<std::string, std::string>>*
                        responsePtr = mapbox::getPtr<const std::vector<
                            std::tuple<std::string, std::string>>>(resp);
                    if (responsePtr == nullptr || responsePtr->size() < 1)
                    {
                        return;
                    }

                    const std::string& listenStream =
                        std::get<NET_PROTO_LISTEN_STREAM>((*responsePtr)[0]);
                    std::size_t lastColonPos = listenStream.rfind(":");
                    if (lastColonPos == std::string::npos)
                    {
                        // Not a port
                        return;
                    }
                    std::string portStr = listenStream.substr(lastColonPos + 1);
                    char* endPtr = nullptr;
                    // Use strtol instead of stroi to avoid
                    // exceptions
                    long port = std::strtol(portStr.c_str(), &endPtr, 10);

                    if (*endPtr != '\0' || portStr.empty())
                    {
                        // Invalid value
                        asyncResp->res.jsonValue[service]["Port"] = nullptr;
                    }
                    else
                    {
                        // Everything OK
                        asyncResp->res.jsonValue[service]["Port"] = port;
                    }
                },
                "org.freedesktop.systemd1", socketPath,
                "org.freedesktop.DBus.Properties", "Get",
                "org.freedesktop.systemd1.Socket", "Listen");

            crow::connections::systemBus->async_method_call(
                [asyncResp, service{std::string(kv.first)}](
                    const boost::system::error_code ec,
                    const sdbusplus::message::variant<std::string>& resp) {
                    if (ec)
                    {
                        messages::addMessageToJson(asyncResp->res.jsonValue,
                                                   messages::internalError(),
                                                   "/" + service);
                        return;
                    }

                    const std::string* state =
                        mapbox::getPtr<const std::string>(resp);
                    if (state == nullptr)
                    {
                        return;
                    }
                    asyncResp->res.jsonValue[service]["ProtocolEnabled"] =
                        *state == "active";
                },
                "org.freedesktop.systemd1", socketPath,
                "org.freedesktop.DBus.Properties", "Get",
                "org.freedesktop.systemd1.Unit", "ActiveState");
        }
    }

    void changeProtocolInfo(const std::shared_ptr<AsyncResp>& asyncResp,
                            const std::string& protocol,
                            const nlohmann::json& input)
    {
        if (!input.is_object())
        {
            messages::addMessageToJson(
                asyncResp->res.jsonValue,
                messages::propertyValueTypeError(input.dump(), protocol),
                "/" + protocol);
            return;
        }

        const auto& conf = protocolToDBus.at(protocol);
        for (const auto& item : input.items())
        {
            if (item.key() == "ProtocolEnabled")
            {
                const bool* protocolEnabled =
                    item.value().get_ptr<const bool*>();

                if (protocolEnabled == nullptr)
                {
                    messages::addMessageToErrorJson(
                        asyncResp->res.jsonValue,
                        messages::propertyValueFormatError(item.value().dump(),
                                                           "ProtocolEnabled"));
                    return;
                }

                std::string action = "Start";
                if (*protocolEnabled == false)
                {
                    action = "Stop";
                }
                crow::connections::systemBus->async_method_call(
                    [asyncResp, &protocol](
                        const boost::system::error_code ec,
                        const sdbusplus::message::object_path objectPath) {
                        if (ec)
                        {
                            messages::addMessageToJson(
                                asyncResp->res.jsonValue,
                                messages::internalError(), "/" + protocol);
                            return;
                        }
                        messages::addMessageToJson(asyncResp->res.jsonValue,
                                                   messages::success(),
                                                   "/" + protocol);
                    },
                    "org.freedesktop.systemd1", conf.socketPath,
                    "org.freedesktop.systemd1.Unit", action, "replace");
                if (protocol != "SSH")
                {
                    crow::connections::systemBus->async_method_call(
                        [asyncResp, &protocol](
                            const boost::system::error_code ec,
                            const sdbusplus::message::object_path objectPath) {
                            if (ec)
                            {
                                messages::addMessageToJson(
                                    asyncResp->res.jsonValue,
                                    messages::internalError(), "/" + protocol);
                                return;
                            }
                        },
                        "org.freedesktop.systemd1", conf.servicePath,
                        "org.freedesktop.systemd1.Unit", action, "replace");
                }
            }
            else if (item.key() == "Port")
            {
                const int64_t* portNumber =
                    item.value().get_ptr<const int64_t*>();

                if (portNumber == nullptr)
                {
                    messages::addMessageToErrorJson(
                        asyncResp->res.jsonValue,
                        messages::propertyValueFormatError(item.value().dump(),
                                                           "Port"));
                    return;
                }

                std::string socketFile = protocolToDBus.at(protocol).socketFile;
                std::string confPath = "/lib/systemd/system/" + socketFile;
                std::fstream stream(confPath.c_str(), std::fstream::in);
                if (!stream.is_open())
                {
                    messages::addMessageToJson(asyncResp->res.jsonValue,
                                               messages::internalError(),
                                               "/" + protocol);
                    return;
                }

                // read old config
                std::vector<std::string> config;
                std::string line;
                while (std::getline(stream, line))
                {
                    if (boost::beast::string_view(line.c_str(), 6) == "Listen")
                    {
                        std::string listen = line.substr(0, line.find("=") + 1);
                        line = listen + std::to_string(*portNumber);
                    }
                    config.emplace_back(line);
                }
                stream.close();

                // rewrite config
                stream.open(confPath.c_str(), std::fstream::out);
                if (!stream.is_open())
                {
                    messages::addMessageToJson(asyncResp->res.jsonValue,
                                               messages::internalError(),
                                               "/" + protocol);
                    return;
                }
                for (std::string i : config)
                {
                    stream << i << "\n";
                }
                stream.close();

                // stop and reload services
                if (protocol != "SSH")
                {
                    // no async here, since we need it stopped before reloading
                    auto m = crow::connections::systemBus->new_method_call(
                        "org.freedesktop.systemd1", conf.servicePath,
                        "org.freedesktop.systemd1.Unit", "Stop");
                    m.append("replace");
                    crow::connections::systemBus->call(m);
                }
                // crow::connections::systemBus->async_method_call(
                //     [asyncResp, &protocol](
                //         const boost::system::error_code ec,
                //         const sdbusplus::message::object_path objectPath) {
                //         if (ec)
                //         {
                //             messages::addMessageToJson(
                //                 asyncResp->res.jsonValue,
                //                 messages::internalError(), "/" + protocol);
                //             return;
                //         }
                //     },
                //     "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                //     "org.freedesktop.systemd1.Manager", "Reload");
            }
            else
            {
                messages::addMessageToErrorJson(
                    asyncResp->res.jsonValue,
                    messages::propertyNotWritable(item.key()));
                asyncResp->res.result(boost::beast::http::status::bad_request);
                return;
            }
        }
    }
};

} // namespace redfish
