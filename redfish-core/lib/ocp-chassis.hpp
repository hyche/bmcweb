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
 * ChassisCollection derived class for delivering Chassis Collection Schema
 */
class ChassisCollection : public Node {
 public:
  template <typename CrowApp>
  ChassisCollection(CrowApp &app): Node(app, "/redfish/v1/Chassis/") {
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

  // TODO The chassis member will be declared here.
};
}  // namespace redfish
