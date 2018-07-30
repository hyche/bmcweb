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
#include "sensors.hpp"

namespace redfish {

class Thermal : public Node {
 public:
  Thermal(CrowApp& app)
      : Node((app), "/redfish/v1/Chassis/<str>/Thermal/", std::string()) {
    Node::json["@odata.type"] = "#Thermal.v1_2_0.Thermal";
    Node::json["@odata.context"] = "/redfish/v1/$metadata#Thermal.Thermal";
    Node::json["Id"] = "Thermal";
    Node::json["Name"] = "Thermal";

    entityPrivileges = {{crow::HTTPMethod::GET, {{"Login"}}},
                        {crow::HTTPMethod::HEAD, {{"Login"}}},
                        {crow::HTTPMethod::PATCH, {{"ConfigureManager"}}},
                        {crow::HTTPMethod::PUT, {{"ConfigureManager"}}},
                        {crow::HTTPMethod::DELETE, {{"ConfigureManager"}}},
                        {crow::HTTPMethod::POST, {{"ConfigureManager"}}}};
  }

 private:
  void doGet(crow::response& res, const crow::request& req,
             const std::vector<std::string>& params) override {
    if (params.size() != 1) {
      res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
      res.end();
      return;
    }
    const std::string& chassis_name = params[0];

#ifdef OCP_CUSTOM_FLAG // Add specific chassis sub-node name: Thermal
    const std::string& subNodeName = "Thermal";
#endif //OCP_CUSTOM_FLAG
#ifdef OCP_CUSTOM_FLAG // Allocate @odata.id to appropriate schema
    Node::json["@odata.id"] = "/redfish/v1/Chassis/" +
                              chassis_name + "/Thermal";
#endif //OCP_CUSTOM_FLAG
    res.json_value = Node::json;
    auto sensorAsyncResp = std::make_shared<SensorAsyncResp>(
        res, chassis_name,
        std::initializer_list<const char*>{
#ifdef OCP_CUSTOM_FLAG // Remove Entity-Manager object
            "/xyz/openbmc_project/sensors/fan_tach",
            "/xyz/openbmc_project/sensors/temperature"},
        subNodeName);
#else
            "/xyz/openbmc_project/Sensors/fan",
            "/xyz/openbmc_project/Sensors/temperature"});
#endif //OCP_CUSTOM_FLAG
    // TODO Need to get Chassis Redundancy information.
    getChassisData(sensorAsyncResp);
  }
};

}  // namespace redfish
