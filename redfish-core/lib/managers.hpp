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

#include "node.hpp"
#include "utils/ampere-utils.hpp"

namespace redfish
{

/**
 * DBus types primitives for several generic DBus interfaces
 * TODO consider move this to separate file into boost::dbus
 */
using PropertiesMapType = boost::container::flat_map<
    std::string,
    sdbusplus::message::variant<std::string, bool, uint8_t, int16_t, uint16_t,
                                int32_t, uint32_t, int64_t, uint64_t, double>>;

using GetManagedObjectsType = boost::container::flat_map<
    sdbusplus::message::object_path,
    boost::container::flat_map<std::string, PropertiesMapType>>;

using GetAllPropertiesType = PropertiesMapType;

/**
 * ManagerActionsReset class supports handle POST method for Reset action.
 * The class retrieves and sends data directly to dbus.
 */
class ManagerActionsReset : public Node
{
  public:
    ManagerActionsReset(CrowApp &app) :
        Node(app, "/redfish/v1/Managers/bmc/Actions/Manager.Reset/")
    {
        entityPrivileges = {
            {boost::beast::http::verb::post, {{"ConfigureComponents"}}}};
    }

  private:
    /**
     * Function handles POST method request.
     * Analyzes POST body message before sends Reset request data to dbus.
     * OpenBMC allows for ResetType is GracefulRestart only.
     */
    void doPost(crow::Response &res, const crow::Request &req,
                const std::vector<std::string> &params) override
    {
        // Parse request data to JSON object
        auto request_json_obj = nlohmann::json::parse(req.body, nullptr, false);
        if (request_json_obj.is_discarded())
        {
            res.result(boost::beast::http::status::bad_request);
            return;
        }

        // Within the Reset action, the POST request should be only 1 JSON
        // element.
        if (request_json_obj.size() == 1)
        {
            nlohmann::json::iterator it = request_json_obj.begin();
            // Extract data
            std::string data = it.key();
            if (data == "ResetType")
            {
                // Extract property value before invoke the corresponding
                // method.
                const std::string &property_value =
                    it->get<const std::string>();
                // OpenBMC only allows for GracefulRestart property.
                if (property_value == "GracefulRestart")
                {
                    doBMCGracefulRestart(res, req, params);
                }
                else
                {
                    res.result(boost::beast::http::status::bad_request);
                    return;
                }
            }
            else
            {
                res.result(boost::beast::http::status::bad_request);
                return;
            }
        }
        else
        {
            res.result(boost::beast::http::status::bad_request);
            return;
        }
    }

