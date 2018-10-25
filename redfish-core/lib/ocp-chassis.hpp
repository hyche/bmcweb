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

/*
// The OCP chassis module supports to retrieve chassis inventory via D-Bus.
// It's used to replace the redfish-core/lib/chassis.hpp module
// (https://github.com/openbmc/bmcweb).
// Module is implemented in difference way from chassis.hpp which uses
// Entity-Manager service (https://github.com/openbmc/entity-manager).
*/

#pragma once

#include "node.hpp"

#include <boost/container/flat_map.hpp>

namespace redfish
{

/**
 * D-Bus types primitives for several generic D-Bus interfaces
 * TODO consider move this to separate file into boost::dbus
 */
using VariantType = sdbusplus::message::variant<
                                          std::string, bool, uint8_t, int16_t,
                                          uint16_t, int32_t, uint32_t, int64_t,
                                          uint64_t, double>;
using ManagedObjectsType = std::vector<std::pair<
    sdbusplus::message::object_path,
    std::vector<std::pair<
        std::string, std::vector<std::pair<std::string, VariantType>>>>>>;
using PropertiesType = boost::container::flat_map<std::string, VariantType>;

/**
 * OnDemandChassisProvider
 * Chassis provider class that retrieves data directly from D-Bus, before seting
 * it into JSON output. This does not cache any data.
 *
 * Class can be a good example on how to scale different data providing
 * solutions to produce single schema output.
 *
 * TODO
 * This perhaps shall be different file, which has to be chosen on compile time
 * depending on OEM needs
 */
class OnDemandChassisProvider
{
  public:
    /**
     * Function that retrieves all properties for given Chassis Object.
     * @param[in] aResp     Shared pointer for completing asynchronous calls.
     * @return None.
     */
    void get_chassis_data(const std::shared_ptr<AsyncResp> aResp)
    {
        crow::connections::systemBus->async_method_call(
            [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                      const PropertiesType &properties) {
                // Callback requires flat_map<string, string> so prepare one.
                boost::container::flat_map<std::string, std::string> output;
                if (ec)
                {
                    aResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }
                // Prepare all the schema required fields which retrieved from
                // D-Bus.
                for (const std::string p : std::array<const char *, 4>{
                         "Asset_Tag", "Part_Number", "Serial_Number", "SKU"})
                {
                    PropertiesType::const_iterator it = properties.find(p);
                    if (it != properties.end())
                    {
                        const std::string *s =
                            mapbox::getPtr<const std::string>(it->second);
                        if (s != nullptr)
                        {
                            if (p == "Asset_Tag")
                            {
                                aResp->res.jsonValue["AssetTag"] = *s;
                            }
                            else if (p == "Part_Number")
                            {
                                aResp->res.jsonValue["PartNumber"] = *s;
                            }
                            else if (p == "Serial_Number")
                            {
                                aResp->res.jsonValue["SerialNumber"] = *s;
                            }
                            else
                            {
                                aResp->res.jsonValue[p] = *s;
                            }
                        }
                    }
                }
            },
            "xyz.openbmc_project.Inventory.FRU",
            "/xyz/openbmc_project/inventory/fru0/chassis",
            "org.freedesktop.DBus.Properties", "GetAll",
            "xyz.openbmc_project.Inventory.FRU.Chassis");
    }

