#include "bsp/board.hpp"

namespace bsp::board {

namespace {

constexpr Info BOARD_INFO{};

} // namespace

const Info& info()
{
    return BOARD_INFO;
}

} // namespace bsp::board
