#include <crow/app.h>
#include <systemd/sd-daemon.h>

#include <bmcweb/settings.hpp>
#include <boost/asio.hpp>
#include <dbus_monitor.hpp>
#include <dbus_singleton.hpp>
#include <image_upload.hpp>
#include <memory>
#include <openbmc_dbus_rest.hpp>
#include <persistent_data_middleware.hpp>
#include <redfish.hpp>
#include <redfish_v1.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <security_headers_middleware.hpp>
#include <ssl_key_handler.hpp>
#include <string>
#include <token_authorization_middleware.hpp>
#include <web_kvm.hpp>
#include <webassets.hpp>
#include <webserver_common.hpp>

constexpr int defaultPort = 18080;

template <typename... Middlewares>
void setupSocket(crow::Crow<Middlewares...>& app)
{
    int listenFd = sd_listen_fds(0);
    if (1 == listenFd)
    {
        BMCWEB_LOG_INFO << "attempting systemd socket activation";
        if (sd_is_socket_inet(SD_LISTEN_FDS_START, AF_UNSPEC, SOCK_STREAM, 1,
                              0))
        {
            BMCWEB_LOG_INFO << "Starting webserver on socket handle "
                            << SD_LISTEN_FDS_START;
            app.socket(SD_LISTEN_FDS_START);
        }
        else
        {
            BMCWEB_LOG_INFO
                << "bad incoming socket, starting webserver on port "
                << defaultPort;
            app.port(defaultPort);
        }
    }
    else
    {
        BMCWEB_LOG_INFO << "Starting webserver on port " << defaultPort;
        app.port(defaultPort);
    }
}

int main(int argc, char** argv)
{
    crow::logger::setLogLevel(crow::LogLevel::DEBUG);

    auto io = std::make_shared<boost::asio::io_service>();
    CrowApp app(io);

#ifdef BMCWEB_ENABLE_SSL
    std::string sslPemFile("server.pem");
    std::cout << "Building SSL Context\n";

    ensuressl::ensureOpensslKeyPresentAndValid(sslPemFile);
    std::cout << "SSL Enabled\n";
    auto sslContext = ensuressl::getSslContext(sslPemFile);
    app.ssl(std::move(sslContext));
#endif
    // Static assets need to be initialized before Authorization, because auth
    // needs to build the whitelist from the static routes

#ifdef BMCWEB_ENABLE_STATIC_HOSTING
    crow::webassets::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_KVM
    crow::kvm::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_REDFISH
    crow::redfish::requestRoutes(app);
#endif

#ifdef BMCWEB_ENABLE_DBUS_REST
    crow::dbus_monitor::requestRoutes(app);
    crow::image_upload::requestRoutes(app);
    crow::openbmc_mapper::requestRoutes(app);
#endif

    crow::token_authorization::requestRoutes(app);

    BMCWEB_LOG_INFO << "bmcweb (" << __DATE__ << ": " << __TIME__ << ')';
    setupSocket(app);

    crow::connections::systemBus =
        std::make_shared<sdbusplus::asio::connection>(*io);
    redfish::RedfishService redfish(app);

    app.run();
    io->run();

    crow::connections::systemBus.reset();
}