    /**
     * Function that retrieves some properties for given Chassis Object.
     * @param[in] aResp     Shared pointer for completing asynchronous calls.
     * @return None.
     * TODO: These info is not available from Chassis Area
     *       So we need to use the info from Product Area.
     *       Update for Chassis Area after this info is available
     */
    void get_chassis_data_from_product(const std::shared_ptr<AsyncResp> aResp)
    {
        crow::connections::systemBus->async_method_call(
            [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                      const PropertiesType &properties) {
                // Callback requires flat_map<string, string> so prepare one.
                boost::container::flat_map<std::string, std::string> output;
                if (ec)
                {
                    aResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }
                // Prepare all the schema required fields which retrieved from
                // D-Bus.
                for (const std::string p : std::array<const char *, 2>{
                         "Manufacturer", "Name"})
                {
                    PropertiesType::const_iterator it = properties.find(p);
                    if (it != properties.end())
                    {
                        const std::string *s =
                            mapbox::getPtr<const std::string>(it->second);
                        if (s != nullptr)
                        {
                            if (p == "Manufacturer")
                            {
                                aResp->res.jsonValue["Manufacturer"] = *s;
                            }
                            if (p == "Name")
                            {
                                aResp->res.jsonValue["Model"] = *s;
                            }
                       }
                    }
                }
            },
            "xyz.openbmc_project.Inventory.FRU",
            "/xyz/openbmc_project/inventory/fru0/product",
            "org.freedesktop.DBus.Properties", "GetAll",
            "xyz.openbmc_project.Inventory.FRU.Product");
    }

    /**
     * @brief Retrieves chassis state properties over D-Bus
     *
     * @param[in] aResp     Shared pointer for completing asynchronous calls.
     * @return None.
     */
    void get_chassis_state(const std::shared_ptr<AsyncResp> aResp)
    {
        BMCWEB_LOG_DEBUG << "Get Chassis information.";
        crow::connections::systemBus->async_method_call(
            [aResp{std::move(aResp)}](const boost::system::error_code ec,
                                      const PropertiesType &properties) {
                if (ec)
                {
                    aResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    BMCWEB_LOG_ERROR << "D-Bus response error " << ec;
                    return;
                }
                BMCWEB_LOG_DEBUG << "Got " << properties.size()
                                 << " chassis properties.";
                PropertiesType::const_iterator it =
                    properties.find("CurrentPowerState");
                if (it != properties.end())
                {
                    const std::string *s =
                        mapbox::getPtr<const std::string>(it->second);
                    if (s != nullptr)
                    {
                        const std::size_t pos = s->rfind('.');
                        if (pos != std::string::npos)
                        {
                            // Verify Chassis state
                            if (s->substr(pos + 1) == "On")
                            {
                                aResp->res.jsonValue["PowerState"] = "On";
                                aResp->res.jsonValue["Status"]["State"] =
                                    "Enabled";
                            }
                            else
                            {
                                aResp->res.jsonValue["PowerState"] = "Off";
                                aResp->res.jsonValue["Status"]["State"] =
                                    "Disabled";
                            }
                        }
                    }
                }
                it = properties.find("HealthState");
                if (it != properties.end())
                {
                    const std::string *s =
                        mapbox::getPtr<const std::string>(it->second);
                    if (s != nullptr)
                    {
                        // Retrieve the Health value from:
                        // xyz.openbmc_project.State.Chassis.Health.<state>
                        const std::size_t pos = s->rfind('.');
                        if (pos != std::string::npos)
                        {
                            aResp->res.jsonValue["Status"]["Health"] =
                                s->substr(pos + 1);
                        }
                    }
                }
            },
            "xyz.openbmc_project.State.Chassis",
            "/xyz/openbmc_project/state/chassis0",
            "org.freedesktop.DBus.Properties", "GetAll",
            "xyz.openbmc_project.State.Chassis");
    }
};

/**
 * Chassis override class for delivering Chassis Schema
 */
class Chassis : public Node
{
  public:
    /*
     * Default Constructor
     */
    template <typename CrowApp>
    Chassis(CrowApp &app) :
        Node(app, "/redfish/v1/Chassis/<str>/", std::string())
    {
        Node::json["@odata.type"] = "#Chassis.v1_4_0.Chassis";
        Node::json["@odata.context"] = "/redfish/v1/$metadata#Chassis.Chassis";
        Node::json["Name"] =
            "Ampere System Chassis"; // TODO hardcode in temporary.
        // TODO Implements mapping from fru to redfish
        Node::json["ChassisType"] = "RackMount";
        Node::json["Id"] = "1";
        // TODO Initial State for chassis
        Node::json["PowerState"] = "Off";
        Node::json["Status"]["State"] = "Disabled";
        Node::json["Status"]["Health"] =
            "OK"; // Resource can response so assume
                  // default Health state is Ok.

        entityPrivileges = {
            {boost::beast::http::verb::get, {{"Login"}}},
            {boost::beast::http::verb::head, {{"Login"}}},
            {boost::beast::http::verb::patch, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::put, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::post, {{"ConfigureComponents"}}}};
    }

