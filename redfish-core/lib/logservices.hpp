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
