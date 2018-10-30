/*
// Copyright (c) 2018 Intel Corporation
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

class Roles : public Node
{
  public:
    Roles(CrowApp& app) :
        Node(app, "/redfish/v1/AccountService/Roles/<str>/", std::string())
    {
        Node::json["@odata.type"] = "#Role.v1_0_2.Role";
        Node::json["@odata.context"] = "/redfish/v1/$metadata#Role.Role";
        Node::json["Name"] = "User Role";
        Node::json["IsPredefined"] = true;
        Node::json["OemPrivileges"] = nlohmann::json::array();
        entityPrivileges = {
            {boost::beast::http::verb::get, {{"Login"}}},
            {boost::beast::http::verb::head, {{"Login"}}},
            {boost::beast::http::verb::patch, {{"ConfigureManager"}}},
            {boost::beast::http::verb::put, {{"ConfigureManager"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureManager"}}},
            {boost::beast::http::verb::post, {{"ConfigureManager"}}}};
    }

  private:
    void doGet(crow::Response& res, const crow::Request& req,
               const std::vector<std::string>& params) override
    {
        res.jsonValue = Node::json;
        auto asyncResp = std::make_shared<AsyncResp>(res);

        const std::string& role = params[0];
        if (role == "Administrator")
        {
            res.jsonValue["AssignedPrivileges"] = {
                "Login", "ConfigureManager", "ConfigureUsers", "ConfigureSelf",
                "ConfigureComponents"};
        }
        else if (role == "Operator")
        {
            res.jsonValue["AssignedPrivileges"] = {"Login", "ConfigureSelf",
                                                   "ConfigureComponents"};
        }
        else if (role == "ReadOnly")
        {
            res.jsonValue["AssignedPrivileges"] = {"Login", "ConfigureSelf"};
        }
        else if (role == "Callback")
        {
            res.jsonValue["AssignedPrivileges"] = {"Login", "ConfigureSelf"};
            res.jsonValue["IsPredefined"] = false;
        }
        else
        {
            res.result(boost::beast::http::status::not_found);
            return;
        }

        res.jsonValue["@odata.id"] = "/redfish/v1/AccountService/Roles/" + role;
        res.jsonValue["Id"] = role;
        res.jsonValue["Description"] = role + " User Role";
    }
};

class RoleCollection : public Node
{
  public:
    RoleCollection(CrowApp& app) :
        Node(app, "/redfish/v1/AccountService/Roles/")
    {
        Node::json["@odata.id"] = "/redfish/v1/AccountService/Roles";
        Node::json["@odata.type"] = "#RoleCollection.RoleCollection";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#RoleCollection.RoleCollection";
        Node::json["Name"] = "Roles Collection";
        Node::json["Description"] = "BMC User Roles";
        Node::json["Members@odata.count"] = 4;
        Node::json["Members"] = {
            {{"@odata.id", "/redfish/v1/AccountService/Roles/Administrator"}},
            {{"@odata.id", "/redfish/v1/AccountService/Roles/Callback"}},
            {{"@odata.id", "/redfish/v1/AccountService/Roles/Operator"}},
            {{"@odata.id", "/redfish/v1/AccountService/Roles/ReadOnly"}}};

        entityPrivileges = {
            {boost::beast::http::verb::get, {{"Login"}}},
            {boost::beast::http::verb::head, {{"Login"}}},
            {boost::beast::http::verb::patch, {{"ConfigureManager"}}},
            {boost::beast::http::verb::put, {{"ConfigureManager"}}},
            {boost::beast::http::verb::delete_, {{"ConfigureManager"}}},
            {boost::beast::http::verb::post, {{"ConfigureManager"}}}};
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
