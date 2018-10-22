/*
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

using GetManagedObjectsType = boost::container::flat_map<
    sdbusplus::message::object_path,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<
            std::string, sdbusplus::message::variant<
                             std::string, bool, uint8_t, int16_t, uint16_t,
                             int32_t, uint32_t, int64_t, uint64_t, double>>>>;

/**
 * ProcessorCollection derived class for delivering Processor Collection Schema
 */
class ProcessorCollection : public Node
{
  public:
    template <typename CrowApp>
    ProcessorCollection(CrowApp &app) :
        Node(app, "/redfish/v1/Systems/1/Processors/")
    {
        Node::json["@odata.type"] =
            "#ProcessorCollection.ProcessorCollection";
        Node::json["@odata.id"] = "/redfish/v1/Systems/1/Processors";
        Node::json["@odata.context"] = "/redfish/v1/"
                                       "$metadata#ProcessorCollection."
                                       "ProcessorCollection";
        Node::json["Description"] =
            "Collection of processors for this system";
        Node::json["Name"] = "Processor Collection";
        // TODO: Add host processor info
        Node::json["Members@odata.count"] = 0;
        Node::json["Members"] = nlohmann::json::array();

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
