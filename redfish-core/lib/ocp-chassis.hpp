/*
// Copyright (c) 2018 Ampere Computing LLC
// Author: Tung Vu <tung.vu@amperecomputing.com>
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
//
// The OCP chassis module supports to retrieve chassis inventory via D-Bus.
// It's used to replace the redfish-core/lib/chassis.hpp module
// (https://github.com/openbmc/bmcweb).
// Module is implemented in difference way from chassis.hpp which uses
// Entity-Manager service (https://github.com/openbmc/entity-manager).
*/

#pragma once

#include "node.hpp"
#include <boost/container/flat_map.hpp>

namespace redfish {

/**
 * D-Bus types primitives for several generic D-Bus interfaces
 * TODO consider move this to separate file into boost::dbus
 */
using ManagedObjectsType = std::vector<
    std::pair<dbus::object_path,
              std::vector<std::pair<
                  std::string,
                  std::vector<std::pair<std::string, dbus::dbus_variant>>>>>>;

using PropertiesType =
    boost::container::flat_map<std::string, dbus::dbus_variant>;

/**
* ChassisAsyncResp
* Gathers data needed for response processing after async calls are done
*/
class ChassisAsyncResp {
public:
 ChassisAsyncResp(crow::response& response): res(response) {}

 ~ChassisAsyncResp() {
   if (res.code != static_cast<int>(HttpRespCode::OK)) {
     // Reset the json object to clear out any data that made it in before the
     // error happened
     // TODO handle error condition with proper code
     res.json_value = nlohmann::json::object();
   }
   res.end();
 }
 void setErrorStatus() {
   res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
 }

 crow::response& res;
};

/**
 * Chassis override class for delivering Chassis Schema
 */
class Chassis : public Node {
 public:
  /*
   * Default Constructor
   */
  template <typename CrowApp>
  Chassis(CrowApp &app)
      : Node(app, "/redfish/v1/Chassis/1/") {
    Node::json["@odata.type"] = "#Chassis.v1_4_0.Chassis";
    Node::json["@odata.id"] = "/redfish/v1/Chassis/1";
    Node::json["@odata.context"] = "/redfish/v1/$metadata#Chassis.Chassis";
    Node::json["Name"] = "Ampere System Chassis"; // TODO hardcode in temporary.
    Node::json["ChassisType"] = "Rack Mount Chassis";
    Node::json["Id"] = "1";
    // TODO Currently not support "SKU" and "AssetTag" yet
    Node::json["SKU"] = "";
    Node::json["AssetTag"] = "";

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
    auto asyncResp = std::make_shared<ChassisAsyncResp>(res);

    // Create JSON copy based on Node::json, this is to avoid possible
    // race condition
    nlohmann::json json_response(Node::json);

    // Thermal object
    json_response["Thermal"] = {
                        {"@odata.id", "/redfish/v1/Chassis/1/Thermal"}};
    // Power object
    json_response["Power"] = {
                        {"@odata.id", "/redfish/v1/Chassis/1/Power"}};
    // An array of references to computer systems contained in this chassis.
    json_response["Links"]["ComputerSystems"] =
                              {{{"@odata.id", "/redfish/v1/Systems/1"}}};
    // An array of references to the Managers responsible
    // for managing this chassis.
    json_response["Links"]["ManagedBy"] =
                            {{{"@odata.id", "/redfish/v1/Managers/1"}}};

    asyncResp->res.json_value = json_response;

  }

  // TODO Chassis Provider object will be declared here
};

/**
 * ChassisCollection derived class for delivering Chassis Collection Schema
 */
class ChassisCollection : public Node {
 public:
  template <typename CrowApp>
  ChassisCollection(CrowApp &app)
    : Node(app, "/redfish/v1/Chassis/"), memberChassis(app) {
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

  // The Chassis object as member of ChassisCollection.
  Chassis memberChassis;
};
}  // namespace redfish
