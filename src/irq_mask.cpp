#include <jw/dpmi/irq_mask.h>

using namespace std;
namespace jw
{
    namespace dpmi
    {
        volatile int interrupt_mask::count { 0 };
        bool interrupt_mask::initial_state;

        std::array<irq_mask::mask_counter, 16> irq_mask::map { };
        constexpr io::io_port<byte> irq_mask::pic0_data;
        constexpr io::io_port<byte> irq_mask::pic1_data;

        //int nmi_mask::count { 0 };
    }
}
