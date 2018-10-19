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

#include "boost/container/flat_map.hpp"
#include "node.hpp"

#include <utils/json_utils.hpp>

namespace redfish
{

/**
 * @brief Retrieves Host Bios Version properties over D-Bus.
 *
 * @param[in] asyncResp  Shared pointer for completing asynchronous calls.
 *
 * @return None.
 */
void getBiosVersion(std::shared_ptr<AsyncResp> asyncResp)
{
    BMCWEB_LOG_DEBUG << "Get Bios Version enter.";
    crow::connections::systemBus->async_method_call(
        [asyncResp{std::move(asyncResp)}](const boost::system::error_code ec,
                                          const PropertiesType &properties) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "D-Bus response error " << ec;
                asyncResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }
            std::string output{}; // Initial output Bios Version as Null.
            PropertiesType::const_iterator it = properties.find("BiosVersion");
            if (it != properties.end())
            {
                const std::string *s =
                    mapbox::getPtr<const std::string>(it->second);
                BMCWEB_LOG_DEBUG << "Found BiosVersion: " << *s;
                if (nullptr != s)
                {
                    output = *s;
                }
            }
            // Update JSON payload with Bios Version information.
            asyncResp->res.jsonValue["BiosVersion"] = output;
        },
        "xyz.openbmc_project.Software.Host.Updater",
        "/xyz/openbmc_project/software/host/inventory",
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Software.Host");
}

/**
 * Function that retrieves all properties of boot interface.
 * @param[in] asyncResp     Shared pointer for completing asynchronous calls.
 * @return None.
 */
void getBootPolicy(std::shared_ptr<AsyncResp> asyncResp)
{
    BMCWEB_LOG_DEBUG << "Get boot policy enter.";
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const PropertiesType &properties) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "D-Bus response error " << ec;
                asyncResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }
            // Prepare all the schema required fields which retrieved from
            // D-Bus.
            for (const std::string p : std::array<const char *, 2>{
                     "BootSourceOverrideEnabled", "BootSourceOverrideTarget"})
            {
                PropertiesType::const_iterator it = properties.find(p);
                if (it != properties.end())
                {
                    const std::string *s =
                        mapbox::getPtr<const std::string>(it->second);
                    if (s != nullptr)
                    {
                        asyncResp->res.jsonValue["Boot"][p] = *s;
                    }
                }
            }
        },
        "xyz.openbmc_project.Software.Host.Updater",
        "/xyz/openbmc_project/software/host/inventory",
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Software.Host.Boot");
};

/**
 * Function that retrieves all properties of processor interface.
 * @param[in] asyncResp     Shared pointer for completing asynchronous calls.
 * @return None.
 */
void getProcessorSummary(std::shared_ptr<AsyncResp> asyncResp)
{
    BMCWEB_LOG_DEBUG << "Get processor summary enter.";
    // Make call to Host service.
    crow::connections::systemBus->async_method_call(
        [asyncResp{std::move(asyncResp)}](const boost::system::error_code ec,
                                          const PropertiesType &properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "D-Bus response error " << ec;
                asyncResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }
            // Prepare all the schema required fields which retrieved
            // from D-Bus.
            for (const std::string p : std::array<const char *, 4>{
                     "Count", "Model", "State", "Health"})
            {
                PropertiesType::const_iterator it = properties.find(p);
                if (it != properties.end())
                {
                    if (p == "Count")
                    {
                        const uint32_t *count =
                            mapbox::getPtr<const uint32_t>(it->second);
                        if (count != nullptr)
                        {
                            asyncResp->res
                                .jsonValue["ProcessorSummary"]["Count"] =
                                *count;
                        }
                    }
                    else
                    {
                        const std::string *s =
                            mapbox::getPtr<const std::string>(it->second);
                        if (s != nullptr)
                        {
                            if (p == "State" || p == "Health")
                            {
                                asyncResp->res.jsonValue["ProcessorSummary"]
                                                        ["Status"][p] = *s;
                            }
                            else
                            {
                                asyncResp->res
                                    .jsonValue["ProcessorSummary"][p] = *s;
                            }
                        }
                    }
                }
            }
        },
        "xyz.openbmc_project.Software.Host.Updater",
        "/xyz/openbmc_project/software/host/inventory",
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Software.Host.Processor");
}

