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
#include <boost/container/flat_map.hpp>

namespace redfish {

/**
 * D-Bus types primitives for several generic DBus interfaces
 * TODO consider move this to separate file into boost::dbus
 */
using PropertiesMapType =
    boost::container::flat_map<std::string, dbus::dbus_variant>;

using GetManagedObjectsType = boost::container::flat_map<
    dbus::object_path,
    boost::container::flat_map<std::string, PropertiesMapType>>;

using GetAllPropertiesType = PropertiesMapType;

/**
 * OnDemandLogServiceProvider
 * Log Service provider class that retrieves data directly from dbus,
 * before setting it into JSON output. This does not cache any data.
 *
 * TODO
 * This perhaps shall be different file, which has to be chosen on compile time
 * depending on OEM needs
 */
class OnDemandLogServiceProvider {
 private:
  // Helper function that allows to extract GetAllPropertiesType from
  // GetManagedObjectsType, based on object path, and interface name
  const PropertiesMapType *extractInterfaceProperties(
      const dbus::object_path &objpath, const std::string &interface,
      const GetManagedObjectsType &dbus_data) {
    const auto &dbus_obj = dbus_data.find(objpath);
    if (dbus_obj != dbus_data.end()) {
      const auto &iface = dbus_obj->second.find(interface);
      if (iface != dbus_obj->second.end()) {
        return &iface->second;
      }
    }
    return nullptr;
  }

  // Helper Wrapper that does inline object_path conversion from string
  // into dbus::object_path type
  inline const PropertiesMapType *extractInterfaceProperties(
      const std::string &objpath, const std::string &interface,
      const GetManagedObjectsType &dbus_data) {
    const auto &dbus_obj = dbus::object_path{objpath};
    return extractInterfaceProperties(dbus_obj, interface, dbus_data);
  }

  // Helper function that allows to get pointer to the property from
  // GetAllPropertiesType native, or extracted by GetAllPropertiesType
  template <typename T>
  inline const T *extractProperty(const PropertiesMapType &properties,
                                  const std::string &name) {
    const auto &property = properties.find(name);
    if (property != properties.end()) {
      return boost::get<T>(&property->second);
    }
    return nullptr;
  }
  // TODO Consider to move the above functions to D-Bus
  // generic_interfaces.hpp

 public:
  /**
   * Function that retrieves all Log Entries Interfaces available through
   * Logging Service
   * @param asyncResp Pointer to object holding the response data
   * @param callback a function that shall be called to
   * convert D-Bus output into JSON.
   */
  template <typename CallbackFunc>
  void getLogEntriesIfaceList(
                        const std::shared_ptr<AsyncResp>& asyncResp,
                        CallbackFunc &&callback) {
    const dbus::endpoint loggingService = {
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"};

    // Process response for Logging service and extract interface list
    auto resp_handler = [ asyncResp, callback{std::move(callback)} ](
        const boost::system::error_code ec, GetManagedObjectsType& resp) {
      CROW_LOG_DEBUG << "getLogEntriesIfaceList resp_handler callback Done";
      // Callback requries vector<string> to retrieve all available
      // Logging entry interfaces
      std::vector<std::string> iface_list;
      iface_list.reserve(resp.size());

      if (ec) {
        // TODO Handle for specific error code
        CROW_LOG_ERROR << "getLogEntriesIfaceList resp_handler got error"
                       << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }

      // Iterate over all retrived ObjectPath
      for (auto &objpath : resp) {
        // And all interfaces available for certain ObjectPath.
        for (auto &interface : objpath.second) {
          // If interface is xyz.openbmc_project.Logging.Entry
          if (interface.first == "xyz.openbmc_project.Logging.Entry") {
            // Cut out everything until last "/", ...
            const std::string &iface_id = objpath.first.value;
            CROW_LOG_DEBUG << "Found iface: " << iface_id;
            std::size_t last_pos = iface_id.rfind("/");
            if (last_pos != std::string::npos) {
              // and put it into output vector
              iface_list.emplace_back(iface_id.substr(last_pos + 1));
            }
          }
        }
      }
      // Finally make a callback with useful data
      callback(iface_list);
    };

    // Make call to Logging Service to find all log entry objects
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     loggingService);
  }
};

/**
 * LogEntryCollection derived class for delivering Log Entry Collection Schema
 */
class LogEntryCollection : public Node {
 public:
  template <typename CrowApp>
  LogEntryCollection(CrowApp &app)
    : Node(app, "/redfish/v1/Systems/1/LogServices/SEL/Entries/") {
    Node::json["@odata.type"] = "#LogEntryCollection.LogEntryCollection";
    Node::json["@odata.context"] =
                "/redfish/v1/$metadata#LogEntryCollection.LogEntryCollection";
    Node::json["@odata.id"] = "/redfish/v1/Systems/1/LogServices/SEL/Entries";
    Node::json["Description"] = "Collection of Logs for this System";
    Node::json["Name"] = "Log Service Collection";

    entityPrivileges = {{crow::HTTPMethod::GET, {{"Login"}}},
                       {crow::HTTPMethod::HEAD, {{"Login"}}},
                       {crow::HTTPMethod::PATCH, {{"ConfigureComponents"}}},
                       {crow::HTTPMethod::PUT, {{"ConfigureComponents"}}},
                       {crow::HTTPMethod::DELETE, {{"ConfigureComponents"}}},
                       {crow::HTTPMethod::POST, {{"ConfigureComponents"}}}};
  }

