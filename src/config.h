/*
 * Copyright (c) 2016, Takashi TOYOSHIMA <toyoshim@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the authors nor the names of its contributors may be
 *   used to endorse or promote products derived from this software with out
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUE
 * NTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#if !defined(__config_h__)
# define __config_h__

#define TEST

# if defined(TEST)
#  define CPU_EMU_C
#if !defined(USE_FLASH)
#  define MON_SDC
//#  define CHK_SDC
#  define MON_FAT
#endif //!defined(USE_FLASH)
/* #  define MSG_MIN */
#  define CLR_MEM
#  define CHK_MEM
/* #  define CHK_MIN */
#  define MONITOR
#  define  MON_MEM
#  define  MON_CON
#  define  MON_HELP
# else // defined(TEST)
#  define CPU_EMU_A
/* #  define CPM_DEBUG */
/* #  define USE_FAT */
//#  define MSG_MIN
/* #  define CLR_MEM */
//#  define CHK_MEM
#  define CHK_MIN
/* #  define CHK_SDC */
#  define MONITOR
/* #  define  MON_MEM */
#if !defined(USE_FLASH)
#  define  MON_SDC
#  define  MON_FAT
#endif //!defined(USE_FLASH)
#  define  MON_CON
#  define  MON_HELP
#  define IOEXT
# endif // defined(TEST)

# define MAX_PROMPT 16

# if !defined(NULL)
#  define NULL ((void*)0)
# endif // !defined(NULL)

#endif // !defined(__config_h__)