/*
 * Function that retrieves all properties of memory interface.
 * @param[in] asyncResp     Shared pointer for completing asynchronous calls.
 * @return None.
 */
void getMemorySummary(std::shared_ptr<AsyncResp> asyncResp)
{
    BMCWEB_LOG_DEBUG << "Get system memory summary.";
    crow::connections::systemBus->async_method_call(
        [asyncResp{std::move(asyncResp)}](const boost::system::error_code ec,
                                          const PropertiesType &properties) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "D-Bus response error " << ec;
                asyncResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }

            // Prepare all the schema required fields which retrieved
            // from D-Bus.
            for (const std::string p : std::array<const char *, 3>{
                     "TotalSystemMemoryGiB", "State", "Health"})
            {
                PropertiesType::const_iterator it = properties.find(p);
                if (it == properties.end())
                {
                    continue;
                }
                if (p == "TotalSystemMemoryGiB")
                {
                    const uint32_t *total =
                        mapbox::getPtr<const uint32_t>(it->second);
                    if (total != nullptr)
                    {
                        asyncResp->res.jsonValue["MemorySummary"]
                                                ["TotalSystemMemoryGiB"] =
                            *total;
                    }
                }
                else
                {
                    const std::string *s =
                        mapbox::getPtr<const std::string>(it->second);
                    if (s != nullptr)
                    {
                        asyncResp->res.jsonValue["MemorySummary"]["Status"][p] =
                            *s;
                    }
                }
            }
        },
        "xyz.openbmc_project.Software.Host.Updater",
        "/xyz/openbmc_project/software/host/inventory",
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Software.Host.Memory");
}

/**
 * @brief Retrieves System Unique ID (UUID) properties over D-Bus.
 *
 * @param[in] asyncResp  Shared pointer for completing asynchronous calls.
 *
 * @return None.
 */
void getSystemUniqueID(std::shared_ptr<AsyncResp> asyncResp)
{
    BMCWEB_LOG_DEBUG << "Get System Unique ID.";
    crow::connections::systemBus->async_method_call(
        [asyncResp{std::move(asyncResp)}](const boost::system::error_code ec,
                                          const PropertiesType &properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "D-Bus response error " << ec;
                asyncResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }

            std::string output{}; // Initial output UUID as Null.
            PropertiesType::const_iterator it = properties.find("Record_1");
            if (it != properties.end())
            {
                const std::string *s =
                    mapbox::getPtr<const std::string>(it->second);
                if (nullptr != s)
                {
                    output = *s;
                }
            }
            // Update JSON payload with UUID information.
            asyncResp->res.jsonValue["UUID"] = output;
        },
        "xyz.openbmc_project.Inventory.FRU",
        "/xyz/openbmc_project/inventory/fru0/multirecord",
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.FRU.MultiRecord");
}

/**
 * @brief Retrieves computer system properties over dbus
 *
 * @param[in] aResp Shared pointer for completing asynchronous calls
 * @param[in] name  Computer system name from request
 *
 * @return None.
 */