 private:
  /**
   * Functions triggers appropriate requests on D-Bus
   */
  void doGet(crow::response &res, const crow::request &req,
             const std::vector<std::string> &params) override {
    // Update JSON payload
    res.json_value = Node::json;
    // Create asyncResp pointer to object holding the response data
    auto asyncResp = std::make_shared<AsyncResp>(res);
    // Process callback to prepare JSON payload
    auto callback = [ asyncResp ](
                                  const std::vector<std::string> &iface_list) {
      nlohmann::json iface_array = nlohmann::json::array();
      for (const std::string &iface_item : iface_list) {
        iface_array.push_back(
            {{"@odata.id", "/redfish/v1/Systems/1/LogServices/SEL/Entries/" +
              iface_item}});
      }
      asyncResp->res.json_value["Members"] = iface_array;
      asyncResp->res.json_value["Member@odata.count"] = iface_array.size();
    };

    // Get log entry interface list, and call the resp_handler callback
    // for JSON payload
    logservice_provider.getLogEntriesIfaceList(asyncResp, callback);
  }

  // Log Service Provider object.
  // TODO Consider to move it to singleton.
  OnDemandLogServiceProvider logservice_provider;
};


/**
 * LogServiceActionsClear class supports POST method for ClearLog action.
 */
class LogServiceActionsClear : public Node {
 public:
  LogServiceActionsClear(CrowApp& app)
      : Node(app,
            "/redfish/v1/Systems/1/LogServices/SEL/Actions/LogService.Reset") {

    entityPrivileges = {{crow::HTTPMethod::GET, {{"Login"}}},
                        {crow::HTTPMethod::HEAD, {{"Login"}}},
                        {crow::HTTPMethod::PATCH, {{"ConfigureManager"}}},
                        {crow::HTTPMethod::PUT, {{"ConfigureManager"}}},
                        {crow::HTTPMethod::DELETE, {{"ConfigureManager"}}},
                        {crow::HTTPMethod::POST, {{"ConfigureManager"}}}};
  }

 private:
  /**
   * Function handles GET method request.
   * LogServiceActionsClear supports for POST method,
   * it is not required to retrieve more information in GET.
   */
  void doGet(crow::response& res, const crow::request& req,
             const std::vector<std::string>& params) override {
    res.json_value = Node::json;
    res.end();
  }

  /**
   * Function handles POST method request.
   * The Clear Log actions does not require any parameter.The action deletes
   * all entries found in the Entries collection for this Log Service.
   */
  void doPost(crow::response& res, const crow::request& req,
              const std::vector<std::string>& params) override {
    CROW_LOG_DEBUG << "Do delete all entries.";

    auto asyncResp = std::make_shared<AsyncResp>(res);
    // Execute Action
    doClearLog(asyncResp);
  }

