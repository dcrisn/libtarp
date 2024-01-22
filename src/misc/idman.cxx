#include <tarp/idman.hxx>

using namespace tarp;

IdManager::IdManager(void)
    : m_next_id(0)
{
}

uint32_t IdManager::get_id(void){
   uint32_t id = m_next_id; 
   m_next_id++; /* unsigned; let it wrap around */
   return id;
}

