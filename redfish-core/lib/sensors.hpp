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

#include <math.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/range/algorithm/replace_copy_if.hpp>
#include <dbus_singleton.hpp>

namespace redfish
{

#ifdef OCP_CUSTOM_FLAG // Remove Entity-Manager object
constexpr const char* DBUS_SENSOR_PREFIX = "/xyz/openbmc_project/sensors/";
#else
constexpr const char* DBUS_SENSOR_PREFIX = "/xyz/openbmc_project/Sensors/";
#endif // OCP_CUSTOM_FLAG

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

using SensorVariant = sdbusplus::message::variant<int64_t, double>;

using ManagedObjectsVectorType = std::vector<std::pair<
    sdbusplus::message::object_path,
    boost::container::flat_map<
        std::string, boost::container::flat_map<std::string, SensorVariant>>>>;

/**
 * SensorAsyncResp
 * Gathers data needed for response processing after async calls are done
 */
class SensorAsyncResp
{
  public:
    SensorAsyncResp(crow::Response& response, const std::string& chassisId,
#ifdef OCP_CUSTOM_FLAG // Add specific sub-Node
                    const std::initializer_list<const char*> types,
                    const std::string& subNode) :
        chassisId(chassisId),
        res(response), types(types), chassisSubNode(subNode)
    {
#else
                    const std::initializer_list<const char*> types) :
        chassisId(chassisId),
        res(response), types(types)
    {
        res.jsonValue["@odata.id"] =
            "/redfish/v1/Chassis/" + chassisId + "/Thermal";
#endif // OCP_CUSTOM_FLAG
    }

    ~SensorAsyncResp()
    {
        if (res.result() == boost::beast::http::status::internal_server_error)
        {
            // Reset the json object to clear out any data that made it in
            // before the error happened todo(ed) handle error condition with
            // proper code
            res.jsonValue = nlohmann::json::object();
        }
        res.end();
    }

    void setErrorStatus()
    {
        res.result(boost::beast::http::status::internal_server_error);
    }

