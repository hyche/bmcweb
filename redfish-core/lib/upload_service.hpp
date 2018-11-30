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

#include <boost/container/flat_map.hpp>

namespace redfish
{

static bool fileUpload = true;

class UploadService : public Node
{
  public:
    UploadService(CrowApp &app) : Node(app, "/redfish/v1/AmpereComputing/UploadService/")
    {
        Node::json["@odata.type"] = "#UploadService.v1_0_0.UploadService";
        Node::json["@odata.id"] = "/redfish/v1/AmpereComputing/UploadService";
        Node::json["@odata.context"] =
            "/redfish/v1/$metadata#UploadService.UploadService";
        Node::json["Id"] = "UploadService";
        Node::json["Description"] = "Service for upload file to BMC";
        Node::json["Name"] = "Upload Service";
        Node::json["HttpPushUri"] = "/redfish/v1/AmpereComputing/UploadService";
        // TODO UploadService is always enabled
        Node::json["ServiceEnabled"] = true;

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

    void doPost(crow::Response &res, const crow::Request &req,
                const std::vector<std::string> &params) override
    {
        BMCWEB_LOG_DEBUG << "doPost...";
        // Only allow one file upload at a time
        if (fileUpload != true)
        {
            res.addHeader("Retry-After", "30");
            res.result(boost::beast::http::status::service_unavailable);
            res.jsonValue = messages::serviceTemporarilyUnavailable("3");
            res.end();
            return;
        }
        fileUpload = false;

        // Make this const static so it survives outside this method
        static boost::asio::deadline_timer timeout(
            *req.ioService, boost::posix_time::seconds(5));

        timeout.expires_from_now(boost::posix_time::seconds(5));

        timeout.async_wait([&res](const boost::system::error_code &ec) {
            fileUpload = true;
            if (ec == boost::asio::error::operation_aborted)
            {
                // expected, we were canceled before the timer completed.
                return;
            }
            BMCWEB_LOG_ERROR
                << "Timed out waiting for writing file to server";
            if (ec)
            {
                BMCWEB_LOG_ERROR << "Async_wait failed" << ec;
                return;
            }

            res.result(boost::beast::http::status::internal_server_error);
            res.jsonValue = redfish::messages::internalError();
            res.end();
        });

        // TODO Require D-Bus service to monitor /tmp/ folder
        // to trigger appropriate handling:
        // i.e. decode SMBIOS info, verify file integrity...
        std::string filepath(
            "/tmp/smbios/" +
            boost::uuids::to_string(boost::uuids::random_generator()()));
        BMCWEB_LOG_DEBUG << "Writing file to " << filepath;
        std::ofstream out(filepath, std::ofstream::out | std::ofstream::binary |
                                        std::ofstream::trunc);
        out << req.body;
        out.close();
        BMCWEB_LOG_DEBUG << "file upload complete!!";

        boost::system::error_code ec;
        timeout.cancel(ec);
        if (ec)
        {
            BMCWEB_LOG_ERROR << "error canceling timer " << ec;
            BMCWEB_LOG_ERROR << "File may has already been uploaded to server";
        }
        res.jsonValue = redfish::messages::success();
        res.end();
        fileUpload = true;
    }
};

} // namespace redfish