  private:
    /**
     * Functions triggers appropriate requests on D-Bus
     */
    void doGet(crow::Response &res, const crow::Request &req,
               const std::vector<std::string> &params) override
    {
        const std::string &chassisId = params[0];
        // TODO(HY): Remove ocp-chassis in the future since it causes lots of
        //           mess.
        if (chassisId != "1")
        {
            res.result(boost::beast::http::status::not_found);
            res.end();
            return;
        }

        Node::json["@odata.id"] = "/redfish/v1/Chassis/1";
        Node::json["Thermal"] = {
            {"@odata.id", "/redfish/v1/Chassis/1/Thermal"}};
        Node::json["Power"] = {{"@odata.id", "/redfish/v1/Chassis/1/Power"}};
        Node::json["Links"]["ComputerSystems"] = {
            {{"@odata.id", "/redfish/v1/Systems/1"}}};
        Node::json["Links"]["ManagedBy"] = {
            {{"@odata.id", "/redfish/v1/Managers/bmc"}}};

        auto asyncResp = std::make_shared<AsyncResp>(res);
        asyncResp->res.jsonValue = Node::json;

        // Get chassis information:
        //        Name,
        //        SerialNumber,
        //        PartNumber,
        chassis_provider.get_chassis_data(asyncResp);

        // Get chassis information:
        //        Manufacturer,
        //        Model
        // TODO: Because this info is not available from Chassis Area
        //       So get follow chassis information from Product FRU Area
        //       Update the solution after Chassis Area supports this info
        chassis_provider.get_chassis_data_from_product(asyncResp);

        // Get chassis state:
        //        PowerState,
        //        Status:
        //          State,
        //          Health
        chassis_provider.get_chassis_state(asyncResp);
    }

    // Chassis Provider object
    // TODO consider move it to singleton
    OnDemandChassisProvider chassis_provider;
};

/**
 * ChassisCollection derived class for delivering Chassis Collection Schema
 */
class ChassisCollection : public Node
{
  public:
    template <typename CrowApp>
    ChassisCollection(CrowApp &app) : Node(app, "/redfish/v1/Chassis/")
    {
        Node::json["@odata.type"] = "#ChassisCollection.ChassisCollection";
        Node::json["@odata.id"] = "/redfish/v1/Chassis";
        Node::json["@odata.context"] =
             "/redfish/v1/$metadata#ChassisCollection.ChassisCollection";
        Node::json["Name"] = "Chassis Collection";
        // System only has 1 chassis, there is only 1 appropriate link
        // Then attach members, count size and return.
        // TODO hardcode the member id with "1"
        Node::json["Members"] = {{{"@odata.id", "/redfish/v1/Chassis/1"}}};
        Node::json["Members@odata.count"] = 1;

        entityPrivileges = {
            {boost::beast::http::verb::get, {{"Login"}}},
            {boost::beast::http::verb::head, {{"Login"}}},
            {boost::beast::http::verb::patch, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::put, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::post, {{"ConfigureComponents"}}}};
    }

  private:
    void doGet(crow::Response &res, const crow::Request &req,
               const std::vector<std::string> &params) override
    {
        res.jsonValue = Node::json;
        res.end();
    }
};
} // namespace redfish