  /**
   * Function will delete all entries found in the Entries collection for
   * this Log Service.
   */
  void doClearLog(const std::shared_ptr<AsyncResp>& asyncResp) {
    const dbus::endpoint object_logging(
        "xyz.openbmc_project.Logging", "/xyz/openbmc_project/logging",
        "xyz.openbmc_project.Collection.DeleteAll", "DeleteAll");

    // Process response from Logging service.
    auto resp_handler = [asyncResp](const boost::system::error_code ec) {
      CROW_LOG_DEBUG << "doClearLog resp_handler callback: Done";
      if (ec) {
        // TODO Handle for specific error code
        CROW_LOG_ERROR << "doClearLog resp_handler got error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }

      asyncResp->res.code = static_cast<int>(HttpRespCode::NO_CONTENT);
    };

    // Make call to Logging service to request Clear Log
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     object_logging);
  }
};

/**
* LogService derived class for delivering Log Service Schema.
*/
class LogService : public Node {
public:
 template <typename CrowApp>
 LogService(CrowApp &app)
   : Node(app, "/redfish/v1/Systems/1/LogServices/<str>/", std::string()),
     memberActionsClear(app) {
   Node::json["@odata.type"] = "#LogService.v1_1_0.LogService";
   Node::json["@odata.context"] =
                              "/redfish/v1/$metadata#LogService.LogService";
   Node::json["Name"] = "System Log Service";
   Node::json["Id"] = "SEL"; // System Event Log
   Node::json["Entries"] =
              {{"@odata.id", "/redfish/v1/Systems/1/LogServices/SEL/Entries"}};

   entityPrivileges = {{crow::HTTPMethod::GET, {{"Login"}}},
                       {crow::HTTPMethod::HEAD, {{"Login"}}},
                       {crow::HTTPMethod::PATCH, {{"ConfigureComponents"}}},
                       {crow::HTTPMethod::PUT, {{"ConfigureComponents"}}},
                       {crow::HTTPMethod::DELETE, {{"ConfigureComponents"}}},
                       {crow::HTTPMethod::POST, {{"ConfigureComponents"}}}};
 }

private:
 /**
  * Functions triggers appropriate requests on D-Bus
  */
 void doGet(crow::response &res, const crow::request &req,
            const std::vector<std::string> &params) override {
   // Check if there is required param, truly entering this shall be
   // impossible
   if (params.size() != 1) {
     res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
     res.end();
     return;
   }

   // Get Log Service name
   const std::string &name = params[0];
   Node::json["@odata.id"] = "/redfish/v1/Systems/1/LogServices/" + name;

   // TODO Logging service has not supported get MaxNumberOfRecords property yet
   // hardcode to ERROR_CAP (200) from phosphor-logging.
   Node::json["MaxNumberOfRecords"] = 200;
   Node::json["OverWritePolicy"] = "WrapsWhenFull"; // TODO hardcoded
                                                    // should retrieve from
                                                    // Logging service
   // Get DateTime with format: yyyy-mm-ddThh:mm:ssZhh:mm
   Node::json["DateTime"] = getDateTime("%FT%T%z");
   // Get DateTimeLocalOffset with format: Zhh:mm
   Node::json["DateTimeLocalOffset"] = getDateTime("%z");
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

   res.json_value = Node::json;
   res.end();
 }

 /**
  * Function returns Date Time information according to requested format
  */
 std::string getDateTime(const char* format) {
   std::array<char, 128> dateTime;
   std::string redfishDateTime{};
   std::time_t time = std::time(nullptr);

   if (std::strftime(dateTime.begin(), dateTime.size(), format,
                     std::localtime(&time))) {
     redfishDateTime = dateTime.data();
     redfishDateTime.insert(redfishDateTime.end() - 2, ':');
   }

   return redfishDateTime;
 }

 // Action ClearLog object as a member of LogService resource.
 // Handle clear log action from POST request
 LogServiceActionsClear memberActionsClear;
};

/**
 * LogServiceCollection derived class for delivering
 * Log Service Collection Schema
 */
class LogServiceCollection : public Node {
 public:
  template <typename CrowApp>
  LogServiceCollection(CrowApp &app)
    : Node(app, "/redfish/v1/Systems/1/LogServices/") {
    Node::json["@odata.type"] = "#LogServiceCollection.LogServiceCollection";
    Node::json["@odata.id"] = "/redfish/v1/Systems/1/LogServices";
    Node::json["@odata.context"] =
        "/redfish/v1/$metadata##LogServiceCollection.LogServiceCollection";
    Node::json["Name"] = "Log Services Collection";
    Node::json["Members"] =
                    {{{"@odata.id", "/redfish/v1/Systems/1/LogServices/SEL"}}};
    Node::json["Members@odata.count"] = 1; // TODO There supports only SEL
                                           // (System Event Log)

    entityPrivileges = {{crow::HTTPMethod::GET, {{"Login"}}},
                        {crow::HTTPMethod::HEAD, {{"Login"}}},
                        {crow::HTTPMethod::PATCH, {{"ConfigureComponents"}}},
                        {crow::HTTPMethod::PUT, {{"ConfigureComponents"}}},
                        {crow::HTTPMethod::DELETE, {{"ConfigureComponents"}}},
                        {crow::HTTPMethod::POST, {{"ConfigureComponents"}}}};
  }

 private:
  /**
   * Functions triggers appropriate requests on D-Bus
   */
  void doGet(crow::response &res, const crow::request &req,
             const std::vector<std::string> &params) override {
    res.json_value = Node::json;
    res.end();
  }

};
}  // namespace redfish
