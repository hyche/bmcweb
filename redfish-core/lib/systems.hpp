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

#include <error_messages.hpp>
#include <utils/json_utils.hpp>
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

  // List of allowed Reset Type for Computer System resource.
  // TODO These should be retrieved from D-Bus.
  const std::vector<std::string> allowedResetType{
               "On",                // Turn the unit on.
               "ForceOff",          // Turn the unit off immediately
                                    // (non-graceful shutdown).
               "GracefulRestart",   // Perform a graceful shutdown
                                    // followed by a restart of the system.
               "GracefulShutdown",  // Perform a graceful shutdown
                                    // and power off.
               "ForceRestart"};     // Perform an immediate shutdown,
                                    // followed by a restart.

  // List of allowed boot source to be overridden.
  // TODO These should be retrieved from D-Bus.
  const std::vector<std::string> allowedBootSourceTarget{
               "None", // Boot from the normal boot device.
               "Pxe", // Boot from the Pre-Boot EXecution (PXE) environment.
               "Usb", // Boot from a USB device as specified by the system BIOS.
               "Hdd", // Boot from a hard drive.
               "UefiShell"}; // Boot to the UEFI Shell.

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
          }
        }
      }
      it = properties.find("HealthState");
      if (it != properties.end()) {
        const std::string *s = boost::get<std::string>(&it->second);
        if (s != nullptr) {
          // Retrieve the Health value from:
          // xyz.openbmc_project.State.Host.Health.<state>
          const std::size_t pos = s->rfind('.');
          if (pos != std::string::npos) {
            asyncResp->res.json_value["Status"]["Health"] = s->substr(pos + 1);
          }
        }
      }
    };
    // Make call to Host State service to retrieve host state.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objHostState,
                                                     ifaceName);
  }

  /**
   * @brief Retrieves Host Name from system call.
   *
   * param[in] None.
   * @return Host name in string.
   * TODO This method should be located in utils.hpp.
   */
  std::string getHostName() const {
    std::string hostName;

    std::array<char, HOST_NAME_MAX> hostNameCStr;
    if (gethostname(hostNameCStr.data(), hostNameCStr.size()) == 0) {
      hostName = hostNameCStr.data();
    }
    return hostName;
  }

  /**
   * @brief Retrieves identify led group properties over D-Bus.
   *
   * @param[in] asyncResp  Shared pointer for completing asynchronous calls.
   * @param[in] callback   Callback for process retrieved data.
   *
   * @return None.
   */
  template <typename CallbackFunc>
  void getLedGroupIdentify(const std::shared_ptr<AsyncResp> &asyncResp,
                           CallbackFunc &&callback) {
    CROW_LOG_DEBUG << "Get LED groups.";
    const dbus::endpoint objLedGroups = {
      "xyz.openbmc_project.LED.GroupManager",
      "/xyz/openbmc_project/led/groups",
      "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"};
    // Process response from LED group manager service.
    auto resp_handler = [ asyncResp, &callback ](
                            const boost::system::error_code ec,
                            const ManagedObjectsType &resp) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Got " << resp.size() << " led group objects.";
      for (const auto &objPath : resp) {
        const std::string &path = objPath.first.value;
        // Look for enclosure_identify object path...
        if (path.rfind("enclosure_identify") != std::string::npos) {
          for (const auto &interface : objPath.second) {
            // then find out the LED Group interface...
            if (interface.first == "xyz.openbmc_project.Led.Group") {
              for (const auto &property : interface.second) {
                // retrieve Asserted property from it.
                if (property.first == "Asserted") {
                  const bool asserted = boost::get<bool>(property.second);
                  CROW_LOG_DEBUG << "Found Asserted with value: " << asserted;
                  callback(asserted, asyncResp);
                }
              }
            }
          }
        }
      }
    };
    // Make call to LED Group Manager service.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objLedGroups);
  }

  /**
   * @brief Retrieves identify led properties over D-Bus.
   *
   * @param[in] asyncResp  Shared pointer for completing asynchronous calls.
   * @param[in] callback   Callback for process retrieved data.
   *
   * @return None.
   */
  template <typename CallbackFunc>
  void getLedIdentify(const std::shared_ptr<AsyncResp> &asyncResp,
                      CallbackFunc &&callback) {
    CROW_LOG_DEBUG << "Get LED indentify property.";
    const dbus::endpoint objLedIdentify = {
      "xyz.openbmc_project.LED.Controller.identify",
      "/xyz/openbmc_project/led/physical/identify",
      "org.freedesktop.DBus.Properties", "GetAll"};
    const std::string ifaceName = "xyz.openbmc_project.Led.Physical";
    // Process response from LED Controller service.
    auto resp_handler = [ asyncResp, &callback ](
                            const boost::system::error_code ec,
                            const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Got " << properties.size() << " led properties.";
      std::string output{};   // Initial output Led State as Null.
      PropertiesType::const_iterator it = properties.find("State");
      if (it != properties.end()) {
        const std::string *s =
              boost::get<std::string>(&it->second);
        if (nullptr != s) {
          CROW_LOG_DEBUG << "Identify Led State: " << *s;
          const auto pos = s->rfind('.');
          if (pos != std::string::npos) {
            const auto led = s->substr(pos + 1);
            for (const std::pair<const char *, const char *> &p :
                 std::array<std::pair<const char *, const char *>, 3>{
                      {{"On", "Lit"},
                      {"Blink", "Blinking"},
                      {"Off", "Off"}}}) {
              if (led == p.first) {
                output = p.second;
              }
            }
          }
        }
      }
      // Update JSON payload with LED state information.
      callback(output, asyncResp);
    };
    // Make call to LED Controller service.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objLedIdentify,
                                                     ifaceName);
  }

  /**
   * @brief Retrieves Host Bios Version properties over D-Bus.
   *
   * @param[in] asyncResp  Shared pointer for completing asynchronous calls.
   *
   * @return None.
   */
  void getBiosVersion(const std::shared_ptr<AsyncResp> &asyncResp) {
    CROW_LOG_DEBUG << "Get Host Software properties.";
    const dbus::endpoint objHostUpdater = {
      "xyz.openbmc_project.Software.Host.Updater",
      "/xyz/openbmc_project/software/host/inventory",
      "org.freedesktop.DBus.Properties", "GetAll"};
    const std::string ifaceName = "xyz.openbmc_project.Software.Host";

    auto resp_handler = [ asyncResp ](
                            const boost::system::error_code ec,
                            const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Got " << properties.size() << " properties.";
      std::string output{};   // Initial output Bios Version as Null.
      PropertiesType::const_iterator it = properties.find("BiosVersion");
      if (it != properties.end()) {
        const std::string *s =
              boost::get<std::string>(&it->second);
        if (nullptr != s) {
          CROW_LOG_DEBUG << "Found BiosVersion: " << *s;
          output = *s;
        }
      }
      // Update JSON payload with Bios Version information.
      asyncResp->res.json_value["BiosVersion"] = output;
    };
    // Make call to Host service.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objHostUpdater,
                                                     ifaceName);
  }

  /**
  * Function that retrieves all properties of processor interface.
  * @param[in] asyncResp     Shared pointer for completing asynchronous calls.
  * @return None.
  */
  void getProcessorSummary(const std::shared_ptr<AsyncResp> &asyncResp) {
    CROW_LOG_DEBUG << "Get processor summary object...";
    const dbus::endpoint objHostUpdater = {
      "xyz.openbmc_project.Software.Host.Updater",
      "/xyz/openbmc_project/software/host/inventory",
      "org.freedesktop.DBus.Properties", "GetAll"};
    const std::string ifaceName = "xyz.openbmc_project.Software.Host.Processor";

    auto resp_handler = [ asyncResp ](
                            const boost::system::error_code ec,
                            const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Got " << properties.size() << " properties.";

      // Prepare all the schema required fields which retrieved from D-Bus.
      for (const char *p :
           std::array<const char *, 4>
               {"Count",
                "Model",
                "State",
                "Health"}) {
        PropertiesType::const_iterator it = properties.find(p);
        if (it != properties.end()) {
          if (p == "Count") {
            const uint32_t *count = boost::get<uint32_t>(&it->second);
            if (count != nullptr)
              asyncResp->res.json_value["ProcessorSummary"]["Count"] = *count;
          } else {
            const std::string *s = boost::get<std::string>(&it->second);
            if (s != nullptr) {
              if (p == "State" || p == "Health")
                asyncResp->res.json_value["ProcessorSummary"]["Status"][p] = *s;
              else
                asyncResp->res.json_value["ProcessorSummary"][p] = *s;
            }
          }
        }
      }

    };
     // Make call to Host service.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objHostUpdater,
                                                     ifaceName);
  }

  /**
  * Function that retrieves all properties of memory interface.
  * @param[in] asyncResp     Shared pointer for completing asynchronous calls.
  * @return None.
  */
  void getMemorySummary(const std::shared_ptr<AsyncResp> &asyncResp) {
    CROW_LOG_DEBUG << "Get memory summary object...";
    const dbus::endpoint objHostUpdater = {
      "xyz.openbmc_project.Software.Host.Updater",
      "/xyz/openbmc_project/software/host/inventory",
      "org.freedesktop.DBus.Properties", "GetAll"};
    const std::string ifaceName = "xyz.openbmc_project.Software.Host.Memory";

    auto resp_handler = [ asyncResp ](
                            const boost::system::error_code ec,
                            const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Got " << properties.size() << " properties.";

      // Prepare all the schema required fields which retrieved from D-Bus.
      for (const char *p :
           std::array<const char *, 3>
               {"TotalSystemMemoryGiB",
                "State",
                "Health"}) {
        PropertiesType::const_iterator it = properties.find(p);
        if (it != properties.end()) {
          if (p == "TotalSystemMemoryGiB") {
            const uint32_t *total = boost::get<uint32_t>(&it->second);
            if (total != nullptr)
              asyncResp->res.json_value["MemorySummary"]
                                       ["TotalSystemMemoryGiB"] = *total;
          } else {
            const std::string *s = boost::get<std::string>(&it->second);
            if (s != nullptr)
                asyncResp->res.json_value["MemorySummary"]["Status"][p] = *s;
          }
        }
      }

    };
     // Make call to Host service.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objHostUpdater,
                                                     ifaceName);
  }

  /**
  * Function that retrieves all properties of boot interface.
  * @param[in] asyncResp     Shared pointer for completing asynchronous calls.
  * @return None.
  */
  void getBootPolicy(const std::shared_ptr<AsyncResp> &asyncResp) {
    CROW_LOG_DEBUG << "Get boot object...";
    const dbus::endpoint objHostUpdater = {
      "xyz.openbmc_project.Software.Host.Updater",
      "/xyz/openbmc_project/software/host/inventory",
      "org.freedesktop.DBus.Properties", "GetAll"};
    const std::string ifaceName = "xyz.openbmc_project.Software.Host.Boot";

    auto resp_handler = [ asyncResp ](
                            const boost::system::error_code ec,
                            const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Got " << properties.size() << " properties.";

      // Prepare all the schema required fields which retrieved from D-Bus.
      for (const char *p :
           std::array<const char *, 2>
               {"BootSourceOverrideEnabled",
                "BootSourceOverrideTarget"}) {
        PropertiesType::const_iterator it = properties.find(p);
        if (it != properties.end()) {
           const std::string *s = boost::get<std::string>(&it->second);
            if (s != nullptr)
                asyncResp->res.json_value["Boot"][p] = *s;
        }
      }

    };
     // Make call to Host service.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objHostUpdater,
                                                     ifaceName);
  }

  /**
   * @brief Retrieves System Unique ID (UUID) properties over D-Bus.
   *
   * @param[in] asyncResp  Shared pointer for completing asynchronous calls.
   *
   * @return None.
   */
  void getSystemUniqueID(const std::shared_ptr<AsyncResp> &asyncResp) {
    CROW_LOG_DEBUG << "Get System Unique ID properties.";
    const dbus::endpoint objFRUManager = {
      "xyz.openbmc_project.Inventory.FRU",
      "/xyz/openbmc_project/inventory/fru0/multirecord",
      "org.freedesktop.DBus.Properties", "GetAll"};
    const std::string ifaceName =
      "xyz.openbmc_project.Inventory.FRU.MultiRecord";

    auto resp_handler = [ asyncResp ](
                            const boost::system::error_code ec,
                            const PropertiesType &properties) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Got " << properties.size() << " properties.";
      std::string output{};   // Initial output UUID as Null.
      PropertiesType::const_iterator it = properties.find("Record_1");
      if (it != properties.end()) {
        const std::string *s =
              boost::get<std::string>(&it->second);
        if (nullptr != s) {
          CROW_LOG_DEBUG << "Found UUID: " << *s;
          output = *s;
        }
      }
      // Update JSON payload with UUID information.
      asyncResp->res.json_value["UUID"] = output;
    };
    // Make call to Host service.
    crow::connections::system_bus->async_method_call(resp_handler,
                                                     objFRUManager,
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
 * SystemActionsReset class supports handle POST method for Reset action.
 * The class retrieves and sends data directly to D-Bus.
 */
class SystemActionsReset : public Node {
 public:
  SystemActionsReset(CrowApp& app)
      : Node(app, "/redfish/v1/Systems/1/Actions/ComputerSystem.Reset/") {

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
   * SystemActionsReset supports for POST method,
   * it is not required to retrieve more information in GET.
   */
  void doGet(crow::response& res, const crow::request& req,
             const std::vector<std::string>& params) override {
    res.json_value = Node::json;
    res.end();
  }

  /**
   * Function handles POST method request.
   * Analyzes POST body message before sends Reset request data to D-Bus.
   */
  void doPost(crow::response& res, const crow::request& req,
             const std::vector<std::string>& params) override {
    // Parse JSON request body.
    nlohmann::json post;
    if (!json_util::processJsonFromRequest(res, req, post)) {
      return;
    }
    // Find key "ResetType" with reset request type
    const std::string *reqResetType = nullptr;
    json_util::Result r = json_util::getString(
        "ResetType", post, reqResetType,
        static_cast<int>(json_util::MessageSetting::TYPE_ERROR) |
            static_cast<int>(json_util::MessageSetting::MISSING),
        res.json_value, std::string("/ResetType"));
    if ((r != json_util::Result::SUCCESS) || (reqResetType == nullptr)) {
      res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
      res.end();
      return;
    }
    // Verify key value
    if (std::find(provider.allowedResetType.begin(),
                  provider.allowedResetType.end(), *reqResetType) ==
        provider.allowedResetType.end()) {
      res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
      res.end();
      return;
    }

    // Define metadata.
    const dbus::endpoint objChassis = {
        "xyz.openbmc_project.State.Chassis",
        "/xyz/openbmc_project/state/chassis0",
        "org.freedesktop.DBus.Properties", "Set"};
    const dbus::endpoint objHost = {
        "xyz.openbmc_project.State.Host",
        "/xyz/openbmc_project/state/host0",
        "org.freedesktop.DBus.Properties", "Set"};
    const std::string ifaceChassis{"xyz.openbmc_project.State.Chassis"};
    const std::string ifaceHost{"xyz.openbmc_project.State.Host"};
    const std::string argChassis{"RequestedPowerTransition"};
    const std::string argHost{"RequestedHostTransition"};
    const std::string prefixArgChassis{
                              "xyz.openbmc_project.State.Chassis.Transition."};
    const std::string prefixArgHost{
                              "xyz.openbmc_project.State.Host.Transition."};

    auto asyncResp = std::make_shared<AsyncResp>(res);
    // Execute Reset Action regarding to each reset type.
    if (*reqResetType == "On") {
      exeActionsReset(asyncResp, objHost, ifaceHost, argHost,
                      dbus::dbus_variant(prefixArgHost + "On"));
    } else if (*reqResetType == "GracefulShutdown") {
      exeActionsReset(asyncResp, objHost, ifaceHost, argHost,
                      dbus::dbus_variant(prefixArgHost + "Off"));
    } else if (*reqResetType == "GracefulRestart") {
      exeActionsReset(asyncResp, objHost, ifaceHost, argHost,
                      dbus::dbus_variant(prefixArgHost + "Reboot"));
    } else if (*reqResetType == "ForceOff") {
      exeActionsReset(asyncResp, objChassis, ifaceChassis, argChassis,
                      dbus::dbus_variant(prefixArgChassis + "Off"));
    } else if (*reqResetType == "ForceRestart") {
      exeActionsReset(asyncResp, objChassis, ifaceChassis, argChassis,
                      dbus::dbus_variant(prefixArgChassis + "Reboot"));
    }
  }

  /**
   * Function sends data to D-Bus directly.
   * TODO consider to make it general.
   */
  void exeActionsReset(const std::shared_ptr<AsyncResp> &asyncResp,
                       const dbus::endpoint &endpoint,
                       const std::string &interface,
                       const std::string &argument,
                       const dbus::dbus_variant &variant) {
    crow::connections::system_bus->async_method_call(
      [ asyncResp ](const boost::system::error_code ec) {
        if (ec) {
          CROW_LOG_ERROR << "D-Bus responses error: " << ec;
          asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
          return;
        }
        // TODO Consider support polling mechanism to verify status of host and
        // chassis after execute the requested action.
        CROW_LOG_DEBUG << "Response with no content";
        asyncResp->res.code = static_cast<int>(HttpRespCode::NO_CONTENT);
      },
      endpoint, interface, argument, variant);
  }

  OnDemandComputerSystemProvider provider;
};

/**
* Systems derived class for delivering Computer Systems Schema.
*/
class Systems : public Node {
 public:
  template <typename CrowApp>
  Systems(CrowApp &app)
       : Node(app, "/redfish/v1/Systems/<str>/", std::string()),
         memberActionsReset(app) {
     Node::json["@odata.type"] = "#ComputerSystem.v1_5_0.ComputerSystem";
     Node::json["@odata.context"] =
                  "/redfish/v1/$metadata#ComputerSystem.ComputerSystem";
     Node::json["Name"] = "Computer System";

     Node::json["SystemType"] = "Physical";
     Node::json["Description"] = "Computer System";
     Node::json["LogServices"] =
                     {{"@odata.id", "/redfish/v1/Systems/1/LogServices"}};
     Node::json["Links"]["Chassis"] =
                    {{{"@odata.id", "/redfish/v1/Chassis/1"}}};
     Node::json["Links"]["ManagedBy"] =
                    {{{"@odata.id", "/redfish/v1/Managers/bmc"}}};
     Node::json["SKU"] = ""; // TODO Not supported in D-Bus yet.
     Node::json["Status"]["Health"] = "OK"; // Resource can response so assume
                                            // default Health state is Ok.

     Node::json["Actions"]["#ComputerSystem.Reset"] =
      {{"target", "/redfish/v1/Systems/1/Actions/ComputerSystem.Reset"},
       {"ResetType@Redfish.AllowableValues", provider.allowedResetType}};
     Node::json["Boot"]["BootSourceOverrideTarget@Redfish.AllowableValues"] =
       provider.allowedBootSourceTarget;

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
    // Get system id
    const std::string &id = params[0];
    if (id != "1") {
      // only 1 System for now
      res.code = static_cast<int>(HttpRespCode::NOT_FOUND);
      res.end();
      return;
    }

    res.json_value = Node::json;

    res.json_value["Id"] = id;
    res.json_value["@odata.id"] = "/redfish/v1/Systems/" + id;

    // Create asyncResp pointer to object holding the response data
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
    asyncResp->res.json_value["HostName"] = provider.getHostName();

    // Get IndicatorLED property
    provider.getLedGroupIdentify(
        asyncResp, [&](const bool &asserted,
                       const std::shared_ptr<AsyncResp> &asyncResp) {
          if (asserted) {
            // If led group is asserted, then another call is needed to
            // get led status.
            provider.getLedIdentify(
              asyncResp, [](const std::string &ledStatus,
                            const std::shared_ptr<AsyncResp> &asyncResp) {
                if (!ledStatus.empty()) {
                  asyncResp->res.json_value["IndicatorLED"] = ledStatus;
                }
              });
          } else {
            asyncResp->res.json_value["IndicatorLED"] = "Off";
          }
        });

    // Get BiosVersion property
    provider.getBiosVersion(asyncResp);
    // Get Processor property
    provider.getProcessorSummary(asyncResp);
    // Get Memory Summary property
    provider.getMemorySummary(asyncResp);
    // Get Boot Override Policy
    provider.getBootPolicy(asyncResp);
    // Get System UUID
    provider.getSystemUniqueID(asyncResp);
  }

  /**
   * Function handles PATCH request before
   * triggers appropriate requests on D-Bus to change resource's property.
   */
  void doPatch(crow::response &res, const crow::request &req,
               const std::vector<std::string> &params) override {
    // Check if there is required param, truly entering this shall be
    // impossible
    if (params.size() != 1) {
      res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
      res.end();
      return;
    }
    // Parse JSON request body
    nlohmann::json patch;
    if (!json_util::processJsonFromRequest(res, req, patch)) {
      return;
    }

    for (auto propertyIt = patch.begin(); propertyIt != patch.end();
         ++propertyIt) {
      if (propertyIt.key() == "IndicatorLED") {
        changeIndicatorLEDState(res, patch, params);
      } else if (propertyIt.key() == "BootSourceOverrideEnabled" ||
                 propertyIt.key() == "BootSourceOverrideTarget") {
        changeBootSourceOverridePolicy(res, patch, propertyIt.key(), params);
      } else {
        // User attempted to modify non-writable field
        messages::addMessageToJsonRoot(
            res.json_value,
            messages::propertyNotWritable(propertyIt.key()));
      }
    }
  }

  void changeBootSourceOverridePolicy(crow::response &res,
                                      const nlohmann::json& reqJson,
                                      const std::string &key,
                                      const std::vector<std::string> &params) {
    CROW_LOG_DEBUG << "Request to change Boot Source Override policy";
    // Find key with new override policy
    const std::string &name = params[0];
    const std::string *reqOverridePolicy = nullptr;
    json_util::Result r = json_util::getString(
        key.c_str(), reqJson, reqOverridePolicy,
        static_cast<int>(json_util::MessageSetting::TYPE_ERROR) |
            static_cast<int>(json_util::MessageSetting::MISSING),
        res.json_value, std::string("/" + name +
                                    "Boot/" + key));
    if ((r != json_util::Result::SUCCESS) || (reqOverridePolicy == nullptr)) {
      res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
      res.end();
      return;
    }

    // List of allowed policies overriding boot source.
    // TODO These should be retrieved from D-Bus.
    const std::vector<std::string> allowedPolicy{
      "Disabled", // The system will boot normally.
      "Once", // On its next boot cycle, the system will boot (one time)
              // to the Boot Source Override Target.
              // The value of BootSourceOverrideEnabled is then
              // reset back to Disabled.
      "Continuous"}; // The system will boot to the target specified in
                     // the BootSourceOverrideTarget until
                     // this property is set to Disabled.

    // Verify request value
    if (key == "BootSourceOverrideTarget") {
      auto it = std::find(provider.allowedBootSourceTarget.begin(),
                          provider.allowedBootSourceTarget.end(),
                          *reqOverridePolicy);
      if (it == provider.allowedBootSourceTarget.end()) {
        res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
        res.end();
        return;
      }
    } else if (key == "BootSourceOverrideEnabled") {
      auto it = std::find(allowedPolicy.begin(), allowedPolicy.end(),
                          *reqOverridePolicy);
      if (it == allowedPolicy.end()) {
        res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
        res.end();
        return;
      }
    }

    auto asyncResp = std::make_shared<AsyncResp>(res);
    res.json_value = Node::json;
    res.json_value["@odata.id"] = "/redfish/v1/Systems/" + name;

    // Get Boot Override Policy
    provider.getBootPolicy(asyncResp);

    const dbus::endpoint objHostUpdater = {
      "xyz.openbmc_project.Software.Host.Updater",
      "/xyz/openbmc_project/software/host/inventory",
      "org.freedesktop.DBus.Properties", "Set"};
    const std::string ifaceName = "xyz.openbmc_project.Software.Host.Boot";
    const std::string reqValue = *reqOverridePolicy;

    auto resp_handler = [key{std::move(key)}, reqValue{std::move(reqValue)},
                         asyncResp{std::move(asyncResp)}](
        const boost::system::error_code ec) {
      if (ec) {
        CROW_LOG_ERROR << "D-Bus response error " << ec;
        asyncResp->res.code = static_cast<int>(HttpRespCode::INTERNAL_ERROR);
        return;
      }
      CROW_LOG_DEBUG << "Boot Source Override policy update done.";
      asyncResp->res.json_value["Boot"][key] = std::move(reqValue);
    };
    crow::connections::system_bus->async_method_call(resp_handler,
        objHostUpdater, ifaceName, key,
        dbus::dbus_variant(*reqOverridePolicy));
  }

  void changeIndicatorLEDState(crow::response &res,
                               const nlohmann::json& reqJson,
                               const std::vector<std::string> &params) {
    // Find key with new led value
    const std::string &name = params[0];
    const std::string *reqLedState = nullptr;
    json_util::Result r = json_util::getString(
        "IndicatorLED", reqJson, reqLedState,
        static_cast<int>(json_util::MessageSetting::TYPE_ERROR) |
            static_cast<int>(json_util::MessageSetting::MISSING),
        res.json_value, std::string("/" + name + "/IndicatorLED"));
    if ((r != json_util::Result::SUCCESS) || (reqLedState == nullptr)) {
      res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
      res.end();
      return;
    }
    // Verify key value
    std::string dbusLedState;
    for (const auto &p : boost::container::flat_map<const char *, const char *>{
             {"On", "Lit"}, {"Blink", "Blinking"}, {"Off", "Off"}}) {
      if (*reqLedState == p.second) {
        dbusLedState = p.first;
      }
    }

    // Update led status
    auto asyncResp = std::make_shared<AsyncResp>(res);
    res.json_value = Node::json;
    res.json_value["@odata.id"] = "/redfish/v1/Systems/" + name;

    provider.getHostState(asyncResp);
    // TODO Need to refactor getComputerSystem method.
    const std::array<const std::string, 3> arrayIfaceName
    {"xyz.openbmc_project.Inventory.Decorator.Asset",
     "xyz.openbmc_project.Inventory.Item",
     "xyz.openbmc_project.Inventory.Decorator.AssetTag"};
    for (const std::string ifaceName : arrayIfaceName) {
      provider.getComputerSystem(asyncResp, ifaceName);
    }

    if (dbusLedState.empty()) {
      CROW_LOG_ERROR << "Bad Request Led state: " << *reqLedState;
      asyncResp->res.code = static_cast<int>(HttpRespCode::BAD_REQUEST);
    } else {
      // Update led group
      CROW_LOG_DEBUG << "Update led group.";
      crow::connections::system_bus->async_method_call(
          [&, asyncResp{std::move(asyncResp)} ](
              const boost::system::error_code ec) {
            if (ec) {
              CROW_LOG_DEBUG << "DBUS response error " << ec;
              asyncResp->res.code =
                                static_cast<int>(HttpRespCode::INTERNAL_ERROR);
              return;
            }
            CROW_LOG_DEBUG << "Led group update done.";
          },
          {"xyz.openbmc_project.LED.GroupManager",
          "/xyz/openbmc_project/led/groups/enclosure_identify",
          "org.freedesktop.DBus.Properties", "Set"},
          "xyz.openbmc_project.Led.Group", "Asserted",
          dbus::dbus_variant((dbusLedState == "Off" ? false : true)));
      // Update identify led status
      CROW_LOG_DEBUG << "Update led SoftwareInventoryCollection.";
      crow::connections::system_bus->async_method_call(
          [&, asyncResp{std::move(asyncResp)} ](
              const boost::system::error_code ec) {
            if (ec) {
              CROW_LOG_DEBUG << "DBUS response error " << ec;
              asyncResp->res.code =
                                static_cast<int>(HttpRespCode::INTERNAL_ERROR);
              return;
            }
            CROW_LOG_DEBUG << "Led state update done.";
            res.json_value["IndicatorLED"] = *reqLedState;
          },
          {"xyz.openbmc_project.LED.Controller.identify",
          "/xyz/openbmc_project/led/physical/identify",
          "org.freedesktop.DBus.Properties", "Set"},
          "xyz.openbmc_project.Led.Physical", "State",
          dbus::dbus_variant(
              "xyz.openbmc_project.Led.Physical.Action." + dbusLedState));
    }
  }

  // Reset actions as member of System resource.
  // Handle POST action for specified reset type.
  SystemActionsReset memberActionsReset;
  // Computer System Provider object.
  // TODO Consider to move it to singleton.
  OnDemandComputerSystemProvider provider;

};

}  // namespace redfish
