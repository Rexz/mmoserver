// This file is part of SWGANH which is released under the MIT license.
// See file LICENSE or go to http://swganh.com/LICENSE
#pragma once

#include <cstdint>
#include "anh/byte_buffer.h"
#include "base_swg_message.h"

namespace swganh
{
namespace messages
{

struct CmdSceneReady : public BaseSwgMessage
{
    uint16_t Opcount() const
    {
        return 1;
    }
    uint32_t Opcode() const
    {
        return 0x43FD1C22;
    }

    void OnSerialize(swganh::ByteBuffer& buffer) const {}

    void OnDeserialize(swganh::ByteBuffer& buffer) {}
};

}
}  // namespace swganh::messages
