#include "web_reorder_state.hpp"

#include "vault/vault_order.hpp"

namespace web::reorder {

bool init()
{
    return vault::order::init();
}

std::vector<uint32_t> get_order()
{
    return vault::order::get_order();
}

bool set_order(const std::vector<uint32_t>& ids)
{
    return vault::order::set_order(ids);
}

} // namespace web::reorder
