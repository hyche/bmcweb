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

using PropertiesType =
    boost::container::flat_map<std::string, dbus::dbus_variant>;

/**
 * OnDemandComputerSystemProvider
 * Computer System provider class that retrieves data directly from D-Bus,
 * before setting it into JSON output. This does not cache any data.
 *
 * Class can be a good example on how to scale different data providing
 * solutions to produce single schema output.
 *
 * TODO
 * This perhaps shall be different file, which has to be chosen on compile time
 * depending on OEM needs
 */
class OnDemandComputerSystemProvider {
 public:
  /**
  * Function that retrieves all properties of Computer System.
  * @param[in] asyncResp Shared pointer for completing asynchronous calls.
  * @param[in] ifaceName Target Interface name to retrieve D-Bus data.
  * @return None.
  */
  void getComputerSystem(const std::shared_ptr<AsyncResp>& asyncResp,
                         const std::string& ifaceName) {
    CROW_LOG_DEBUG << "Get Computer System information... ";
    const dbus::endpoint objComputerSystem = {
        "xyz.openbmc_project.Inventory.Manager",
        "/xyz/openbmc_project/inventory/system",
        "org.freedesktop.DBus.Properties", "GetAll"};
    // Process response and extract data
    auto resp_handler = [ asyncResp, ifaceName ](
                          const boost::system::error_code ec,
                          const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error: " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }

      // Verify ifaceName
      if (ifaceName == "xyz.openbmc_project.Inventory.Decorator.Asset") {
        // Prepare all the schema required fields which retrieved from D-Bus.
        for (const char *p :
             std::array<const char *, 4>
                 {"Manufacturer",
                  "Model",
                  "PartNumber",
                  "SerialNumber"}) {
          PropertiesType::const_iterator it = properties.find(p);
          if (it != properties.end()) {
            const std::string *s = boost::get<std::string>(&it->second);
            if (s != nullptr) {
              asyncResp->res.json_value[p] = *s;
            }
          }
        }
      } else if (ifaceName == "xyz.openbmc_project.Inventory.Item") {
        // Look for PrettyName property
        PropertiesType::const_iterator it = properties.find("PrettyName");
        if (it != properties.end()) {
          const std::string *s = boost::get<std::string>(&it->second);
          if (s != nullptr) {
            asyncResp->res.json_value["Name"] = *s;
          }
        }
      } else if (ifaceName ==
                 "xyz.openbmc_project.Inventory.Decorator.AssetTag") {
        // Look for AssetTag property
        PropertiesType::const_iterator it = properties.find("AssetTag");
        if (it != properties.end()) {
          const std::string *s = boost::get<std::string>(&it->second);
          if (s != nullptr) {
            CROW_LOG_DEBUG << "Found AssetTag: " << *s;
            asyncResp->res.json_value["AssetTag"] = *s;
          }
        }
      } else {
        CROW_LOG_ERROR << "Bad Request Interface Name: " << ifaceName;
        asyncResp->res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
        return;
      }
    };
    // Make call to Inventory Manager service to find computer system info.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objComputerSystem,
                                                     ifaceName);
  }

  /**
   * @brief Retrieves Host state properties over D-Bus
   *
   * @param[in] asyncResp  Shared pointer for completing asynchronous calls.
   * @return None.
   */
  void getHostState(const std::shared_ptr<AsyncResp>& asyncResp) {
    CROW_LOG_DEBUG << "Get Host state.";
    const dbus::endpoint objHostState = {
        "xyz.openbmc_project.State.Host",
        "/xyz/openbmc_project/state/host0",
        "org.freedesktop.DBus.Properties", "GetAll"};
    const std::string ifaceName = "xyz.openbmc_project.State.Host";
    // Process response and extract data
    auto resp_handler = [ asyncResp ](const boost::system::error_code ec,
                                  const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      PropertiesType::const_iterator it =
                                     properties.find("CurrentHostState");
      if (it != properties.end()) {
        const std::string *s = boost::get<std::string>(&it->second);
        if (s != nullptr) {
          const std::size_t pos = s->rfind('.');
          if (pos != std::string::npos) {
            // Retrieve Host State
            const std::string hostState = s->substr(pos + 1);
            // Determine PowerState and State property
            if (hostState == "Running") {
              asyncResp->res.json_value["PowerState"] = "On";
              asyncResp->res.json_value["Status"]["State"] = "Enabled";
            } else if (hostState == "Off") {
              asyncResp->res.json_value["PowerState"] = "Off";
              asyncResp->res.json_value["Status"]["State"] = "Disabled";
            } else if (hostState == "Quiesced") {
              asyncResp->res.json_value["PowerState"] = "On";
              asyncResp->res.json_value["Status"]["State"] = "Quiesced";
            } else {
              CROW_LOG_ERROR << "Invalid host state: " << hostState;
            }
            // TODO D-Bus does not support to retrieve Health property yet.
            asyncResp->res.json_value["Status"]["Health"] = "";
          }
        }
      }
    };
    // Make call to Host State service to retrieve host state.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objHostState,
                                                     ifaceName);
  }
};

