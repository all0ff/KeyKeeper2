#pragma once

#include "esp_http_server.h"

namespace web {

void register_reorder_routes(httpd_handle_t server);

} // namespace web
