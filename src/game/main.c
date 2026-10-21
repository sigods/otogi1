/* Host entry point for Otogi: Myth of Demons.
 * The template does the boot; you supply three constants.
 * The toolkit has no main() — this file is why an .exe exists. */

#define OTOGI_ENTRY_POINT   0x00000000 /* TODO: XBE entry point VA from Step 2 */
#define OTOGI_XBE_PATH      "game/default.xbe"
#define OTOGI_DIR           "game"

/* The template already, in order:
 *   install VEH crash handler
 *   load the XBE
 *   xbox_MemoryLayoutInit
 *   xbox_kernel_init
 *   xbox_path_init
 *   xbox_kernel_bridge_init
 *   g_esp = XBOX_STACK_TOP
 *   recomp_dispatch_init()
 *   xbe_entry_point()   <- named after OTOGI_ENTRY_POINT
 *
 * Do not add hand-written game logic here. Boot constants only.
 */