void getComputerSystem(std::shared_ptr<AsyncResp> asyncResp)
{
    BMCWEB_LOG_DEBUG << "Get Computer System information... ";
    crow::connections::systemBus->async_method_call(
        [asyncResp](const boost::system::error_code ec,
                    const PropertiesType &properties) {
            if (ec)
            {
                BMCWEB_LOG_ERROR << "D-Bus response error: " << ec;
                asyncResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }

            // Verify ifaceName
            for (auto &p : std::array<const std::string, 7>{
                     "Asset_Tag", "Manufacturer", "Model_Number", "Name",
                     "Serial_Number", "Part_Number", "SKU"})
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
                            asyncResp->res.jsonValue["AssetTag"] = *s;
                        }
                        else if (p == "Model_Number")
                        {
                            asyncResp->res.jsonValue["Model"] = *s;
                        }
                        else if (p == "Serial_Number")
                        {
                            asyncResp->res.jsonValue["SerialNumber"] = *s;
                        }
                        else if (p == "Part_Number")
                        {
                            asyncResp->res.jsonValue["PartNumber"] = *s;
                        }
                        else if (p == "SKU")
                        {
                            asyncResp->res.jsonValue["SKU"] = *s;
                        }
                        else
                        {
                            asyncResp->res.jsonValue[p] = *s;
                        }
                    }
                }
            }
        },
        "xyz.openbmc_project.Inventory.FRU",
        "/xyz/openbmc_project/inventory/fru0/product",
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Inventory.FRU.Product");
    getBiosVersion(asyncResp);
    getBootPolicy(asyncResp);
    getMemorySummary(asyncResp);
    getProcessorSummary(asyncResp);
    getSystemUniqueID(asyncResp);
}

/**
 * @brief Retrieves identify led group properties over dbus
 *
 * @param[in] aResp     Shared pointer for completing asynchronous calls.
 * @param[in] callback  Callback for process retrieved data.
 *
 * @return None.
 */
template <typename CallbackFunc>
void getLedGroupIdentify(std::shared_ptr<AsyncResp> aResp,
                         CallbackFunc &&callback)
{
    BMCWEB_LOG_DEBUG << "Get led groups";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)},
         callback{std::move(callback)}](const boost::system::error_code &ec,
                                        const ManagedObjectsType &resp) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                aResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }
            BMCWEB_LOG_DEBUG << "Got " << resp.size() << "led group objects.";
            for (const auto &objPath : resp)
            {
                const std::string &path = objPath.first;
                if (path.rfind("enclosure_identify") != std::string::npos)
                {
                    for (const auto &interface : objPath.second)
                    {
                        if (interface.first == "xyz.openbmc_project.Led.Group")
                        {
                            for (const auto &property : interface.second)
                            {
                                if (property.first == "Asserted")
                                {
                                    const bool *asserted =
                                        mapbox::getPtr<const bool>(
                                            property.second);
                                    if (nullptr != asserted)
                                    {
                                        callback(*asserted, aResp);
                                    }
                                    else
                                    {
                                        callback(false, aResp);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        },
        "xyz.openbmc_project.LED.GroupManager",
        "/xyz/openbmc_project/led/groups", "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects");
}

template <typename CallbackFunc>
void getLedIdentify(std::shared_ptr<AsyncResp> aResp, CallbackFunc &&callback)
{
    BMCWEB_LOG_DEBUG << "Get identify led properties";
    crow::connections::systemBus->async_method_call(
        [aResp,
         callback{std::move(callback)}](const boost::system::error_code ec,
                                        const PropertiesType &properties) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                aResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }
            BMCWEB_LOG_DEBUG << "Got " << properties.size()
                             << "led properties.";
            std::string output;
            for (const auto &property : properties)
            {
                if (property.first == "State")
                {
                    const std::string *s =
                        mapbox::getPtr<std::string>(property.second);
                    if (nullptr != s)
                    {
                        BMCWEB_LOG_DEBUG << "Identify Led State: " << *s;
                        const auto pos = s->rfind('.');
                        if (pos != std::string::npos)
                        {
                            auto led = s->substr(pos + 1);
                            for (const std::pair<const char *, const char *>
                                     &p :
                                 std::array<
                                     std::pair<const char *, const char *>, 3>{
                                     {{"On", "Lit"},
                                      {"Blink", "Blinking"},
                                      {"Off", "Off"}}})
                            {
                                if (led == p.first)
                                {
                                    output = p.second;
                                }
                            }
                        }
                    }
                }
            }
            callback(output, aResp);
        },
        "xyz.openbmc_project.LED.Controller.identify",
        "/xyz/openbmc_project/led/physical/identify",
        "org.freedesktop.DBus.Properties", "GetAll",
        "xyz.openbmc_project.Led.Physical");
}

/**
 * @brief Retrieves host state properties over dbus
 *
 * @param[in] aResp     Shared pointer for completing asynchronous calls.
 *
 * @return None.
 */
void getHostState(std::shared_ptr<AsyncResp> aResp)
{
    BMCWEB_LOG_DEBUG << "Get host information.";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}](
            const boost::system::error_code ec,
            const sdbusplus::message::variant<std::string> &hostState) {
            if (ec)
            {
                BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                aResp->res.result(
                    boost::beast::http::status::internal_server_error);
                return;
            }

            const std::string *s = mapbox::getPtr<const std::string>(hostState);
            BMCWEB_LOG_DEBUG << "Host state: " << *s;
            if (s != nullptr)
            {
                // Verify Host State
                if (*s == "xyz.openbmc_project.State.Host.HostState.Running")
                {
                    aResp->res.jsonValue["PowerState"] = "On";
                    aResp->res.jsonValue["Status"]["State"] = "Enabled";
                }
                else
                {
                    aResp->res.jsonValue["PowerState"] = "Off";
                    aResp->res.jsonValue["Status"]["State"] = "Disabled";
                }
            }
        },
        "xyz.openbmc_project.State.Host", "/xyz/openbmc_project/state/host0",
        "org.freedesktop.DBus.Properties", "Get",
        "xyz.openbmc_project.State.Host", "CurrentHostState");
}

