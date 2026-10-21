/* Hand-written replacements for individual lifted functions.
 * This is an ALLOWED place for fixes (see AGENTS.md §2).
 * Register each override in the manual lookup table so ICALLs find it.
 *
 * Pattern:
 *   void sub_001A3F50(void) {
 *       eax = 1;          // return success
 *       esp += 4; return; // clean up the fake return address
 *   }
 */
