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

#include <boost/container/flat_map.hpp>

namespace redfish
{

using GetManagedObjectsTypes = boost::container::flat_map<
    sdbusplus::message::object_path,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<
            std::string, sdbusplus::message::variant<
                             std::string, bool, uint8_t, int16_t, uint16_t,
                             int32_t, uint32_t, int64_t, uint64_t, double,
                             std::vector<std::string>>>>>;

/** @brief A fixed array of sensor type - following the LogEntry schema  */
constexpr std::array<const char *, 46> sensorTypeList{
    "Reserved",                            // 0x00
    "Temperature",                         // 0x01
    "Voltage",                             // 0x02
    "Current",                             // 0x03
    "Fan",                                 // 0x04
    "Physical Chassis Security",           // 0x05
    "Platform Security Violation Attempt", // 0x06
    "Processor",                           // 0x07
    "Power Supply / Converter",            // 0x08
    "PowerUnit",                           // 0x09
    "CoolingDevice",                       // 0x0a
    "Other Units-based Sensor",            // 0x0b
    "Memory",                              // 0x0c
    "Drive Slot/Bay",                      // 0x0d
    "POST Memory Resize",                  // 0x0e
    "System Firmware Progress",            // 0x0f
    "Event Logging Disabled",              // 0x10
    "Watchdog",                            // 0x11
    "System Event",                        // 0x12
    "Critical Interrupt",                  // 0x13
    "Button/Switch",                       // 0x14
    "Module/Board",                        // 0x15
    "Microcontroller/Coprocessor",         // 0x16
    "Add-in Card",                         // 0x17
    "Chassis",                             // 0x18
    "ChipSet",                             // 0x19
    "Other FRU",                           // 0x1a
    "Cable/Interconnect",                  // 0x1b
    "Terminator",                          // 0x1c
    "SystemBoot/Restart",                  // 0x1d
    "Boot Error",                          // 0x1e
    "BaseOSBoot/InstallationStatus",       // 0x1f
    "OS Stop/Shutdown",                    // 0x20
    "Slot/Connector",                      // 0x21
    "System ACPI PowerState",              // 0x22
    "Reserved",                            // 0x23
    "Platform Alert",                      // 0x24
    "Entity Presence",                     // 0x25
    "Monitor ASIC/IC",                     // 0x26
    "LAN",                                 // 0x27
    "Management Subsystem Health",         // 0x28
    "Battery",                             // 0x29
    "Reserved",                            // 0x2a
    "Version Change",                      // 0x2b
    "FRUState",                            // 0x2c
    "OEM"                                  // 0xc0
};

inline std::string getEventDirType(uint16_t data)
{
    uint8_t eventDir = 0x0;
    uint8_t eventType = 0x0;
    std::string ret = "";

    eventDir = (data & 0x80);
    eventType = (data & 0x3F);

    if (eventDir)
    {
        ret = "Deassertion";
    }
    else
    {
        ret = "Assertion";
    }

    // Following the SEL spec for the Event Type value
    // 0x01 - Threshold
    // 0x02:0x0C - Discrete
    // 0x6F - Sensor-specific
    // Others will be OEM
    if (eventType == 0x1)
    {
        ret += " Threshold";
    }
    else if ((eventType >= 0x2) && (eventType <= 0x0C))
    {
        ret += " Discrete";
    }
    else if (eventType == 0x6F)
    {
        ret += " Sensor-specific";
    }
    else
    {
        ret += " OEM";
    }

    return ret;
}

inline std::string getSELSpecificInfo(const std::string &s, uint8_t field)
{
    std::string ret = "";
    std::string subSEL = "";
    uint16_t data;

    // The format of AdditionalData for SEL will be:
    // STRING=XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
    switch (field)
    {
        case 0:
            // Request getting the Sensor Type info
            // It will be the byte 11 of SEL info
            subSEL = s.substr(37, 2);
            data = std::stoi(subSEL, nullptr, 16);
            BMCWEB_LOG_DEBUG << "-> Byte 11 0x" << data;
            ret = sensorTypeList[data];
            break;
        case 1:
            // Request getting the Sensor Number info
            // It will be the byte 12 of SEL info
            subSEL = s.substr(40, 2);
            BMCWEB_LOG_DEBUG << "-> Byte 12 0x" << data;
            ret = subSEL;
            break;
        case 2:
            // Request getting the Event Type info
            // It will be the byte 13 of SEL info
            subSEL = s.substr(43, 2);
            data = std::stoi(subSEL, nullptr, 16);
            BMCWEB_LOG_DEBUG << "-> Byte 13 0x" << data;
            ret = getEventDirType(data);
            break;
        default: // Have not support other fields yet
            break;
    }

    return ret;
}

inline std::string translateSeverityDbusToRedfish(const std::string &s)
{
    if (s == "xyz.openbmc_project.Logging.Entry.Level.Alert")
    {
        return "Critical";
    }
    else if (s == "xyz.openbmc_project.Logging.Entry.Level.Critical")
    {
        return "Critical";
    }
    else if (s == "xyz.openbmc_project.Logging.Entry.Level.Debug")
    {
        return "OK";
    }
    else if (s == "xyz.openbmc_project.Logging.Entry.Level.Emergency")
    {
        return "Critical";
    }
    else if (s == "xyz.openbmc_project.Logging.Entry.Level.Error")
    {
        return "Critical";
    }
    else if (s == "xyz.openbmc_project.Logging.Entry.Level.Information")
    {
        return "OK";
    }
    else if (s == "xyz.openbmc_project.Logging.Entry.Level.Notice")
    {
        return "OK";
    }
    else if (s == "xyz.openbmc_project.Logging.Entry.Level.Warning")
    {
        return "Warning";
    }
    return "";
}

/**
 * LogEntry derived class for delivering Log Entry Schema.
 */
class LogEntry : public Node
{
  public:
    template <typename CrowApp>
    LogEntry(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/LogServices/SEL/Entries/<str>/",
             std::string())
    {
        Node::json["@odata.type"] = "#LogEntry.v1_3_0.LogEntry";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#LogEntry.LogEntry";
        Node::json["EntryType"] = "SEL"; // System Event Log

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
            "/redfish/v1/Systems/1/LogServices/SEL/Entries/" + entryId;
        auto asyncResp = std::make_shared<AsyncResp>(res);
        crow::connections::systemBus->async_method_call(
            [asyncResp, entryId](const boost::system::error_code ec,
                                 const GetManagedObjectsTypes &resp) {
                if (ec)
                {
                    // TODO Handle for specific error code
                    asyncResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }

                bool idFound = false;
                for (auto &objectPath : resp)
                {
                    for (auto &interfaceMap : objectPath.second)
                    {
                        if (interfaceMap.first !=
                            "xyz.openbmc_project.Logging.Entry")
                        {
                            BMCWEB_LOG_DEBUG << "Bailing early on "
                                             << interfaceMap.first;
                            continue;
                        }
                        if (idFound == true)
                        {
                            break;
                        }

                        for (auto &propertyMap : interfaceMap.second)
                        {
                            if (propertyMap.first == "Id")
                            {
                                const uint32_t *id =
                                    mapbox::getPtr<const uint32_t>(
                                        propertyMap.second);
                                // only assign properties if the id is matched
                                if (id == nullptr ||
                                    entryId != std::to_string(*id))
                                {
                                    break;
                                }
                                idFound = true;
                                asyncResp->res.jsonValue["Id"] = entryId;
                                asyncResp->res.jsonValue["Name"] =
                                    "Log Entry " + entryId;
                            }
                            else if (propertyMap.first == "Timestamp")
                            {
                                const uint64_t *millisTimeStamp =
                                    mapbox::getPtr<const uint64_t>(
                                        propertyMap.second);
                                if (millisTimeStamp != nullptr)
                                {
                                    // Retrieve Created property with format:
                                    // yyyy-mm-ddThh:mm:ss
                                    std::string created = getDateTime(
                                        Milliseconds{*millisTimeStamp},
                                        "%FT%T%z");
                                    created.insert(created.end() - 2, ':');
                                    asyncResp->res.jsonValue["Created"] =
                                        created;
                                }
                            }
                            else if (propertyMap.first == "Severity")
                            {
                                const std::string *severity =
                                    mapbox::getPtr<const std::string>(
                                        propertyMap.second);
                                if (severity != nullptr)
                                {
                                    asyncResp->res.jsonValue["Severity"] =
                                        translateSeverityDbusToRedfish(
                                            *severity);
                                }
                            }
                            else if (propertyMap.first == "AdditionalData")
                            {
                                const std::vector<std::string> *addData =
                                    mapbox::getPtr<
                                        const std::vector<std::string>>(
                                        propertyMap.second);
                                if (addData != nullptr)
                                {
                                    std::string selData =
                                        (std::string)addData->at(1);
                                    asyncResp->res.jsonValue["MessageId"] =
                                        getSELSpecificInfo(selData, 2);
                                    asyncResp->res.jsonValue["SensorType"] =
                                        getSELSpecificInfo(selData, 0);
                                    asyncResp->res.jsonValue["SensorNumber"] =
                                        std::stoi(
                                            getSELSpecificInfo(selData, 1),
                                            nullptr, 10);
                                }
                            }
                            else if (propertyMap.first == "Message")
                            {
                                const std::string *message =
                                    mapbox::getPtr<const std::string>(
                                        propertyMap.second);
                                if (message != nullptr)
                                {
                                    asyncResp->res.jsonValue["Message"] =
                                        *message;
                                }
                            }
                            else if (propertyMap.first == "Resolved")
                            {
                                const bool *resolved =
                                    mapbox::getPtr<const bool>(
                                        propertyMap.second);
                                if (resolved != nullptr)
                                {
                                    // No place to put this for now
                                }
                            }
                            else
                            {
                                // TODO Retrieve Message Arguments object
                                // TODO Need get MessageId, SensorType,
                                // SensorNumber,
                                // EntryCode, OemRecordFormat and Links object.
                                // Now D-Bus does not support to retrieve these
                                // objects.
                                BMCWEB_LOG_WARNING << "Got extra property in "
                                                      "log entry interface "
                                                   << propertyMap.first;
                            }
                        }
                    }
                }
                if (idFound == false)
                {
                    // some properties may be misassigned so must clear the
                    // response here
                    asyncResp->res.clear();
                    asyncResp->res.result(
                        boost::beast::http::status::not_found);
                    return;
                }
            },
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    }
};

/**
 * LogEntryCollection derived class for delivering Log Entry Collection Schema
 */
class LogEntryCollection : public Node
{
  public:
    template <typename CrowApp>
    LogEntryCollection(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/LogServices/SEL/Entries/")
    {
        Node::json["@odata.type"] = "#LogEntryCollection.LogEntryCollection";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#LogEntryCollection.LogEntryCollection";
        Node::json["@odata.id"] =
            "/redfish/v1/Systems/1/LogServices/SEL/Entries";
        Node::json["Description"] = "Collection of Logs for this System";
        Node::json["Name"] = "Log Service Collection";

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
                        GetManagedObjectsTypes &resp) {
                if (ec)
                {
                    // TODO Handle for specific error code
                    asyncResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }

                nlohmann::json &entries = asyncResp->res.jsonValue["Members"];
                entries = nlohmann::json::array();
                // Iterate over all retrived ObjectPath
                for (auto &objPath : resp)
                {
                    for (auto &interface : objPath.second)
                    {
                        if (interface.first ==
                            "xyz.openbmc_project.Logging.Entry")
                        {
                            const std::string &id = objPath.first;
                            // Cut out everything until last "/", and put it
                            // into output vector
                            entries.push_back(
                                {{"@odata.id",
                                  "/redfish/v1/Systems/1/LogServices/SEL/"
                                  "Entries/" +
                                      id.substr(id.rfind("/") + 1)}});
                        }
                    }
                }
                asyncResp->res.jsonValue["Member@odata.count"] = entries.size();
            },
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    }
};

/**
 * LogServiceActionsClear class supports POST method for ClearLog action.
 */
class LogServiceActionsClear : public Node
{
  public:
    LogServiceActionsClear(CrowApp &app) :
        Node(app,
             "/redfish/v1/Systems/1/LogServices/SEL/Actions/LogService.Reset")
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