    std::string chassisId{};
    crow::Response& res;
    const std::vector<const char*> types;
#ifdef OCP_CUSTOM_FLAG
    std::string chassisSubNode{};
#endif // OCP_CUSTOM_FLAG
};

/**
 * @brief Creates connections necessary for chassis sensors
 * @param SensorsAsyncResp Pointer to object holding response data
 * @param sensorNames Sensors retrieved from chassis
 * @param callback Callback for processing gathered connections
 */
template <typename Callback>
void getConnections(const std::shared_ptr<SensorAsyncResp>& sensorAsyncResp,
#ifndef OCP_CUSTOM_FLAG
                    const boost::container::flat_set<std::string>& sensorNames,
#endif
                    Callback&& callback)
{
    BMCWEB_LOG_DEBUG << "getConnections";
    const std::string path = "/xyz/openbmc_project/sensors";
    const std::array<std::string, 1> interfaces = {
        "xyz.openbmc_project.Sensor.Value"};

    // Response handler for parsing objects subtree
    auto respHandler = [callback{std::move(callback)}, sensorAsyncResp
#ifdef OCP_CUSTOM_FLAG
                        ](const boost::system::error_code ec,
#else
                        sensorNames](const boost::system::error_code ec,
#endif
                                     const GetSubTreeType& subtree) {
        if (ec != 0)
        {
            sensorAsyncResp->setErrorStatus();
            BMCWEB_LOG_ERROR << "Dbus error " << ec;
            return;
        }

        BMCWEB_LOG_DEBUG << "Found " << subtree.size() << " subtrees";

        // Make unique list of connections only for requested sensor types and
        // found in the chassis
        boost::container::flat_set<std::string> connections;
        // Intrinsic to avoid malloc.  Most systems will have < 8 sensor
        // producers
        connections.reserve(8);

#ifndef OCP_CUSTOM_FLAG
        BMCWEB_LOG_DEBUG << "sensorNames list cout: " << sensorNames.size();
        for (const std::string& tsensor : sensorNames)
        {
            BMCWEB_LOG_DEBUG << "Sensor to find: " << tsensor;
        }
#endif

        for (const std::pair<
                 std::string,
                 std::vector<std::pair<std::string, std::vector<std::string>>>>&
                 object : subtree)
        {
            for (const char* type : sensorAsyncResp->types)
            {
                if (boost::starts_with(object.first, type))
                {
                    auto lastPos = object.first.rfind('/');
                    if (lastPos != std::string::npos)
                    {
#ifdef OCP_CUSTOM_FLAG // Remove unused parameter: sensorNames.
                        // Add connection list.
                        for (const std::pair<std::string,
                                             std::vector<std::string>>& objData
                                    : object.second)
                        {
                            BMCWEB_LOG_DEBUG << "objData.first: "
                                             << objData.first;
                            connections.insert(objData.first);
                        }
#else
                        std::string sensorName =
                            object.first.substr(lastPos + 1);

                        if (sensorNames.find(sensorName) != sensorNames.end())
                        {
                            // For each connection name
                            for (const std::pair<std::string,
                                                 std::vector<std::string>>&
                                     objData : object.second)
                            {
                                connections.insert(objData.first);
                            }
                        }
#endif
                    }
                    break;
                }
            }
        }

        BMCWEB_LOG_DEBUG << "Found " << connections.size() << " connections";
        callback(std::move(connections));
        BMCWEB_LOG_DEBUG << "getConnections respHandler exit";
    };

    // Make call to ObjectMapper to find all sensors objects
    crow::connections::systemBus->async_method_call(
        std::move(respHandler), "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", path, 2, interfaces);
    BMCWEB_LOG_DEBUG << "getConnections exit";
}

#ifndef OCP_CUSTOM_FLAG // Comment out getChassis method
                        // Unuse Entity-Manager object
/**
 * @brief Retrieves requested chassis sensors and redundancy data from DBus .
 * @param SensorAsyncResp   Pointer to object holding response data
 * @param callback  Callback for next step in gathered sensor processing
 */
template <typename Callback>
void getChassis(std::shared_ptr<SensorAsyncResp> sensorAsyncResp,
                Callback&& callback)
{
    BMCWEB_LOG_DEBUG << "getChassis enter";
    // Process response from EntityManager and extract chassis data
    auto respHandler = [callback{std::move(callback)},
                        sensorAsyncResp](const boost::system::error_code ec,
                                         ManagedObjectsVectorType& resp) {
        BMCWEB_LOG_DEBUG << "getChassis respHandler enter";
        if (ec)
        {
            BMCWEB_LOG_ERROR << "getChassis respHandler DBUS error: " << ec;
            sensorAsyncResp->setErrorStatus();
            return;
        }
        boost::container::flat_set<std::string> sensorNames;

        //   sensorAsyncResp->chassisId
        bool foundChassis = false;
        std::vector<std::string> split;
        // Reserve space for
        // /xyz/openbmc_project/inventory/<name>/<subname> + 3 subnames
        split.reserve(8);

        for (const auto& objDictEntry : resp)
        {
            const std::string& objectPath =
                static_cast<const std::string&>(objDictEntry.first);
            boost::algorithm::split(split, objectPath, boost::is_any_of("/"));
            if (split.size() < 2)
            {
                BMCWEB_LOG_ERROR << "Got path that isn't long enough "
                                 << objectPath;
                split.clear();
                continue;
            }
            const std::string& sensorName = split.end()[-1];
            const std::string& chassisName = split.end()[-2];

            if (chassisName != sensorAsyncResp->chassisId)
            {
                split.clear();
                continue;
            }
            BMCWEB_LOG_DEBUG << "New sensor: " << sensorName;
            foundChassis = true;
            sensorNames.emplace(sensorName);
            split.clear();
        };
        BMCWEB_LOG_DEBUG << "Found " << sensorNames.size() << " Sensor names";

        if (!foundChassis)
        {
            BMCWEB_LOG_INFO << "Unable to find chassis named "
                            << sensorAsyncResp->chassisId;
            sensorAsyncResp->res.result(boost::beast::http::status::not_found);
        }
        else
        {
            callback(sensorNames);
        }
        BMCWEB_LOG_DEBUG << "getChassis respHandler exit";
    };

    // Make call to EntityManager to find all chassis objects
    crow::connections::systemBus->async_method_call(
        respHandler, "xyz.openbmc_project.EntityManager", "/",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    BMCWEB_LOG_DEBUG << "getChassis exit";
}
#endif // OCP_CUSTOM_FLAG

/**
 * @brief Builds a json sensor representation of a sensor.
 * @param sensorName  The name of the sensor to be built
 * @param sensorType  The type (temperature, fan_tach, etc) of the sensor to
 * build
 * @param interfacesDict  A dictionary of the interfaces and properties of said
 * interfaces to be built from
 * @param sensor_json  The json object to fill
 */
void objectInterfacesToJson(
    const std::string& sensorName, const std::string& sensorType,
    const boost::container::flat_map<
        std::string, boost::container::flat_map<std::string, SensorVariant>>&
        interfacesDict,
    nlohmann::json& sensor_json)
{
    // We need a value interface before we can do anything with it
    auto valueIt = interfacesDict.find("xyz.openbmc_project.Sensor.Value");
    if (valueIt == interfacesDict.end())
    {
        BMCWEB_LOG_ERROR << "Sensor doesn't have a value interface";
        return;
    }

    // Assume values exist as is (10^0 == 1) if no scale exists
    int64_t scaleMultiplier = 0;

    auto scaleIt = valueIt->second.find("Scale");
    // If a scale exists, pull value as int64, and use the scaling.
    if (scaleIt != valueIt->second.end())
    {
        const int64_t* int64Value =
            mapbox::getPtr<const int64_t>(scaleIt->second);
        if (int64Value != nullptr)
        {
            scaleMultiplier = *int64Value;
        }
    }

    sensor_json["MemberId"] = sensorName;
    sensor_json["Name"] = sensorName;
    sensor_json["Status"]["State"] = "Enabled";
    sensor_json["Status"]["Health"] = "OK";

    // Parameter to set to override the type we get from dbus, and force it to
    // int, regardless of what is available.  This is used for schemas like fan,
    // that require integers, not floats.
    bool forceToInt = false;

    const char* unit = "Reading";
    if (sensorType == "temperature")
    {
        unit = "ReadingCelsius";
        sensor_json["@odata.type"] = "#Thermal.v1_3_0.Temperature";
        // TODO(ed) Documentation says that path should be type fan_tach,
        // implementation seems to implement fan
    }
    else if (sensorType == "fan" || sensorType == "fan_tach")
    {
        unit = "Reading";
        sensor_json["ReadingUnits"] = "RPM";
        sensor_json["@odata.type"] = "#Thermal.v1_3_0.Fan";
        forceToInt = true;
    }
    else if (sensorType == "voltage")
    {
        unit = "ReadingVolts";
        sensor_json["@odata.type"] = "#Power.v1_0_0.Voltage";
    }
    else if (sensorType == "power" || sensorType == "current")
    {
        unit = "LastPowerOutputWatts";
        sensor_json["@odata.type"] = "#Power.v1_5_0.PowerSupply";
    }
    else
    {
        BMCWEB_LOG_ERROR << "Redfish cannot map object type for " << sensorName;
        return;
    }
    // Map of dbus interface name, dbus property name and redfish property_name
    std::vector<std::tuple<const char*, const char*, const char*>> properties;
    properties.reserve(8);

    properties.emplace_back("xyz.openbmc_project.Sensor.Value", "Value", unit);
    properties.emplace_back("xyz.openbmc_project.Sensor.Threshold.Warning",
                            "WarningHigh", "UpperThresholdNonCritical");
    properties.emplace_back("xyz.openbmc_project.Sensor.Threshold.Warning",
                            "WarningLow", "LowerThresholdNonCritical");
    properties.emplace_back("xyz.openbmc_project.Sensor.Threshold.Critical",
                            "CriticalHigh", "UpperThresholdCritical");
    properties.emplace_back("xyz.openbmc_project.Sensor.Threshold.Critical",
                            "CriticalLow", "LowerThresholdCritical");
    properties.emplace_back("xyz.openbmc_project.Sensor.Threshold.Fatal",
                            "FatalHigh", "UpperThresholdFatal");
    properties.emplace_back("xyz.openbmc_project.Sensor.Threshold.Fatal",
                            "FatalLow", "LowerThresholdFatal");

    if (sensorType == "temperature")
    {
        properties.emplace_back("xyz.openbmc_project.Sensor.Value", "MinValue",
                                "MinReadingRangeTemp");
        properties.emplace_back("xyz.openbmc_project.Sensor.Value", "MaxValue",
                                "MaxReadingRangeTemp");
        properties.emplace_back("xyz.openbmc_project.Sensor.Value", "SensorID",
                                "SensorNumber");
    }
    else if (sensorType == "voltage")
    {
        properties.emplace_back("xyz.openbmc_project.Sensor.Value", "SensorID",
                                "SensorNumber");
    }
    else if (sensorType == "power")
    {
        // TODO Power schema does not include MinReadingRange/MaxReadingRange,
        // This should be replaced by PowerInputWatts/PowerOutputWatts...
    }
    else
    {
        properties.emplace_back("xyz.openbmc_project.Sensor.Value", "MinValue",
                                "MinReadingRange");
        properties.emplace_back("xyz.openbmc_project.Sensor.Value", "MaxValue",
                                "MaxReadingRange");
    }

    for (const std::tuple<const char*, const char*, const char*>& p :
         properties)
    {
        auto interfaceProperties = interfacesDict.find(std::get<0>(p));
        if (interfaceProperties != interfacesDict.end())
        {
            auto valueIt = interfaceProperties->second.find(std::get<1>(p));
            if (valueIt != interfaceProperties->second.end())
            {
                const SensorVariant& valueVariant = valueIt->second;
                nlohmann::json& valueIt = sensor_json[std::get<2>(p)];

                // Attempt to pull the int64 directly
                const int64_t* int64Value =
                    mapbox::getPtr<const int64_t>(valueVariant);

                if (int64Value != nullptr)
                {
                    auto value = 0;

                    // Don't need to adjust the value of Sensor ID
                    if (std::get<1>(p) == "SensorID")
                    {
                        value = *int64Value;
                    }
                    else
                    {
                        value = *int64Value * std::pow(10, scaleMultiplier);
                    }

                    if (forceToInt || scaleMultiplier >= 0)
                    {
                        valueIt = static_cast<int64_t>(value);
                    }
                    else
                    {
                        valueIt = value;
                    }
                }
                // Attempt to pull the float directly
                const double* doubleValue =
                    mapbox::getPtr<const double>(valueVariant);

                if (doubleValue != nullptr)
                {
                    auto value = *doubleValue * std::pow(10, scaleMultiplier);
                    if (!forceToInt)
                    {
                        valueIt = value;
                    }
                    else
                    {
                        valueIt = static_cast<int64_t>(value);
                    }
                }
            }
        }
    }
    BMCWEB_LOG_DEBUG << "Added sensor " << sensorName;
}

/**
 * @brief Entry point for retrieving sensors data related to requested
 *        chassis.
 * @param sensorAsyncResp   Pointer to object holding response data
 */
void getChassisData(std::shared_ptr<SensorAsyncResp> sensorAsyncResp)
{
    BMCWEB_LOG_DEBUG << "getChassisData enter";
#ifdef OCP_CUSTOM_FLAG // Remove unused parameter: sensorNames
    auto getConnectionCb = [&, sensorAsyncResp](
#else
    auto getChassisCb = [&, sensorAsyncResp](
        boost::container::flat_set<std::string>& sensorNames) {
        BMCWEB_LOG_DEBUG << "getChassisCb enter";
        auto getConnectionCb =
            [&, sensorAsyncResp, sensorNames](
#endif //OCP_CUSTOM_FLAG
                const boost::container::flat_set<std::string>& connections) {
                BMCWEB_LOG_DEBUG << "getConnectionCb enter";
                // Get managed objects from all services exposing sensors
                for (const std::string& connection : connections)
                {
                    // Response handler to process managed objects
#ifdef OCP_CUSTOM_FLAG
                    auto getManagedObjectsCb =
                        [&, sensorAsyncResp
                         ](const boost::system::error_code ec,
#else
                    auto getManagedObjectsCb =
                        [&, sensorAsyncResp,
                         sensorNames](const boost::system::error_code ec,
#endif
                                      ManagedObjectsVectorType& resp) {
                            BMCWEB_LOG_DEBUG << "getManagedObjectsCb enter";
                            if (ec)
                            {
                                BMCWEB_LOG_ERROR
                                    << "getManagedObjectsCb DBUS error: " << ec;
                                sensorAsyncResp->setErrorStatus();
                                return;
                            }
                            // Go through all objects and update response with
                            // sensor data
                            for (const auto& objDictEntry : resp)
                            {
                                const std::string& objPath =
                                    static_cast<const std::string&>(
                                        objDictEntry.first);
                                BMCWEB_LOG_DEBUG
                                    << "getManagedObjectsCb parsing object "
                                    << objPath;

                                std::vector<std::string> split;
                                // Reserve space for
                                // /xyz/openbmc_project/sensors/<name>/<subname>
                                split.reserve(6);
                                boost::algorithm::split(split, objPath,
                                                        boost::is_any_of("/"));
                                if (split.size() < 6)
                                {
                                    BMCWEB_LOG_ERROR
                                        << "Got path that isn't long enough "
                                        << objPath;
                                    continue;
                                }
                                // These indexes aren't intuitive, as
                                // boost::split puts an empty string at the
                                // beggining
                                const std::string& sensorType = split[4];
                                const std::string& sensorName = split[5];
                                BMCWEB_LOG_DEBUG << "sensorName " << sensorName
                                                 << " sensorType "
                                                 << sensorType;
#ifdef OCP_CUSTOM_FLAG
                                // Verify each object path
                                bool isMatchSensorType = false;
                                for (const char* type : sensorAsyncResp->types)
                                {
                                    // only accept the object path consists
                                    // requested type.
                                    if (strstr(type, sensorType.c_str()))
                                    {
                                        isMatchSensorType = true;
                                        break;
                                    }
                                }
                                if (!isMatchSensorType)
                                {
                                    BMCWEB_LOG_DEBUG << sensorType
                                                     << " is not requested";
                                    continue;
                                }
#else
                                if (sensorNames.find(sensorName) ==
                                    sensorNames.end())
                                {
                                    BMCWEB_LOG_ERROR << sensorName
                                                     << " not in sensor list ";
                                    continue;
                                }
#endif
                                const char* fieldName = nullptr;
                                if (sensorType == "temperature")
                                {
                                    fieldName = "Temperatures";
                                }
                                else if (sensorType == "fan" ||
                                         sensorType == "fan_tach")
                                {
                                    fieldName = "Fans";
                                }
                                else if (sensorType == "voltage")
                                {
                                    fieldName = "Voltages";
                                }
                                else if (sensorType == "current")
                                {
                                    fieldName = "PowerSupplies";
                                }
                                else if (sensorType == "power")
                                {
                                    fieldName = "PowerSupplies";
                                }
                                else
                                {
                                    BMCWEB_LOG_ERROR
                                        << "Unsure how to handle sensorType "
                                        << sensorType;
                                    continue;
                                }

                                nlohmann::json& tempArray =
                                    sensorAsyncResp->res.jsonValue[fieldName];

                                // Create the array if it doesn't yet exist
                                if (tempArray.is_array() == false)
                                {
                                    tempArray = nlohmann::json::array();
                                }

                                tempArray.push_back(nlohmann::json::object());
                                nlohmann::json& sensorJson = tempArray.back();
#ifdef OCP_CUSTOM_FLAG // @odata.id belongs to specific Chassis sub-node name.
                                sensorJson["@odata.id"] =
                                    "/redfish/v1/Chassis/" +
                                    sensorAsyncResp->chassisId + "/" +
                                    sensorAsyncResp->chassisSubNode + "#/" +
                                    sensorName;
#else
                                sensorJson["@odata.id"] =
                                    "/redfish/v1/Chassis/" +
                                    sensorAsyncResp->chassisId + "/Thermal#/" +
                                    sensorName;
#endif // OCP_CUSTOM_FLAG
                                objectInterfacesToJson(sensorName, sensorType,
                                                       objDictEntry.second,
                                                       sensorJson);
                            }
                            BMCWEB_LOG_DEBUG << "getManagedObjectsCb exit";
                        };
                    crow::connections::systemBus->async_method_call(
                        getManagedObjectsCb, connection,
                        "/xyz/openbmc_project/sensors",
                        "org.freedesktop.DBus.ObjectManager",
                        "GetManagedObjects");
                };
                BMCWEB_LOG_DEBUG << "getConnectionCb exit";
            };
        BMCWEB_LOG_DEBUG << "getChassisCb exit";
        // get connections and then pass it to get sensors
#ifdef OCP_CUSTOM_FLAG
        getConnections(sensorAsyncResp, std::move(getConnectionCb));
#else
        getConnections(sensorAsyncResp, sensorNames,
                       std::move(getConnectionCb));
    };
    // Get chassis information related to sensors
    getChassis(sensorAsyncResp, std::move(getChassisCb));
#endif // OCP_CUSTOM_FLAG
    BMCWEB_LOG_DEBUG << "getChassisData exit";
};

} // namespace redfish