void getHostHealth(std::shared_ptr<AsyncResp> aResp)
{
    BMCWEB_LOG_DEBUG << "Get host heath information.";
    /* By default, the Health is OK */
    aResp->res.jsonValue["Status"]["Health"] = "OK";
    crow::connections::systemBus->async_method_call(
        [aResp{std::move(aResp)}] (
            const boost::system::error_code ec,
            const std::vector<std::pair<
                std::string, std::vector<std::pair<
                                 std::string, std::vector<std::string>>>>>
                &subtree) {
            if (ec)
            {
                /* No log entry */
                return;
            }
            for (auto &obj : subtree)
            {
                const std::vector<
                    std::pair<std::string, std::vector<std::string>>>
                    &connections = obj.second;

                for (auto &conn : connections)
                {
                    const std::string &connectionName = conn.first;
                        crow::connections::systemBus->async_method_call(
                        [aResp{std::move(aResp)}] (
                            const boost::system::error_code error_code,
                            const VariantType &severity) {

                            if (error_code)
                            {
                                /* No log entry */
                                return;
                            }
                            const std::string *severity_value =
                                    mapbox::getPtr<const std::string>(
                                        severity);
                            if (severity_value == nullptr)
                            {
                                /* No log entry */
                                return;
                            }
                            std::string entryLevel =
                                severity_value->substr(
                                severity_value->rfind(".") + 1);
                            /* Value of severity is exist */
                            /* Check to determine the value of heath */
                            /*
                             * Log severity level ->  Health value
                             * Emergency              Critical
                             * Alert                  Critical
                             * Critical               Critical
                             * Error                  Warning
                             * Warning                Warning
                             * Notice                 OK
                             * Debug                  OK
                             * Informational          OK
                             */
                            if ((entryLevel == "Error") ||
                                (entryLevel == "Warning"))
                            {
                                aResp->res.jsonValue
                                  ["Status"]["Health"] = "Warning";
                            }
                            /* Continue checking if have any critical event */
                            if ((entryLevel == "Critical") ||
                                (entryLevel == "Alert") ||
                                (entryLevel == "Emergency"))
                            {
                                aResp->res.jsonValue
                                  ["Status"]["Health"] = "Critical";
                                return;
                            }
                        },
                        connectionName, obj.first,
                        "org.freedesktop.DBus.Properties", "Get",
                        "xyz.openbmc_project.Logging.Entry",
                        "Severity");
                }
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree",
        "/xyz/openbmc_project/logging", int32_t(0),
        std::array<const char *, 1>{
            "xyz.openbmc_project.Logging.Entry"});
};

/**
 * @brief Retrieves Host Name from system call.
 *
 * param[in] None.
 * @return Host name in string.
 * TODO This method should be located in utils.hpp.
 */
std::string getHostName()
{
    std::string hostName;

    std::array<char, HOST_NAME_MAX> hostNameCStr;
    if (gethostname(hostNameCStr.data(), hostNameCStr.size()) == 0)
    {
        hostName = hostNameCStr.data();
    }
    return hostName;
}

/**
 * SystemsCollection derived class for delivering ComputerSystems Collection
 * Schema
 */
class SystemsCollection : public Node
{
  public:
    SystemsCollection(CrowApp &app) : Node(app, "/redfish/v1/Systems/")
    {
        Node::json["@odata.type"] =
            "#ComputerSystemCollection.ComputerSystemCollection";
        Node::json["@odata.id"] = "/redfish/v1/Systems";
        Node::json["@odata.context"] =
            "/redfish/v1/"
            "$metadata#ComputerSystemCollection.ComputerSystemCollection";
        Node::json["Name"] = "Computer System Collection";
        Node::json["Members"] = {{{"@odata.id", "/redfish/v1/Systems/1"}}};
        Node::json["Members@odata.count"] = 1; // TODO hardcoded number
                                               // of base board to 1

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

/**
 * SystemActionsReset class supports handle POST method for Reset action.
 * The class retrieves and sends data directly to D-Bus.
 */
class SystemActionsReset : public Node
{
  public:
    SystemActionsReset(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/<str>/Actions/ComputerSystem.Reset/",
             std::string())
    {
        entityPrivileges = {
            {boost::beast::http::verb::post, {{"ConfigureComponents"}}}};
    }

  private:
    /**
     * Function handles POST method request.
     * Analyzes POST body message before sends Reset request data to D-Bus.
     */
    void doPost(crow::Response &res, const crow::Request &req,
                const std::vector<std::string> &params) override
    {
        // Parse JSON request body.
        nlohmann::json post;
        if (!json_util::processJsonFromRequest(res, req, post))
        {
            return;
        }

        auto asyncResp = std::make_shared<AsyncResp>(res);

        for (const auto &item : post.items())
        {
            if (item.key() == "ResetType")
            {
                const std::string *reqResetType =
                    item.value().get_ptr<const std::string *>();
                if (reqResetType == nullptr)
                {
                    res.result(boost::beast::http::status::bad_request);
                    messages::addMessageToErrorJson(
                        asyncResp->res.jsonValue,
                        messages::actionParameterValueFormatError(
                            item.value().dump(), "ResetType",
                            "ComputerSystem.Reset"));
                    res.end();
                    return;
                }

                std::string command;
                if (*reqResetType == "ForceOff" ||
                    *reqResetType == "ForceRestart")
                {
                    if (*reqResetType == "ForceOff")
                    {
                        command = "xyz.openbmc_project.State.Chassis."
                                  "Transition.Off";
                    }
                    else
                    {
                        command = "xyz.openbmc_project.State.Chassis."
                                  "Transition.Reboot";
                    }

                    crow::connections::systemBus->async_method_call(
                        [asyncResp](const boost::system::error_code ec) {
                            if (ec)
                            {
                                BMCWEB_LOG_ERROR << "D-Bus responses error: "
                                                 << ec;
                                asyncResp->res.result(
                                    boost::beast::http::status::
                                        internal_server_error);
                                return;
                            }
                            // TODO Consider support polling mechanism to verify
                            // status of host and chassis after execute the
                            // requested action.
                            BMCWEB_LOG_DEBUG << "Response with no content";
                            asyncResp->res.result(
                                boost::beast::http::status::no_content);
                        },
                        "xyz.openbmc_project.State.Chassis",
                        "/xyz/openbmc_project/state/chassis0",
                        "org.freedesktop.DBus.Properties", "Set",
                        "xyz.openbmc_project.State.Chassis",
                        "RequestedPowerTransition",
                        sdbusplus::message::variant<std::string>{command});
                    return;
                }

                // all other actions operate on the host
                // Execute Reset Action regarding to each reset type.
                if (*reqResetType == "On")
                {
                    command = "xyz.openbmc_project.State.Host.Transition.On";
                }
                else if (*reqResetType == "GracefulShutdown")
                {
                    command = "xyz.openbmc_project.State.Host.Transition.Off";
                }
                else if (*reqResetType == "GracefulRestart")
                {
                    command =
                        "xyz.openbmc_project.State.Host.Transition.Reboot";
                }
                else
                {
                    res.result(boost::beast::http::status::bad_request);
                    messages::addMessageToErrorJson(
                        asyncResp->res.jsonValue,
                        messages::actionParameterUnknown("Reset",
                                                         *reqResetType));
                    res.end();
                    return;
                }

                crow::connections::systemBus->async_method_call(
                    [asyncResp](const boost::system::error_code ec) {
                        if (ec)
                        {
                            BMCWEB_LOG_ERROR << "D-Bus responses error: " << ec;
                            asyncResp->res.result(boost::beast::http::status::
                                                      internal_server_error);
                            return;
                        }
                        // TODO Consider support polling mechanism to verify
                        // status of host and chassis after execute the
                        // requested action.
                        BMCWEB_LOG_DEBUG << "Response with no content";
                        asyncResp->res.result(
                            boost::beast::http::status::no_content);
                    },
                    "xyz.openbmc_project.State.Host",
                    "/xyz/openbmc_project/state/host0",
                    "org.freedesktop.DBus.Properties", "Set",
                    "xyz.openbmc_project.State.Host", "RequestedHostTransition",
                    sdbusplus::message::variant<std::string>{command});
            }
            else
            {
                messages::addMessageToErrorJson(
                    asyncResp->res.jsonValue,
                    messages::actionParameterUnknown("ComputerSystem.Reset",
                                                     item.key()));
            }
        }
    }
};

/**
 * Systems derived class for delivering Computer Systems Schema.
 */
class Systems : public Node
{
  public:
    /*
     * Default Constructor
     */
    Systems(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/<str>/", std::string())
    {
        Node::json["@odata.type"] = "#ComputerSystem.v1_5_0.ComputerSystem";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#ComputerSystem.ComputerSystem";
        Node::json["Name"] = "Computer System";
        Node::json["SystemType"] = "Physical";
        Node::json["Description"] = "Computer System";
        Node::json["LogServices"] = {
            {"@odata.id", "/redfish/v1/Systems/1/LogServices"}};
        Node::json["Links"]["Chassis"] = {
            {{"@odata.id", "/redfish/v1/Chassis/1"}}};
        Node::json["Links"]["ManagedBy"] = {
            {{"@odata.id", "/redfish/v1/Managers/bmc"}}};
        Node::json["Boot"]["BootSourceOverrideMode"] =
            "Legacy"; // TODO(Dawid), get real boot data
        Node::json["Boot"]["BootSourceOverrideTarget@Redfish.AllowableValues"] =
            allowedBootSourceTarget; // TODO, get real boot data
        Node::json["ProcessorSummary"]["Count"] = int(0);
        Node::json["ProcessorSummary"]["Status"]["State"] = "Disabled";
        Node::json["MemorySummary"]["TotalSystemMemoryGiB"] = int(0);
        Node::json["MemorySummary"]["Status"]["State"] = "Disabled";
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
     * Functions triggers appropriate requests on DBus
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

        const std::string &name = params[0];

        res.jsonValue = Node::json;
        res.jsonValue["@odata.id"] = "/redfish/v1/Systems/" + name;
        res.jsonValue["Id"] = name;
        res.jsonValue["HostName"] = getHostName();
        res.jsonValue["Actions"]["#ComputerSystem.Reset"] = {
            {"target",
             "/redfish/v1/Systems/" + name + "/Actions/ComputerSystem.Reset"},
            {"ResetType@Redfish.AllowableValues",
             {"On", "ForceOff", "ForceRestart", "GracefulRestart",
              "GracefulShutdown"}}};
        auto asyncResp = std::make_shared<AsyncResp>(res);

        getLedGroupIdentify(
            asyncResp,
            [&](const bool &asserted, const std::shared_ptr<AsyncResp> &aResp) {
                if (asserted)
                {
                    // If led group is asserted, then another call is needed to
                    // get led status
                    getLedIdentify(
                        aResp, [](const std::string &ledStatus,
                                  const std::shared_ptr<AsyncResp> &aResp) {
                            if (!ledStatus.empty())
                            {
                                aResp->res.jsonValue["IndicatorLED"] =
                                    ledStatus;
                            }
                        });
                }
                else
                {
                    aResp->res.jsonValue["IndicatorLED"] = "Off";
                }
            });
        getHostState(asyncResp);
        getHostHealth(asyncResp);
        getComputerSystem(asyncResp);
    }

    void doPatch(crow::Response &res, const crow::Request &req,
                 const std::vector<std::string> &params) override
    {
        // Check if there is required param, truly entering this shall be
        // impossible
        auto asyncResp = std::make_shared<AsyncResp>(res);
        if (params.size() != 1)
        {
            res.result(boost::beast::http::status::internal_server_error);
            return;
        }
        // Parse JSON request body
        nlohmann::json patch;
        if (!json_util::processJsonFromRequest(res, req, patch))
        {
            return;
        }

        const std::string &name = params[0];

        res.jsonValue = Node::json;

        for (const auto &item : patch.items())
        {
            const auto &key = item.key();
            const auto &value = item.value();
            if (key == "IndicatorLed")
            {
                const std::string *reqLedState =
                    value.get_ptr<const std::string *>();
                if (reqLedState == nullptr)
                {
                    messages::addMessageToErrorJson(
                        asyncResp->res.jsonValue,
                        messages::propertyValueFormatError(value.dump(), key));
                    return;
                }

                // Verify key value
                std::string dbusLedState;
                if (*reqLedState == "On")
                {
                    dbusLedState =
                        "xyz.openbmc_project.Led.Physical.Action.Lit";
                }
                else if (*reqLedState == "Blink")
                {
                    dbusLedState =
                        "xyz.openbmc_project.Led.Physical.Action.Blinking";
                }
                else if (*reqLedState == "Off")
                {
                    dbusLedState =
                        "xyz.openbmc_project.Led.Physical.Action.Off";
                }
                else
                {
                    messages::addMessageToJsonRoot(
                        res.jsonValue, messages::propertyValueNotInList(
                                           *reqLedState, "IndicatorLED"));
                    return;
                }

                getHostState(asyncResp);
                getComputerSystem(asyncResp);

                // Update led group
                BMCWEB_LOG_DEBUG << "Update led group.";
                crow::connections::systemBus->async_method_call(
                    [asyncResp{std::move(asyncResp)}](
                        const boost::system::error_code ec) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                            asyncResp->res.result(boost::beast::http::status::
                                                      internal_server_error);
                            return;
                        }
                        BMCWEB_LOG_DEBUG << "Led group update done.";
                    },
                    "xyz.openbmc_project.LED.GroupManager",
                    "/xyz/openbmc_project/led/groups/enclosure_identify",
                    "org.freedesktop.DBus.Properties", "Set",
                    "xyz.openbmc_project.Led.Group", "Asserted",
                    sdbusplus::message::variant<bool>(
                        (dbusLedState ==
                                 "xyz.openbmc_project.Led.Physical.Action.Off"
                             ? false
                             : true)));
                // Update identify led status
                BMCWEB_LOG_DEBUG << "Update led SoftwareInventoryCollection.";
                crow::connections::systemBus->async_method_call(
                    [asyncResp{std::move(asyncResp)},
                     reqLedState{std::move(*reqLedState)}](
                        const boost::system::error_code ec) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                            asyncResp->res.result(boost::beast::http::status::
                                                      internal_server_error);
                            return;
                        }
                        BMCWEB_LOG_DEBUG << "Led state update done.";
                        asyncResp->res.jsonValue["IndicatorLED"] =
                            std::move(reqLedState);
                    },
                    "xyz.openbmc_project.LED.Controller.identify",
                    "/xyz/openbmc_project/led/physical/identify",
                    "org.freedesktop.DBus.Properties", "Set",
                    "xyz.openbmc_project.Led.Physical", "State",
                    sdbusplus::message::variant<std::string>(dbusLedState));
            }
            else if (key == "BootSourceOverrideEnabled" ||
                     key == "BootSourceOverrideTarget")
            {
                BMCWEB_LOG_DEBUG
                    << "Request to change Boot Source Override policy";
                // Find key with new override policy
                const std::string *reqBootOverride =
                    value.get_ptr<const std::string *>();
                if (reqBootOverride == nullptr)
                {
                    messages::addMessageToErrorJson(
                        asyncResp->res.jsonValue,
                        messages::propertyValueFormatError(value, key));
                    return;
                }

                // Verify request value
                if (key == "BootSourceOverrideTarget")
                {
                    auto it = std::find(allowedBootSourceTarget.begin(),
                                        allowedBootSourceTarget.end(),
                                        *reqBootOverride);
                    if (it == allowedBootSourceTarget.end())
                    {
                        messages::addMessageToJsonRoot(
                            res.jsonValue,
                            messages::propertyValueNotInList(
                                *reqBootOverride, "BootSourceOverrideTarget"));
                        return;
                    }
                }
                else
                {
                    if (*reqBootOverride != "None" &&
                        *reqBootOverride != "Disabled" &&
                        *reqBootOverride != "Once")
                    {
                        messages::addMessageToJsonRoot(
                            res.jsonValue,
                            messages::propertyValueNotInList(
                                *reqBootOverride, "BootSourceOverrideEnabled"));
                        return;
                    }
                }

                auto asyncResp = std::make_shared<AsyncResp>(res);
                res.jsonValue = Node::json;
                res.jsonValue["@odata.id"] = "/redfish/v1/Systems/" + name;

                getBootPolicy(asyncResp);
                crow::connections::systemBus->async_method_call(
                    [&key, reqBootOverride, asyncResp{std::move(asyncResp)}](
                        const boost::system::error_code ec) {
                        if (ec)
                        {
                            BMCWEB_LOG_DEBUG << "DBUS response error " << ec;
                            asyncResp->res.result(boost::beast::http::status::
                                                      internal_server_error);
                            return;
                        }
                        asyncResp->res.jsonValue["Boot"][key] =
                            std::move(*reqBootOverride);
                    },
                    "xyz.openbmc_project.Software.Host.Updater",
                    "/xyz/openbmc_project/software/host/inventory",
                    "org.freedesktop.DBus.Properties", "Set",
                    "xyz.openbmc_project.Software.Host.Boot", key,
                    sdbusplus::message::variant<std::string>{
                        std::move(*reqBootOverride)});
            }
            else
            {
                messages::addMessageToErrorJson(
                    asyncResp->res.jsonValue,
                    messages::propertyNotWritable(item.key()));
                return;
            }
        }
    }

    std::array<std::string, 7> allowedBootSourceTarget = {
        "None", "Pxe", "Hdd", "Cd", "BiosSetup", "UefiShell", "Usb"};
};
} // namespace redfish