/**
 * SystemsCollection derived class for delivering
 * Computer Systems Collection Schema
 */
class SystemsCollection : public Node {
 public:
  template <typename CrowApp>
  SystemsCollection(CrowApp &app): Node(app, "/redfish/v1/Systems/") {
    Node::json["@odata.type"] =
                          "#ComputerSystemCollection.ComputerSystemCollection";
    Node::json["@odata.id"] = "/redfish/v1/Systems";
    Node::json["@odata.context"] =
        "/redfish/v1/"
        "$metadata#ComputerSystemCollection.ComputerSystemCollection";
    Node::json["Name"] = "Computer System Collection";
    Node::json["Members"] =
                    {{{"@odata.id", "/redfish/v1/Systems/1"}}};
    Node::json["Members@odata.count"] = 1; // TODO hardcoded number
                                           // of base board to 1

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

/**
* Systems derived class for delivering Computer Systems Schema.
*/
class Systems : public Node {
 public:
  template <typename CrowApp>
  Systems(CrowApp &app)
       : Node(app, "/redfish/v1/Systems/<str>/", std::string()) {
     Node::json["@odata.type"] = "#ComputerSystem.v1_5_0.ComputerSystem";
     Node::json["@odata.context"] =
                  "/redfish/v1/$metadata#ComputerSystem.ComputerSystem";
     Node::json["Name"] = "Computer System";

     Node::json["SystemType"] = "Physical";
     Node::json["Description"] = "Computer System";
     Node::json["Boot"]["BootSourceOverrideEnabled"] =
       "Disabled";  // TODO get real boot data
     Node::json["Boot"]["BootSourceOverrideTarget"] =
       "None";  // TODO get real boot data
     Node::json["Boot"]["BootSourceOverrideMode"] =
       "Legacy";  // TODO get real boot data
     Node::json["Boot"]["BootSourceOverrideTarget@Redfish.AllowableValues"] = {
       "None", "Pxe", "Hdd", "Usb"};  // TODO get real boot data

     Node::json["LogServices"] =
                     {{"@odata.id", "/redfish/v1/Systems/1/LogServices"}};
     Node::json["Links"]["Chassis"] =
                    {{{"@odata.id", "/redfish/v1/Chassis/1"}}};
     Node::json["Links"]["ManagedBy"] =
                    {{{"@odata.id", "/redfish/v1/Managers/bmc"}}};
     Node::json["Id"] = 1; // TODO hardcoded number of base board to 1.

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

    // Get system id
    const std::string &id = params[0];

    res.json_value = Node::json;
    res.json_value["@odata.id"] = "/redfish/v1/Systems/" + id;

    // Create aResp pointer to object holding the response data
    auto asyncResp = std::make_shared<AsyncResp>(res);
    // Get Computer System information via interface name
    const std::array<const std::string, 3> arrayIfaceName
    {"xyz.openbmc_project.Inventory.Decorator.Asset",
     "xyz.openbmc_project.Inventory.Item",
     "xyz.openbmc_project.Inventory.Decorator.AssetTag"};
    for (const std::string ifaceName : arrayIfaceName) {
      provider.getComputerSystem(asyncResp, ifaceName);
    }

    // Get Host state
    provider.getHostState(asyncResp);
  }

  // Computer System Provider object.
  // TODO Consider to move it to singleton.
  OnDemandComputerSystemProvider provider;

};

}  // namespace redfish