        auto asyncResp = std::make_shared<AsyncResp>(res);
        crow::connections::systemBus->async_method_call(
            [asyncResp](const boost::system::error_code ec) {
                if (ec)
                {
                    // TODO Handle for specific error code
                    BMCWEB_LOG_ERROR << "doClearLog resp_handler got error "
                                     << ec;
                    asyncResp->res.result(
                        boost::beast::http::status::internal_server_error);
                    return;
                }

                asyncResp->res.result(boost::beast::http::status::no_content);
            },
            "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
            "xyz.openbmc_project.Collection.DeleteAll", "DeleteAll");
    }
};

/**
 * LogService derived class for delivering Log Service Schema.
 */
class LogService : public Node
{
  public:
    template <typename CrowApp>
    LogService(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/LogServices/SEL/")
    {
        Node::json["@odata.type"] = "#LogService.v1_1_0.LogService";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#LogService.LogService";
        Node::json["Name"] = "System Log Service";
        Node::json["Id"] = "SEL"; // System Event Log
        Node::json["Entries"] = {
            {"@odata.id", "/redfish/v1/Systems/1/LogServices/SEL/Entries"}};

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
        Node::json["@odata.id"] = "/redfish/v1/Systems/1/LogServices/SEL";

        // TODO Logging service has not supported get MaxNumberOfRecords
        // property yet hardcode to ERROR_CAP (200) from phosphor-logging.
        Node::json["MaxNumberOfRecords"] = 200;
        Node::json["OverWritePolicy"] = "WrapsWhenFull"; // TODO hardcoded
                                                         // should retrieve from
                                                         // Logging service
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
            "/redfish/v1/Systems/1/LogServices/SEL/Actions/LogService.Reset";
        Node::json["Actions"]["#LogService.ClearLog"] = clearLog;

        res.jsonValue = Node::json;
        res.end();
    }
};

/**
 * LogServiceCollection derived class for delivering
 * Log Service Collection Schema
 */
class LogServiceCollection : public Node
{
  public:
    template <typename CrowApp>
    LogServiceCollection(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/LogServices/")
    {
        Node::json["@odata.type"] =
            "#LogServiceCollection.LogServiceCollection";
        Node::json["@odata.id"] = "/redfish/v1/Systems/1/LogServices";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#LogServiceCollection.LogServiceCollection";
        Node::json["Name"] = "Log Services Collection";
        Node::json["Members"] = {
            {{"@odata.id", "/redfish/v1/Systems/1/LogServices/SEL"}},
            {{"@odata.id", "/redfish/v1/Systems/1/LogServices/BIOS"}}};
        Node::json["Members@odata.count"] = 2;
        // (System Event Log)

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
