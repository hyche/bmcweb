/*
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

#include <boost/container/flat_map.hpp>

namespace redfish
{

using GetManagedObjectsType = boost::container::flat_map<
    sdbusplus::message::object_path,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<
            std::string, sdbusplus::message::variant<
                             std::string, bool, uint8_t, int16_t, uint16_t,
                             int32_t, uint32_t, int64_t, uint64_t, double>>>>;

/**
 * @brief Retrieves simple storage devices over dbus
 *
 * @param[in] aResp     Shared pointer for completing asynchronous calls.
 *
 * @return None.
 */
void getSimpleStorageDevices(std::shared_ptr<AsyncResp> aResp)
{
    BMCWEB_LOG_DEBUG << "Get simple storage device information.";
    crow::connections::systemBus->async_method_call(
        [aResp](const boost::system::error_code ec,
                const GetManagedObjectsType &resp) {
            if (ec)
            {
                aResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }
            aResp->res.jsonValue["Devices"] = nlohmann::json::array();

            for (auto &objectPath : resp)
            {
                for (auto &interfaceMap : objectPath.second)
                {
                    if (interfaceMap.first !=
                        "xyz.openbmc_project.Inventory.Item.Storage")
                    {
                        BMCWEB_LOG_DEBUG << "Bailing early on "
                                         << interfaceMap.first;
                        continue;
                    }
                    nlohmann::json jsonSimpleStorage;
                    bool found = false;
                    for (auto &propertyMap : interfaceMap.second)
                    {
                        if (propertyMap.first == "CapacityBytes")
                        {
                            const uint32_t *s = mapbox::getPtr<const uint32_t>(
                                propertyMap.second);
                            if (s != nullptr)
                            {
                                BMCWEB_LOG_DEBUG << "-> Value " << *s;
                                found = true;
                                jsonSimpleStorage[propertyMap.first] = *s;
                            }
                        }
                        else
                        {
                            const std::string *s =
                                mapbox::getPtr<const std::string>(
                                    propertyMap.second);
                            if (s != nullptr)
                            {
                                BMCWEB_LOG_DEBUG << "-> Value " << *s;
                                found = true;
                                jsonSimpleStorage[propertyMap.first] = *s;
                            }
                        }
                    }

                    if (found != false)
                    {
                        //TODO (Hoang): need to support Healh info for each
                        //              simple storage device
                        jsonSimpleStorage["Status"]["State"] = "Enabled";

                        aResp->res.jsonValue["Devices"].push_back(
                            jsonSimpleStorage);
                    }
                }
            }
        },
        "xyz.openbmc_project.Inventory.Host.Manager",
        "/xyz/openbmc_project/inventory/host",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
}

/**
 * SimpleStorage derived class for delivering Simple Storage Schema.
 */
class SimpleStorage : public Node
{
  public:
    template <typename CrowApp>
    SimpleStorage(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/SimpleStorage/1")
    {
        Node::json["@odata.type"] = "#SimpleStorage.v1_2_0.SimpleStorage";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#SimpleStorage.SimpleStorage";
        Node::json["@odata.id"] = "/redfish/v1/Systems/1/SimpleStorage/1";
        Node::json["Name"] = "Simple Storage Controller";
        Node::json["Description"] = "System SATA";
        Node::json["Id"] = "1";

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
        res.jsonValue = Node::json;
        auto asyncResp = std::make_shared<AsyncResp>(res);
        getSimpleStorageDevices(asyncResp);
    }
};

/**
 * SimpleStorageCollection derived class for delivering Simple Storage
 * Collection Schema.
 */
class SimpleStorageCollection : public Node
{
  public:
    template <typename CrowApp>
    SimpleStorageCollection(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/SimpleStorage")
    {
        Node::json["@odata.type"] =
            "#SimpleStorageCollection.SimpleStorageCollection";
        Node::json["@odata.context"] = "/redfish/v1/"
                                       "$metadata#SimpleStorageCollection."
                                       "SimpleStorageCollection";
        Node::json["Name"] = "Simple Storage Collection";
        Node::json["Members"] = {{
            {"@odata.id", "/redfish/v1/Systems/1/SimpleStorage/1"}}};
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
    /**
     * Functions triggers appropriate requests on D-Bus
     */
    void doGet(crow::Response &res, const crow::Request &req,
               const std::vector<std::string> &params) override
    {
        res.jsonValue = Node::json;
        res.jsonValue["@odata.id"] = "/redfish/v1/Systems/1/SimpleStorage";
        res.end();
    }
};

} // namespace redfish
