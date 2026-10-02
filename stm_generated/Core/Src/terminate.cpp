#include <cstdlib>
// Replace libstdc++'s verbose terminate handler (pulls in ~32KB of C++ demangler)
namespace __gnu_cxx
{
void __verbose_terminate_handler()
{
  __builtin_trap();
}
} // namespace __gnu_cxx
