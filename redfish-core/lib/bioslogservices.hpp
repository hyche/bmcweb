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
 * BIOSLogEntry derived class for delivering Log Entry Schema.
 */
class BIOSLogEntry : public Node
{
  public:
    template <typename CrowApp>
    BIOSLogEntry(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/LogServices/BIOS/Entries/<str>",
             std::string())
    {
        Node::json["@odata.type"] = "#LogEntry.v1_3_0.LogEntry";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#LogEntry.LogEntry";
        Node::json["EntryType"] = "BIOS Event Log"; // BIOS Event Log

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
        // Check if there is required param, truly entering this shall be
        // impossible
        if (params.size() != 1)
        {
            res.result(boost::beast::http::status::internal_server_error);
            res.end();
            return;
        }

        const std::string &entryId = params[0];
        res.jsonValue = Node::json;
        res.jsonValue["@odata.id"] =
            "/redfish/v1/Systems/1/LogServices/BIOS/Entries/" + entryId;
        auto asyncResp = std::make_shared<AsyncResp>(res);
        crow::connections::systemBus->async_method_call(
            [asyncResp, entryId](const boost::system::error_code ec,
                                 const GetManagedObjectsType &resp) {
                if (ec)
                {
                    asyncResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }

                for (auto &objectPath : resp)
                {
                    for (auto &interfaceMap : objectPath.second)
                    {
                        if (interfaceMap.first !=
                            "xyz.openbmc_project.Inventory.Item.BiosLogEntry")
                        {
                            BMCWEB_LOG_DEBUG << "Bailing early on "
                                             << interfaceMap.first;
                            continue;
                        }
                        for (auto &propertyMap : interfaceMap.second)
                        {
                            if (propertyMap.first == "Id")
                            {
                                const uint16_t *id =
                                    mapbox::getPtr<const uint16_t>(
                                        propertyMap.second);
                                // only assign properties if the id is matched
                                if (id == nullptr ||
                                    entryId != std::to_string(*id))
                                {
                                    break;
                                }
                            }
                            const std::string *s =
                                mapbox::getPtr<const std::string>(
                                    propertyMap.second);
                            if (s != nullptr)
                            {
                                asyncResp->res.jsonValue[propertyMap.first] =
                                    *s;
                            }
                        }
                    }
                }
            },
            "xyz.openbmc_project.Inventory.Host.Manager",
            "/xyz/openbmc_project/inventory/host",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    }
};

/**
 * BIOSLogEntryCollection derived class for delivering Log Entry Collection
 * Schema
 */
class BIOSLogEntryCollection : public Node
{
  public:
    template <typename CrowApp>
    BIOSLogEntryCollection(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/LogServices/BIOS/Entries")
    {
        Node::json["@odata.type"] = "#LogEntryCollection.LogEntryCollection";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#LogEntryCollection.LogEntryCollection";
        Node::json["@odata.id"] =
            "/redfish/v1/Systems/1/LogServices/BIOS/Entries";
        Node::json["Description"] = "Collection of BIOS Logs for this System";
        Node::json["Name"] = "BIOS Log Entry Collection";

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
        crow::connections::systemBus->async_method_call(
            [asyncResp](const boost::system::error_code ec,
                        GetManagedObjectsType &resp) {
                if (ec)
                {
                    asyncResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }

                nlohmann::json &entries = asyncResp->res.jsonValue["Members"];
                entries = nlohmann::json::array();
                for (auto &objPath : resp)
                {
                    for (auto &interface : objPath.second)
                    {
                        if (interface.first ==
                            "xyz.openbmc_project.Inventory.Item.BiosLogEntry")
                        {
                            const std::string &id = objPath.first;
                            // Cut out everything until last "/", and put it
                            // into output vector
                            entries.push_back(
                                {{"@odata.id",
                                  "/redfish/v1/Systems/1/LogServices/BIOS/"
                                  "Entries/" +
                                      id.substr(id.rfind("/") + 1)}});
                        }
                    }
                }
                asyncResp->res.jsonValue["Member@odata.count"] = entries.size();
            },
            "xyz.openbmc_project.Inventory.Host.Manager",
            "/xyz/openbmc_project/inventory/host",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    }
};

/**
 * BIOSLogServiceActionsClear class supports POST method for ClearLog action.
 */
class BIOSLogServiceActionsClear : public Node
{
  public:
    BIOSLogServiceActionsClear(CrowApp &app) :
        Node(app,
             "/redfish/v1/Systems/1/LogServices/BIOS/Actions/LogService.Reset")
    {
        entityPrivileges = {
            {boost::beast::http::verb::post, {{"ConfigureManager"}}}};
    }

  private:
    /**
     * Function handles POST method request.
     * The Clear Log actions does not require any parameter.The action deletes
     * all entries found in the Entries collection for this Log Service.
     */
    void doPost(crow::Response &res, const crow::Request &req,
                const std::vector<std::string> &params) override
    {
        BMCWEB_LOG_DEBUG << "Delete all entries.";
        // TODO (Hoang): support delete bios log entry
    }
};

/**
 * BIOSLogService derived class for delivering Log Service Schema.
 */
class BIOSLogService : public Node
{
  public:
    template <typename CrowApp>
    BIOSLogService(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/LogServices/BIOS/")
    {
        Node::json["@odata.type"] = "#LogService.v1_1_0.LogService";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#LogService.LogService";
        Node::json["Name"] = "System BIOS Log Service";
        Node::json["Id"] = "BIOS Log Service";
        Node::json["Entries"] = {
            {"@odata.id", "/redfish/v1/Systems/1/LogServices/BIOS/Entries"}};

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
        // Get Log Service name
        Node::json["@odata.id"] = "/redfish/v1/Systems/1/LogServices/BIOS";

        // TODO Logging service has not supported get MaxNumberOfRecords
        // property yet hardcode to ERROR_CAP (200) from phosphor-logging.
        Node::json["MaxNumberOfRecords"] = 200;
        Node::json["OverWritePolicy"] = "WrapsWhenFull";
        std::string redfishDateTime = getCurrentDateTime("%FT%T%z");
        // insert the colon required by the ISO 8601 standard
        redfishDateTime.insert(redfishDateTime.end() - 2, ':');
        Node::json["DateTime"] = redfishDateTime;
        Node::json["DateTimeLocalOffset"] =
            redfishDateTime.substr(redfishDateTime.length() - 6);
        // TODO hardcoded ServiceEnabled property to true
        Node::json["ServiceEnabled"] = true;
        // TODO hardcoded Status information
        Node::json["Status"]["State"] = "Enabled";
        Node::json["Status"]["Health"] = "OK";

        // Supported Actions
        nlohmann::json clearLog;
        clearLog["target"] =
            "/redfish/v1/Systems/1/LogServices/BIOS/Actions/LogService.Reset";
        Node::json["Actions"]["#LogService.ClearLog"] = clearLog;

        res.jsonValue = Node::json;
        res.end();
    }
};

} // namespace redfish
