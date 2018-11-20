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

namespace redfish
{

class AmpereComputing : public Node
{
  public:
    AmpereComputing(CrowApp& app) : Node(app, "/redfish/v1/AmpereComputing")
    {
        Node::json["@odata.type"] = "#AmpereComputing.v1_0_0.AmpereComputing";
        Node::json["@odata.id"] = "/redfish/v1/AmpereComputing";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#AmpereComputing.AmpereComputing";
        Node::json["Id"] = "AmpereComputing";
        Node::json["Name"] = "Ampere Computing Service";
        Node::json["UploadService"] =
            {{"@odata.id", "/redfish/v1/AmpereComputing/UploadService"}};

        entityPrivileges = {
            {boost::beast::http::verb::get, {}},
            {boost::beast::http::verb::head, {}},
            {boost::beast::http::verb::patch, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::put, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureComponents"}}},
            {boost::beast::http::verb::post, {{"ConfigureComponents"}}}};
    }

  private:
    void doGet(crow::Response& res, const crow::Request& req,
               const std::vector<std::string>& params) override
    {
        res.jsonValue = Node::json;
        res.end();
    }
};

} // namespace redfish
