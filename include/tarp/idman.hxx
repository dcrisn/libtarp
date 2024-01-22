#pragma once

#include <cstdint>

namespace tarp {

class IdManager {
private:
    using idType = uint32_t;

public:
    IdManager(void);

    idType get_id(void);

private:
    idType m_next_id;
};

}
