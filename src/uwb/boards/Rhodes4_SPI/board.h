/*
 * Copyright 2019-2022,2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#define FSL_FEATURE_PINT_NUMBER_OF_CONNECTED_OUTPUTS (4)

typedef enum Rhodes4_revision {
  RHODES4_REV_B = 0,                 // With IO Expander
  RHODES4_REV_C = 1,                 // With IO Expander
  RHODES4_REV_B_NO_IO_EXPANDER = 2,  // Without IO Expander
} eRhodes4Revision;

#endif