    /**
     * Function transceives data with dbus directly.
     * All BMC state properties will be retrieved before sending reset request.
     */
    void doBMCGracefulRestart(crow::Response &res, const crow::Request &req,
                              const std::vector<std::string> &params)
    {
        auto asyncResp = std::make_shared<AsyncResp>(res);
        // Create the D-Bus variant for D-Bus call.
        crow::connections::systemBus->async_method_call(
            [asyncResp](const boost::system::error_code ec,
                        const PropertiesMapType &properties) {
                if (ec)
                {
                    asyncResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }
                else
                {
                    auto it = properties.find("RequestedBMCTransition");
                    if (it != properties.end())
                    {
                        crow::connections::systemBus->async_method_call(
                            [asyncResp](const boost::system::error_code ec) {
                                // Use "Set" method to set the property value.
                                if (ec)
                                {
                                    asyncResp->res.result(
                                        boost::beast::http::status::
                                            internal_server_error);
                                    return;
                                }
                            },
                            "xyz.openbmc_project.State.BMC",
                            "/xyz/openbmc_project/state/bmc0",
                            "org.freedesktop.DBus.Properties", "Set",
                            "xyz.openbmc_project.State.BMC",
                            "RequestedBMCTransition",
                            sdbusplus::message::variant<std::string>{
                                "xyz.openbmc_project.State.BMC."
                                "Transition.Reboot"});
                    }
                    else
                    {
                        asyncResp->res.result(
                            boost::beast::http::status::not_found);
                        return;
                    }
                }
            },
            "xyz.openbmc_project.State.BMC", "/xyz/openbmc_project/state/bmc0",
            "org.freedesktop.DBus.Properties", "GetAll",
            "xyz.openbmc_project.State.BMC");
    }
};

class Manager : public Node
{
  public:
    Manager(CrowApp &app) :
        Node(app, "/redfish/v1/Managers/bmc/"), memberActionsReset(app)
    {
        Node::json["@odata.id"] = "/redfish/v1/Managers/bmc";
        Node::json["@odata.type"] = "#Manager.v1_3_0.Manager";
        Node::json["@odata.context"] = "/redfish/v1/$metadata#Manager.Manager";
        Node::json["Id"] = "bmc";
        Node::json["Name"] = "OpenBmc Manager";
        Node::json["Description"] = "Baseboard Management Controller";
        Node::json["PowerState"] = "On";
        Node::json["ManagerType"] = "BMC";
        Node::json["UUID"] =
            app.template getMiddleware<crow::persistent_data::Middleware>()
                .systemUuid;
        Node::json["Model"] = "OpenBmc"; // TODO(ed), get model
        Node::json["NetworkProtocol"] = nlohmann::json(
            {{"@odata.id", "/redfish/v1/Managers/bmc/NetworkProtocol"}});
        Node::json["EthernetInterfaces"] = nlohmann::json(
            {{"@odata.id", "/redfish/v1/Managers/bmc/EthernetInterfaces"}});
        Node::json["Links"]["ManagerForServers"] = {
            {{"@odata.id", "/redfish/v1/Systems/1"}}};
        Node::json["Links"]["ManagerForChassis"] = {
            {{"@odata.id", "/redfish/v1/Chassis/1"}}};
        Node::json["Links"]["ManagerInChassis"] = {
            {"@odata.id", "/redfish/v1/Chassis/1"}};
        Node::json["Actions"]["#Manager.Reset"] = {
            {"target", "/redfish/v1/Managers/bmc/Actions/Manager.Reset"},
            {"ResetType@Redfish.AllowableValues", {"GracefulRestart"}}};

        entityPrivileges = {
            {boost::beast::http::verb::get, {{"Login"}}},
            {boost::beast::http::verb::head, {{"Login"}}},
            {boost::beast::http::verb::patch, {{"ConfigureManager"}}},
            {boost::beast::http::verb::put, {{"ConfigureManager"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureManager"}}},
            {boost::beast::http::verb::post, {{"ConfigureManager"}}}};
    }

  private:
    void doGet(crow::Response &res, const crow::Request &req,
               const std::vector<std::string> &params) override
    {
        res.jsonValue = Node::json;
        auto asyncResp = std::make_shared<AsyncResp>(res);

        BMCWEB_LOG_DEBUG << "Get BMC Firmware Version enter.";
        crow::connections::systemBus->async_method_call(
            [asyncResp](const boost::system::error_code ec,
                        const PropertiesMapType &properties) {
                if (ec)
                {
                    BMCWEB_LOG_ERROR << "D-Bus response error " << ec;
                    asyncResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }
                PropertiesMapType::const_iterator it;
                // Get the major version
                uint16_t majorVersion = 0;
                uint16_t minorVersion = 0;
                std::string patchVersion{};
                it = properties.find("MajorVersion");
                if (it != properties.end())
                {
                    const uint16_t *s =
                        mapbox::getPtr<const uint16_t>(it->second);
                    if (nullptr != s)
                    {
                        majorVersion = *s;
                    }
                }
                // Get the minor version
                it = properties.find("MinorVersion");
                if (it != properties.end())
                {
                    const uint16_t *s =
                        mapbox::getPtr<const uint16_t>(it->second);
                    if (nullptr != s)
                    {
                        minorVersion = *s;
                    }
                }
                // Get the patch version
                it = properties.find("PatchVersion");
                // Get the major version
                if (it != properties.end())
                {
                    const uint32_t *s =
                        mapbox::getPtr<const uint32_t>(it->second);
                    // Get the major version
                    if (nullptr != s)
                    {
                        char buff[6] = {};
                        uint16_t major = *s / (256 * 256);
                        uint16_t minor = (*s - major * 256 * 256) / 256;
                        uint16_t build = (*s - major * 256 * 256 - minor * 256);
                        patchVersion = std::to_string(major) + "." +
                                       std::to_string(minor) + "." +
                                       std::to_string(build);
                    }
                }
                // Format of Firmware Version will be MM.NN-X.Y.Z
                std::string fwVersion = std::to_string(majorVersion) + "." +
                                        std::to_string(minorVersion) + "-" +
                                        patchVersion;

                // Update JSON payload with Bios Version information.
                asyncResp->res.jsonValue["FirmwareVersion"] = fwVersion;
            },
            "xyz.openbmc_project.Inventory.BMC.Manager",
            "/xyz/openbmc_project/inventory/bmc/version",
            "org.freedesktop.DBus.Properties", "GetAll",
            "xyz.openbmc_project.Inventory.Item.Bmc");

        BMCWEB_LOG_DEBUG << "Get  status enter.";
        res.jsonValue["CommandShell"] = {
            {"ConnectTypesSupported", nlohmann::json::array()},
            {"MaxConcurrentSessions", 64}, // TODO(Hy): Retrieve real data
            {"ServiceEnabled", true} // true only when all protocols are enabled
        };
        crow::connections::systemBus->async_method_call(
            [asyncResp](const boost::system::error_code ec,
                        const sdbusplus::message::variant<std::string> &resp) {
                if (ec)
                {
                    messages::addMessageToJson(asyncResp->res.jsonValue,
                                               messages::internalError(),
                                               "/CommandShell/SSH");
                    return;
                }

                const std::string *state =
                    mapbox::getPtr<const std::string>(resp);
                if (state == nullptr)
                {
                    messages::addMessageToJson(asyncResp->res.jsonValue,
                                               messages::internalError(),
                                               "/CommandShell/SSH");
                    return;
                }

                auto &commandShell = asyncResp->res.jsonValue["CommandShell"];
                commandShell["ConnectTypesSupported"].emplace_back("SSH");
                if (*state != "active")
                {
                    commandShell["ServiceEnabled"] = false;
                }
            },
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1/unit/dropbear_2esocket",
            "org.freedesktop.DBus.Properties", "Get",
            "org.freedesktop.systemd1.Unit", "ActiveState");

        std::string redfishDateTime = getCurrentDateTime("%FT%T%z");
        // insert the colon required by the ISO 8601 standard
        redfishDateTime.insert(redfishDateTime.end() - 2, ':');
        asyncResp->res.jsonValue["DateTime"] = redfishDateTime;
        asyncResp->res.jsonValue["DateTimeLocalOffset"] =
            redfishDateTime.substr(redfishDateTime.length() - 6);
    }

    // Actions Reset object as a member of Manager resource.
    // Handle reset action from POST request.
    ManagerActionsReset memberActionsReset;
};

class ManagerCollection : public Node
{
  public:
    ManagerCollection(CrowApp &app) : Node(app, "/redfish/v1/Managers/")
    {
        Node::json["@odata.id"] = "/redfish/v1/Managers";
        Node::json["@odata.type"] = "#ManagerCollection.ManagerCollection";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#ManagerCollection.ManagerCollection";
        Node::json["Name"] = "Manager Collection";
        Node::json["Members@odata.count"] = 1;
        Node::json["Members"] = {{{"@odata.id", "/redfish/v1/Managers/bmc"}}};

        entityPrivileges = {
            {boost::beast::http::verb::get, {{"Login"}}},
            {boost::beast::http::verb::head, {{"Login"}}},
            {boost::beast::http::verb::patch, {{"ConfigureManager"}}},
            {boost::beast::http::verb::put, {{"ConfigureManager"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureManager"}}},
            {boost::beast::http::verb::post, {{"ConfigureManager"}}}};
    }

  private:
    void doGet(crow::Response &res, const crow::Request &req,
               const std::vector<std::string> &params) override
    {
        // Collections don't include the static data added by SubRoute because
        // it has a duplicate entry for members
        res.jsonValue["@odata.id"] = "/redfish/v1/Managers";
        res.jsonValue["@odata.type"] = "#ManagerCollection.ManagerCollection";
        res.jsonValue["@odata.context"] =
            "/redfish/v1/$metadata#ManagerCollection.ManagerCollection";
        res.jsonValue["Name"] = "Manager Collection";
        res.jsonValue["Members@odata.count"] = 1;
        res.jsonValue["Members"] = {
            {{"@odata.id", "/redfish/v1/Managers/bmc"}}};
        res.end();
    }
};

} // namespace redfish
